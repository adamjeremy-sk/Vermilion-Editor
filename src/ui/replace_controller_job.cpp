// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Replace All job of the replace controller: slices on the main loop, Cancel and the end.

#include "ui/replace_controller.h"

#include <glib.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "core/log.h"
#include "core/search_messages.h"
#include "core/search_replacement.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

namespace {

// The value of `key` in /proc/self/status, in kB; 0 when the file cannot be read, the key is
// missing or no digit follows it.
std::int64_t memory_kb(const std::string& key) {
    gchar* raw = nullptr;
    if (!g_file_get_contents("/proc/self/status", &raw, nullptr, nullptr)) {
        return 0;
    }
    const std::string text = gtkx::take_string(raw);
    const std::size_t at = text.find("\n" + key + ":");
    if (at == std::string::npos) {
        return 0;
    }
    std::size_t i = at + key.size() + 2;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
        ++i;
    }
    std::int64_t value = 0;
    bool any = false;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        value = value * 10 + (text[i] - '0');
        ++i;
        any = true;
    }
    return any ? value : 0;
}

const char* end_name(ReplaceJobEnd end) {
    switch (end) {
    case ReplaceJobEnd::Finished:
        return "finished";
    case ReplaceJobEnd::Cancelled:
        return "cancelled";
    case ReplaceJobEnd::BufferChanged:
        return "buffer-changed";
    }
    return "finished";
}

} // namespace

ReplaceStart ReplaceController::start_replace_all() {
    if (job_.has_value()) {
        return ReplaceStart::Busy;
    }
    search_.apply();
    SearchSession* session = search_.session();
    if (session == nullptr) {
        return ReplaceStart::NothingToReplace;
    }
    if (session->error().has_value() ||
        replacement_error(search_.options(), replacement_).has_value()) {
        return ReplaceStart::Refused;
    }
    GtkTextBuffer* buffer = text_buffer();
    const int characters = gtk_text_buffer_get_char_count(buffer);
    int range_from = 0;
    int range_to = 0;
    bool in_range = false;
    if (scope() == ReplaceScope::SelectionOnly) {
        in_range = range_offsets(&range_from, &range_to);
    }
    const int from = in_range ? range_from : 0;
    const int to = in_range ? range_to : characters;
    if (!session->begin_replace_run(replacement_, from, to)) {
        search_.show_message(replaced_message(0));
        return ReplaceStart::NothingToReplace;
    }
    Job job;
    job.session = session;
    job.in_range = in_range;
    job.range_characters = to - from;
    job.characters = characters;
    const int count = session->count();
    job.total = (!in_range && count > 0) ? static_cast<std::uint64_t>(count) : 0;
    job.started_us = g_get_monotonic_time();
    job.rss_start_kb = memory_kb("VmRSS");
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
    GtkTextMark* mark = gtk_text_buffer_create_mark(buffer, nullptr, &iter, TRUE);
    job.cursor = gtkx::ObjectPtr<GtkTextMark>::ref(mark);
    job_ = std::move(job);
    search_.set_status_frozen(true);
    schedule_job_slice();
    return ReplaceStart::Started;
}

void ReplaceController::schedule_job_slice() {
    timer_->start(std::max(pacing_.gap_ms, 1u), [this]() { run_job_slice(); });
}

void ReplaceController::run_job_slice() {
    if (!job_.has_value()) {
        return;
    }
    const std::int64_t begin = g_get_monotonic_time();
    std::int64_t deadline = 0;
    if (pacing_.slice_ms != 0) {
        deadline = begin + static_cast<std::int64_t>(pacing_.slice_ms) * 1000;
    }
    const ReplaceSlice slice = job_->session->replace_run_slice(pacing_.max_matches, deadline);
    const std::int64_t spent = g_get_monotonic_time() - begin;
    job_->replaced += slice.replaced;
    job_->work_us += spent;
    ++job_->slices;
    logging::debug("replace slice: n=" + std::to_string(slice.replaced) + " ms=" +
                   std::to_string(spent / 1000) + " done=" + std::to_string(job_->replaced));
    if (slice.finished) {
        finish_job(ReplaceJobEnd::Finished, true);
        return;
    }
    schedule_job_slice();
    if (on_job_progress_) {
        std::function<void()> progress = on_job_progress_;
        progress();
    }
}

void ReplaceController::log_job_end(ReplaceJobEnd end, std::uint64_t count) const {
    std::string scope_text = "scope=whole";
    if (job_->in_range) {
        scope_text = "scope=selection range=" + std::to_string(job_->range_characters);
    }
    logging::debug("replace run: matches=" + std::to_string(count) + " end=" + end_name(end) +
                   " ms=" + std::to_string((g_get_monotonic_time() - job_->started_us) / 1000) +
                   " work_ms=" + std::to_string(job_->work_us / 1000) +
                   " slices=" + std::to_string(job_->slices) + " " + scope_text +
                   " characters=" + std::to_string(job_->characters) +
                   " rss_start_kb=" + std::to_string(job_->rss_start_kb) +
                   " rss_kb=" + std::to_string(memory_kb("VmRSS")) +
                   " peak_kb=" + std::to_string(memory_kb("VmHWM")));
}

void ReplaceController::finish_job(ReplaceJobEnd end, bool notify) {
    timer_->stop();
    const ReplaceAllResult run = job_->session->end_replace_run();
    GtkTextMark* mark = job_->cursor.get();
    GtkTextBuffer* owner = mark == nullptr ? nullptr : gtk_text_mark_get_buffer(mark);
    if (owner != nullptr) {
        if (!job_->in_range && run.count > 0) {
            GtkTextIter iter;
            gtk_text_buffer_get_iter_at_mark(owner, &iter, mark);
            gtk_text_buffer_place_cursor(owner, &iter);
        }
        gtk_text_buffer_delete_mark(owner, mark);
    }
    log_job_end(end, run.count);
    ReplaceJobResult result;
    result.end = end;
    result.count = run.count;
    result.in_selection = job_->in_range;
    const bool in_range = job_->in_range;
    job_.reset();
    if (end != ReplaceJobEnd::BufferChanged) {
        std::string message = end == ReplaceJobEnd::Finished ? replaced_message(run.count)
                                                             : replace_stopped_message(run.count);
        if (in_range) {
            message += " in the selection";
        }
        search_.show_message(message);
    }
    search_.set_status_frozen(false);
    if (notify && on_job_done_) {
        std::function<void(const ReplaceJobResult&)> done = on_job_done_;
        done(result);
    }
}

void ReplaceController::cancel_job() {
    if (job_.has_value()) {
        finish_job(ReplaceJobEnd::Cancelled, true);
    }
}

std::uint64_t ReplaceController::job_replaced() const {
    return job_.has_value() ? job_->replaced : 0;
}

void ReplaceController::set_on_job_progress(std::function<void()> callback) {
    on_job_progress_ = std::move(callback);
}

void ReplaceController::set_on_job_done(std::function<void(const ReplaceJobResult&)> callback) {
    on_job_done_ = std::move(callback);
}

} // namespace vermilion::ui
