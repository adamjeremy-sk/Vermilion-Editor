// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Adds, loads and removes the editor pages, mirrors tab order and flags into DocumentList.

#include "ui/document_tabs.h"

#include <algorithm>
#include <string>
#include <utility>

#include "core/log.h"
#include "core/open_messages.h"
#include "core/recovery_store.h"
#include "core/save_messages.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

struct DocumentTabs::Page {
    Page(DocumentId document, EditorPage::Callbacks callbacks)
        : id(document), editor(std::move(callbacks)) {}

    DocumentId id;
    EditorPage editor;
};

// Floating box and notebook from _new; adopt_floating sinks them, so this object keeps them alive
// until its own connections and pages are gone (members are destroyed in reverse order).
DocumentTabs::DocumentTabs(DocumentList& list, Callbacks callbacks)
    : list_(list), callbacks_(std::move(callbacks)),
      box_(gtkx::ObjectPtr<GtkWidget>::adopt_floating(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0))),
      notebook_(gtkx::ObjectPtr<GtkWidget>::adopt_floating(gtk_notebook_new())) {
    gtk_notebook_set_scrollable(notebook(), TRUE);
    gtk_widget_set_vexpand(notebook_.get(), TRUE);
    gtk_box_append(GTK_BOX(box_.get()), notebook_.get());
    page_changed_ = gtkx::connect<void(GObject*, GParamSpec*)>(
        notebook_.get(), "notify::page", [this](GObject*, GParamSpec*) { on_page_changed(); });
    reordered_ = gtkx::connect<void(GtkNotebook*, GtkWidget*, guint)>(
        notebook_.get(), "page-reordered", [this](GtkNotebook*, GtkWidget* child, guint index) {
            Page* page = find_page_by_widget(child);
            if (page != nullptr) {
                list_.move(page->id, index);
            }
        });
    // Capture phase on the box runs before the notebook's own press handling.
    // gtk_widget_add_controller takes the full reference that _new returned (transfer full).
    GtkGesture* press = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(press), GDK_BUTTON_PRIMARY);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(press), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(box_.get(), GTK_EVENT_CONTROLLER(press));
    pressed_ = gtkx::connect<void(GtkGestureClick*, int, double, double)>(
        press, "pressed", [this](GtkGestureClick*, int, double x, double y) { on_press(x, y); });
}

DocumentTabs::~DocumentTabs() {
    if (idle_id_ != 0) {
        g_source_remove(idle_id_);
    }
}

void DocumentTabs::add_untitled() {
    create_page(list_.add_untitled());
    callbacks_.changed();
}

void DocumentTabs::add_file(const std::string& path, const std::string& title,
                            const ScanResult& scan) {
    const DocumentRecord* open = list_.find_by_path(path);
    if (open != nullptr) {
        select(open->id);
        return;
    }
    DocumentId id = list_.add_file(path, title, scan.format, scan.mixed_line_endings);
    if (scan.binary) {
        list_.set_read_only(id, true);
    }
    create_page(id).editor.load(path, scan.format.encoding);
    callbacks_.changed();
}

// The text goes in with one insert, which is timed in the log: the cost on a big document is what
// the last step of M2.2c measures.
DocumentId DocumentTabs::add_recovered(Recovered recovered) {
    static_assert(kRecoveryMaxFileBytes < (std::uint64_t{1} << 31),
                  "the buffer takes the text in one call, with an int length");
    bool as_file = recovered.path && list_.find_by_path(*recovered.path) == nullptr;
    DocumentId id = 0;
    if (as_file) {
        std::string title = gtkx::take_string(g_filename_display_basename(recovered.path->c_str()));
        id = list_.add_file(*recovered.path, title, recovered.format, recovered.mixed_line_endings);
    } else {
        id = list_.add_untitled();
        list_.set_format(id, recovered.format, recovered.mixed_line_endings);
    }
    Page& page = create_page(id);
    gint64 started = g_get_monotonic_time();
    page.editor.restore_text(recovered.text);
    gint64 elapsed_ms = (g_get_monotonic_time() - started) / 1000;
    logging::debug("recovery: " + std::to_string(page.editor.char_count()) +
                   " characters inserted in " + std::to_string(elapsed_ms) + " ms");
    if (as_file) {
        page.editor.guess_language(*recovered.path);
        if (recovered.file_mtime_us) {
            page.editor.expect_file_time(*recovered.path, *recovered.file_mtime_us);
        }
    }
    callbacks_.changed();
    return id;
}

void DocumentTabs::reopen(DocumentId id, TextEncoding encoding) {
    Page* page = find_page(id);
    const DocumentRecord* record = list_.find(id);
    if (page == nullptr || record == nullptr || record->path.empty() || page->editor.busy()) {
        return;
    }
    // Read-only again only if this encoding also leaves bytes that cannot be shown (on_loaded).
    list_.set_read_only(id, false);
    if (observer_ != nullptr) {
        observer_->document_reloading(id);
    }
    page->editor.load(record->path, encoding);
    callbacks_.changed();
}

void DocumentTabs::close(DocumentId id) {
    auto it = std::find_if(pages_.begin(), pages_.end(),
                           [id](const std::unique_ptr<Page>& page) { return page->id == id; });
    if (it == pages_.end()) {
        return;
    }
    if (observer_ != nullptr) {
        observer_->document_closing(id);
    }
    int index = gtk_notebook_page_num(notebook(), (*it)->editor.widget());
    std::optional<DocumentId> restore;
    if (close_press_ && close_press_->closing == id) {
        restore = close_press_->active_before;
    }
    close_press_.reset();
    // The page's load, connections and label go first, then the notebook page itself.
    pages_.erase(it);
    list_.close(id);
    if (restore) {
        // A background tab was closed: its document stays active (false if it is gone).
        list_.set_active(*restore);
    }
    if (index >= 0) {
        // GTK picks a new current page on removal; DocumentList's neighbour rule decides instead.
        removing_ = true;
        gtk_notebook_remove_page(notebook(), index);
        removing_ = false;
    }
    select_active();
    callbacks_.changed();
}

void DocumentTabs::select_next() {
    select_relative(1, true);
}

void DocumentTabs::select_previous() {
    select_relative(-1, true);
}

void DocumentTabs::select(DocumentId id) {
    if (list_.set_active(id)) {
        select_active();
        callbacks_.changed();
    }
}

std::optional<CursorStatus> DocumentTabs::active_status() const {
    std::optional<DocumentId> active = list_.active();
    Page* page = active ? find_page(*active) : nullptr;
    const DocumentRecord* record = active ? list_.find(*active) : nullptr;
    if (page == nullptr || record == nullptr) {
        return std::nullopt;
    }
    CursorStatus status = page->editor.status();
    status.encoding = encoding_label(record->format);
    status.line_ending =
        record->mixed_line_endings ? "Mixed" : line_ending_label(record->format.line_ending);
    return status;
}

void DocumentTabs::refresh_tab(DocumentId id) {
    Page* page = find_page(id);
    if (page != nullptr) {
        refresh_label(*page);
    }
}

EditorPage* DocumentTabs::editor(DocumentId id) const {
    Page* page = find_page(id);
    return page != nullptr ? &page->editor : nullptr;
}

EditorPage* DocumentTabs::active_editor() const {
    std::optional<DocumentId> active = list_.active();
    Page* page = active ? find_page(*active) : nullptr;
    return page != nullptr ? &page->editor : nullptr;
}

GtkNotebook* DocumentTabs::notebook() const {
    return GTK_NOTEBOOK(notebook_.get());
}

DocumentTabs::Page* DocumentTabs::find_page(DocumentId id) const {
    for (const std::unique_ptr<Page>& page : pages_) {
        if (page->id == id) {
            return page.get();
        }
    }
    return nullptr;
}

DocumentTabs::Page* DocumentTabs::find_page_by_widget(GtkWidget* widget) const {
    for (const std::unique_ptr<Page>& page : pages_) {
        if (page->editor.widget() == widget) {
            return page.get();
        }
    }
    return nullptr;
}

// Builds the editor for a record DocumentList has just added (and made active), appends its tab
// and selects it.
DocumentTabs::Page& DocumentTabs::create_page(DocumentId id) {
    auto page = std::make_unique<Page>(
        id,
        EditorPage::Callbacks{
            [this, id] { request_close(id); }, [this](int step) { select_relative(step, false); },
            [this, id](bool modified) { on_modified(id, modified); },
            [this, id] { notify_if_active(id); },
            [this, id](int percent) { on_load_progress(id, percent); },
            [this, id](DocumentLoader::Outcome outcome, const std::string& error) {
                on_loaded(id, outcome, error);
            },
            [this, id](int percent) { on_save_progress(id, percent); },
            [this, id](DocumentSaver::Outcome outcome, const std::string& error) {
                on_saved(id, outcome, error);
            },
            [this, id] { on_edited(id); }});
    Page& added = *page;
    refresh_label(added);
    // In the list before it is appended: appending the first page announces it as current.
    pages_.push_back(std::move(page));
    GtkWidget* widget = added.editor.widget();
    int index = gtk_notebook_append_page(notebook(), widget, added.editor.tab());
    gtk_notebook_set_tab_reorderable(notebook(), widget, TRUE);
    gtk_notebook_set_current_page(notebook(), index);
    return added;
}

void DocumentTabs::on_modified(DocumentId id, bool modified) {
    Page* page = find_page(id);
    if (page == nullptr) {
        return;
    }
    list_.set_modified(id, modified);
    if (observer_ != nullptr) {
        observer_->document_modified(id, modified);
    }
    refresh_label(*page);
    callbacks_.changed();
}

void DocumentTabs::on_load_progress(DocumentId id, int percent) {
    const DocumentRecord* record = list_.find(id);
    if (record != nullptr) {
        callbacks_.message(loading_message(record->title, percent));
    }
}

// A failed load closes its tab; a load with bytes that cannot be shown leaves it read-only.
void DocumentTabs::on_loaded(DocumentId id, DocumentLoader::Outcome outcome,
                             const std::string& error) {
    Page* page = find_page(id);
    const DocumentRecord* record = list_.find(id);
    if (page == nullptr || record == nullptr) {
        return;
    }
    if (outcome == DocumentLoader::Outcome::Failed) {
        std::string title = record->title;
        close(id);
        callbacks_.message("Could not open " + quoted(title) + ".");
        callbacks_.load_failed("Could not open " + quoted(title), error);
        return;
    }
    if (outcome == DocumentLoader::Outcome::LoadedWithInvalidBytes) {
        list_.set_read_only(id, true);
    }
    // The byte-order mark is whatever the loaded text starts with (also after Reopen as).
    TextFormat format = record->format;
    format.bom = page->editor.starts_with_bom();
    list_.set_format(id, format, record->mixed_line_endings);
    page->editor.finish_load(record->path, record->read_only);
    list_.set_modified(id, false);
    refresh_label(*page);
    sync_file_time(id);
    callbacks_.message(loaded_message(*record));
    callbacks_.changed();
}

void DocumentTabs::on_save_progress(DocumentId id, int percent) {
    const DocumentRecord* record = list_.find(id);
    if (record != nullptr) {
        callbacks_.message(saving_message(record->title, percent));
    }
}

// The length is read here, once per edit: the buffer keeps its character count.
void DocumentTabs::on_edited(DocumentId id) {
    Page* page = find_page(id);
    if (page != nullptr && observer_ != nullptr) {
        observer_->document_edited(id, page->editor.char_count());
    }
}

// Runs inside the page's own save callback, so the page still exists. The receiver of the save is
// told last.
void DocumentTabs::on_saved(DocumentId id, DocumentSaver::Outcome outcome,
                            const std::string& error) {
    if (outcome == DocumentSaver::Outcome::Saved) {
        if (observer_ != nullptr) {
            observer_->document_saved(id);
        }
        sync_file_time(id);
    }
    callbacks_.saved(id, outcome, error);
}

// Asks the page for its file's modified time (a load or a save has just made the file and the
// text equal); the answer goes to the observer when it arrives, unless the page is gone by then.
void DocumentTabs::sync_file_time(DocumentId id) {
    Page* page = find_page(id);
    if (page == nullptr) {
        return;
    }
    page->editor.query_file_time([this, id](std::optional<std::int64_t> mtime_us) {
        if (observer_ != nullptr) {
            observer_->document_synced(id, mtime_us);
        }
    });
}

void DocumentTabs::on_press(double x, double y) {
    close_press_.reset();
    std::optional<DocumentId> active = list_.active();
    GtkWidget* target = gtk_widget_pick(box_.get(), x, y, GTK_PICK_DEFAULT); // borrowed
    for (const std::unique_ptr<Page>& page : pages_) {
        if (page->editor.contains_close_button(target)) {
            if (active && *active != page->id) {
                close_press_ = ClosePress{page->id, *active};
            }
            return;
        }
    }
}

// The tooltip is the file's path for display (on disk it may not be UTF-8), or the title.
void DocumentTabs::refresh_label(Page& page) {
    const DocumentRecord* record = list_.find(page.id);
    if (record == nullptr) {
        return;
    }
    std::string tooltip = record->path.empty()
                              ? record->title
                              : gtkx::take_string(g_filename_display_name(record->path.c_str()));
    page.editor.set_label(record->title, tooltip, record->modified);
}

void DocumentTabs::notify_if_active(DocumentId id) {
    if (list_.active() == id) {
        callbacks_.changed();
    }
}

void DocumentTabs::on_page_changed() {
    if (removing_) {
        return;
    }
    int index = gtk_notebook_get_current_page(notebook());
    Page* page =
        index >= 0 ? find_page_by_widget(gtk_notebook_get_nth_page(notebook(), index)) : nullptr;
    if (page == nullptr) {
        return;
    }
    list_.set_active(page->id);
    gtk_widget_grab_focus(page->editor.view());
    callbacks_.changed();
}

void DocumentTabs::select_active() {
    std::optional<DocumentId> active = list_.active();
    Page* page = active ? find_page(*active) : nullptr;
    if (page == nullptr) {
        return;
    }
    int index = gtk_notebook_page_num(notebook(), page->editor.widget());
    if (index >= 0) {
        gtk_notebook_set_current_page(notebook(), index);
    }
    gtk_widget_grab_focus(page->editor.view());
}

void DocumentTabs::select_relative(int step, bool wrap) {
    int count = gtk_notebook_get_n_pages(notebook());
    int index = gtk_notebook_get_current_page(notebook());
    if (count == 0 || index < 0) {
        return;
    }
    int next = index + step;
    if (wrap) {
        next = (next % count + count) % count;
    } else if (next < 0 || next >= count) {
        return;
    }
    gtk_notebook_set_current_page(notebook(), next);
}

void DocumentTabs::request_close(DocumentId id) {
    // Closing inside the click would destroy the tab label while its own event is delivered.
    if (std::find(pending_closes_.begin(), pending_closes_.end(), id) == pending_closes_.end()) {
        pending_closes_.push_back(id);
    }
    if (idle_id_ == 0) {
        idle_id_ = g_idle_add_once(&DocumentTabs::run_pending_closes, this);
    }
}

void DocumentTabs::run_pending_closes(gpointer data) {
    auto* self = static_cast<DocumentTabs*>(data);
    self->idle_id_ = 0;
    std::vector<DocumentId> ids;
    ids.swap(self->pending_closes_);
    for (DocumentId id : ids) {
        self->callbacks_.close_requested(id);
    }
}

} // namespace vermilion::ui
