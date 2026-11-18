// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Looks at a document's file when it may have changed, and applies the policy of external_change.h.

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "core/baseline.h"
#include "core/document_list.h"
#include "core/external_change.h"
#include "core/file_stamp.h"
#include "core/text_scan.h"
#include "ui/debounce_timer.h"
#include "ui/file_inspector.h"
#include "ui/file_probe.h"
#include "ui/file_watcher.h"
#include "ui/gtkx/async_guard.h"

namespace vermilion::ui {

// What the controller needs from the document pages and the window, by document id. The window's
// implementation reaches the pages; the tests use a recording fake. Every call comes from the main
// loop. A page can disappear at any time: the controller checks page_state() after every wait.
class ExternalChangeHost {
public:
    virtual ~ExternalChangeHost() = default;

    struct PageState {
        // The page is not loading, saving, locked or recording its baseline: its text and its
        // baseline can be trusted. A look at the file waits until this is true.
        bool quiet = false;
        // The file's path and stamp as they were when the text and the file were last equal.
        std::optional<Baseline> baseline;
    };

    // Nothing for an id that has no page.
    virtual std::optional<PageState> page_state(DocumentId id) const = 0;

    // The page's baseline becomes this stamp (its path stays).
    virtual void set_baseline(DocumentId id, const FileStamp& stamp) = 0;

    // Shows the banner on the document's page with these texts and answers, replacing a banner
    // that is already shown. The answers may be called later, from the banner's buttons.
    virtual void show_banner(DocumentId id, const std::string& text, const std::string& primary,
                             const std::string& secondary, std::function<void()> on_primary,
                             std::function<void()> on_secondary) = 0;

    // Hides the banner; does nothing when none is shown.
    virtual void hide_banner(DocumentId id) = 0;

    // Starts reloading the document from its file with this scan of the new content (the
    // document's own encoding rule is the host's business); `message` is the status line to show
    // when it has loaded. True when it started, false when the page cannot reload now.
    virtual bool reload(DocumentId id, const ScanResult& scan, const std::string& message) = 0;

    // The document's text now counts as edited (its modified flag turns on).
    virtual void mark_modified(DocumentId id) = 0;

    // The document was just marked deleted_on_disk in the list: refresh its tab label and tell
    // crash recovery.
    virtual void deleted_on_disk(DocumentId id) = 0;

    // Asks the open-time question again before a reload (a very long line, a NUL byte); `go`
    // runs only when the user chooses to reload anyway.
    virtual void ask_reload_anyway(const std::string& message, const std::string& detail,
                                   std::function<void()> go) = 0;

    // File > Reload from Disk asks before it replaces the text: shows a question with two buttons,
    // `go_label` and Cancel (Cancel is the default). With `offer_dont_ask` the question also has
    // a check box labelled dont_ask_again_label(). `go` runs only when the user chooses
    // `go_label`, with the state of the check box (false when none is offered); it may run long
    // after this call, and never when the user cancels. The default shows nothing and never calls
    // `go`: a host that cannot ask never starts a reload.
    virtual void ask_reload(const std::string& /*message*/, const std::string& /*detail*/,
                            const std::string& /*go_label*/, bool /*offer_dont_ask*/,
                            std::function<void(bool)> /*go*/) {}

    // The setting "ask before reloading a large file" ([files] confirm-large-reload). The default
    // is true.
    virtual bool confirm_large_reload() const { return true; }

    // Turns that setting on or off and saves the settings file; a save that fails shows a line on
    // the status bar (the change then holds until the program ends). The default does nothing.
    virtual void set_confirm_large_reload(bool /*confirm*/) {}

    // A line for the status bar.
    virtual void message(const std::string& text) = 0;
};

// Watches the active document's file, and checks a document's file on demand, then applies the
// decision of core/external_change.h: nothing, a recorded touch, a silent reload, a banner or a
// deleted mark. It owns no widget: everything it does to a page goes through the host. A check
// reads only what it needs (time and size first, the bytes only when those differ), off the main
// loop, and drops its answer when the document or its baseline changed meanwhile.
class ExternalChangeController {
public:
    // list and host are borrowed and must outlive this object. `timer` is the watcher's debounce
    // timer (a GlibTimer in the program).
    ExternalChangeController(DocumentList& list, ExternalChangeHost& host,
                             std::unique_ptr<DebounceTimer> timer,
                             unsigned delay_ms = FileWatcher::kDefaultDelayMs);

    // Stops watching, cancels the probes under way and drops every answer still to come.
    ~ExternalChangeController();

    ExternalChangeController(const ExternalChangeController&) = delete;
    ExternalChangeController& operator=(const ExternalChangeController&) = delete;

    // Call after every change of the documents or the active tab. Watches the active document's
    // file (nothing for an untitled document or no document); when the active document is another
    // one than at the last call, it is checked at once (the tab switch).
    void follow_active();

    // Looks at the file of document id now. Does nothing for an unknown document, an untitled one,
    // or a page without a baseline. While the page is not quiet, or a check of this document is
    // under way, the check is remembered and runs when settled() or follow_active() is next
    // called for it, or when the running check ends.
    void check(DocumentId id);

    // The answer of look(): what the document's file is, compared with the document's baseline.
    using LookDone = std::function<void(FileLook)>;

    // A read-only look at the document's file, for the question before its tab closes and at
    // quit: it changes nothing except that a missing file is marked deleted_on_disk (as check()
    // does: the list flag, the host's hide_banner() and deleted_on_disk(), and the status line). No
    // banner, no reload, no new baseline. Independent of check() and of any check under way for
    // the same document. `done` is called exactly once with the answer, from the main loop, unless
    // the controller is destroyed first (then never); it may run before look() returns when
    // nothing can be looked at. The answer is Unknown for an unknown or untitled document, a page
    // that is not quiet or has no baseline for the document's path, a file that cannot be read, or
    // a document, path or baseline that changed while the look was under way.
    void look(DocumentId id, LookDone done);

    // File > Reload from Disk: reloads the document from its file because the user asked, even
    // when the file did not change. Does nothing for an unknown document, an untitled one or one
    // without a page. While the page is not quiet or a check or reload of this document is under
    // way it only says so on the status line (reload_busy_message). Otherwise it reads the size of
    // the file: unreadable gives reload_failed_message, missing gives reload_missing_message (the
    // document is not marked deleted here). Then reload_ask() decides whether to ask first (the
    // host's ask_reload()); an answer that comes after the controller is gone is ignored. The
    // reload itself is the banner's Reload: the file is scanned off the main loop, a NUL byte or a
    // very long line asks the open-time question again, and the status line says
    // reloaded_by_hand_message. A tick in the "do not ask again" box of the large-file question
    // turns the setting off (set_confirm_large_reload(false)) when the user goes on.
    void reload_from_disk(DocumentId id);

    // The page of id may have become quiet (a load or a baseline read ended, a lock ended): runs
    // the check that was waiting for it, if any.
    void settled(DocumentId id);

    // The path being watched; empty when none.
    const std::string& watched_path() const { return watcher_.path(); }

    // True while any check or reload scan is under way.
    bool busy() const { return !running_.empty(); }

private:
    void on_look();
    void on_stat(DocumentId id, const std::string& path, const FileInspection& seen);
    void on_hashed(DocumentId id, const std::string& path, const FileStamp& used,
                   const FileInspection& seen);
    void conclude(DocumentId id, const FileStamp& stamp, const FileInspection& seen);
    void finish(DocumentId id);
    void show_banner(DocumentId id, BannerKind kind);
    void start_scan(DocumentId id, bool silent);
    void on_scanned(DocumentId id, bool silent, const FileProbe::Result& result);
    void do_reload(DocumentId id, const ScanResult& scan, bool by_hand);
    void on_banner_reload(DocumentId id);
    void on_banner_secondary(DocumentId id, BannerKind kind);
    void on_looked(DocumentId id, const std::string& path, const FileStamp& used, bool hashed,
                   const FileInspection& seen, LookDone done);
    void on_reload_inspected(DocumentId id, const std::string& path, const FileInspection& seen);
    void reload_confirmed(DocumentId id, bool stop_asking);

    DocumentList& list_;
    ExternalChangeHost& host_;
    FileWatcher watcher_;
    DocumentId watched_ = 0;       // the document the watcher follows, 0 for none
    std::set<DocumentId> wanted_;  // checks that wait for a quiet page
    std::set<DocumentId> running_; // documents with a check, scan or answer under way
    std::set<DocumentId> by_hand_; // documents whose scan under way is a File > Reload from Disk
    std::map<DocumentId, std::unique_ptr<FileProbe>> probes_;
    gtkx::AsyncGuard<ExternalChangeController> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
