// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Creates and removes the editor pages, mirrors tab order and flags into DocumentList.

#include "ui/document_tabs.h"

#include <gtksourceview/gtksource.h>

#include <algorithm>
#include <utility>

#include "ui/tab_label.h"

namespace vermilion::ui {

namespace {

constexpr unsigned kTabWidth = 4; // per document; the settings value arrives with M2.5

} // namespace

struct DocumentTabs::Page {
    DocumentId id = 0;
    GtkWidget* scrolled = nullptr;     // borrowed; the notebook owns the page
    GtkSourceView* view = nullptr;     // borrowed; owned by scrolled
    GtkSourceBuffer* buffer = nullptr; // borrowed; the view holds its reference
    std::unique_ptr<TabLabel> label;
    gtkx::SignalConnection modified_changed;
    gtkx::SignalConnection cursor_moved;
    gtkx::SignalConnection overwrite_toggled;
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
    auto page = std::make_unique<Page>();
    page->id = list_.add_untitled();
    // Full reference from _new (not floating); the view takes its own, ours drops at scope end.
    auto buffer = gtkx::ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    GtkSourceStyleScheme* scheme = gtk_source_style_scheme_manager_get_scheme(
        gtk_source_style_scheme_manager_get_default(), "classic"); // borrowed
    if (scheme != nullptr) {
        gtk_source_buffer_set_style_scheme(buffer.get(), scheme);
    }
    // Both widgets are floating: the scrolled window sinks the view, the notebook the window.
    GtkWidget* view = gtk_source_view_new_with_buffer(buffer.get());
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(view), TRUE);
    gtk_source_view_set_tab_width(GTK_SOURCE_VIEW(view), kTabWidth);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(scrolled, "vm-editor-well");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), view);
    page->scrolled = scrolled;
    page->view = GTK_SOURCE_VIEW(view);
    page->buffer = buffer.get();
    Page& added = *page;
    attach_signals(added);
    refresh_label(added);
    // In the list before it is appended: appending the first page announces it as current.
    pages_.push_back(std::move(page));
    int index = gtk_notebook_append_page(notebook(), scrolled, added.label->widget());
    gtk_notebook_set_tab_reorderable(notebook(), scrolled, TRUE);
    gtk_notebook_set_current_page(notebook(), index);
    callbacks_.changed();
}

void DocumentTabs::close(DocumentId id) {
    auto it = std::find_if(pages_.begin(), pages_.end(),
                           [id](const std::unique_ptr<Page>& page) { return page->id == id; });
    if (it == pages_.end()) {
        return;
    }
    int index = gtk_notebook_page_num(notebook(), (*it)->scrolled);
    std::optional<DocumentId> restore;
    if (close_press_ && close_press_->closing == id) {
        restore = close_press_->active_before;
    }
    close_press_.reset();
    // The page's connections and label go first, then the notebook page itself.
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

std::optional<CursorStatus> DocumentTabs::active_status() const {
    std::optional<DocumentId> active = list_.active();
    Page* page = active ? find_page(*active) : nullptr;
    if (page == nullptr) {
        return std::nullopt;
    }
    GtkTextBuffer* text = GTK_TEXT_BUFFER(page->buffer);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(text, &iter, gtk_text_buffer_get_insert(text));
    CursorStatus status;
    status.line = gtk_text_iter_get_line(&iter) + 1;
    status.column = static_cast<int>(gtk_source_view_get_visual_column(page->view, &iter)) + 1;
    status.overwrite = gtk_text_view_get_overwrite(GTK_TEXT_VIEW(page->view)) != FALSE;
    status.tab_width = gtk_source_view_get_tab_width(page->view);
    return status;
}

GtkWidget* DocumentTabs::active_view() const {
    std::optional<DocumentId> active = list_.active();
    Page* page = active ? find_page(*active) : nullptr;
    return page != nullptr ? GTK_WIDGET(page->view) : nullptr;
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
        if (page->scrolled == widget) {
            return page.get();
        }
    }
    return nullptr;
}

void DocumentTabs::attach_signals(Page& page) {
    DocumentId id = page.id;
    Page* target = &page; // stays valid: the connections are members of the page
    page.label = std::make_unique<TabLabel>(TabLabel::Callbacks{
        [this, id] { request_close(id); }, [this](int step) { select_relative(step, false); }});
    page.modified_changed = gtkx::connect<void(GtkTextBuffer*)>(
        page.buffer, "modified-changed", [this, target](GtkTextBuffer* buffer) {
            list_.set_modified(target->id, gtk_text_buffer_get_modified(buffer) != FALSE);
            refresh_label(*target);
            callbacks_.changed();
        });
    page.cursor_moved = gtkx::connect<void(GObject*, GParamSpec*)>(
        page.buffer, "notify::cursor-position",
        [this, id](GObject*, GParamSpec*) { notify_if_active(id); });
    page.overwrite_toggled = gtkx::connect<void(GObject*, GParamSpec*)>(
        page.view, "notify::overwrite",
        [this, id](GObject*, GParamSpec*) { notify_if_active(id); });
}

void DocumentTabs::on_press(double x, double y) {
    close_press_.reset();
    std::optional<DocumentId> active = list_.active();
    GtkWidget* target = gtk_widget_pick(box_.get(), x, y, GTK_PICK_DEFAULT); // borrowed
    for (const std::unique_ptr<Page>& page : pages_) {
        if (page->label->contains_close_button(target)) {
            if (active && *active != page->id) {
                close_press_ = ClosePress{page->id, *active};
            }
            return;
        }
    }
}

void DocumentTabs::refresh_label(Page& page) {
    const DocumentRecord* record = list_.find(page.id);
    if (record != nullptr) {
        page.label->set(record->title, record->modified);
    }
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
    gtk_widget_grab_focus(GTK_WIDGET(page->view));
    callbacks_.changed();
}

void DocumentTabs::select_active() {
    std::optional<DocumentId> active = list_.active();
    Page* page = active ? find_page(*active) : nullptr;
    if (page == nullptr) {
        return;
    }
    int index = gtk_notebook_page_num(notebook(), page->scrolled);
    if (index >= 0) {
        gtk_notebook_set_current_page(notebook(), index);
    }
    gtk_widget_grab_focus(GTK_WIDGET(page->view));
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
