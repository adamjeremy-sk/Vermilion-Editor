// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Opens, sizes and reads a file asynchronously in 64 KiB blocks, feeding every block to the
// scanner.

#include "ui/file_probe.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace vermilion::ui {

namespace {

constexpr gsize kBlockSize = 64 * 1024;

using ErrorPtr = std::unique_ptr<GError, decltype(&g_error_free)>;

ErrorPtr take_error(GError* error) {
    return ErrorPtr(error, &g_error_free);
}

} // namespace

// Full references from _new (not floating).
FileProbe::FileProbe(const std::string& path, Callbacks callbacks)
    : callbacks_(std::move(callbacks)),
      cancellable_(gtkx::ObjectPtr<GCancellable>::adopt(g_cancellable_new())) {
    auto file = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(path.c_str()));
    g_file_read_async(file.get(), G_PRIORITY_DEFAULT, cancellable_.get(), &FileProbe::on_opened,
                      guard_.ticket());
}

FileProbe::~FileProbe() {
    g_cancellable_cancel(cancellable_.get());
}

void FileProbe::on_opened(GObject* source, GAsyncResult* result, gpointer ticket) {
    GError* raw = nullptr;
    // Full reference (transfer full) or nullptr with the error set.
    auto stream =
        gtkx::ObjectPtr<GFileInputStream>::adopt(g_file_read_finish(G_FILE(source), result, &raw));
    ErrorPtr error = take_error(raw);
    FileProbe* self = gtkx::AsyncGuard<FileProbe>::take(ticket);
    if (self == nullptr) {
        return;
    }
    if (error) {
        self->finish(Result{false, ScanResult{}, error->message});
        return;
    }
    self->stream_ = std::move(stream);
    g_file_input_stream_query_info_async(self->stream_.get(), G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                         G_PRIORITY_DEFAULT, self->cancellable_.get(),
                                         &FileProbe::on_info, self->guard_.ticket());
}

// A file whose size cannot be read is still scanned; the progress just stays unknown.
void FileProbe::on_info(GObject* source, GAsyncResult* result, gpointer ticket) {
    GError* raw = nullptr;
    auto info = gtkx::ObjectPtr<GFileInfo>::adopt(
        g_file_input_stream_query_info_finish(G_FILE_INPUT_STREAM(source), result, &raw));
    ErrorPtr error = take_error(raw);
    FileProbe* self = gtkx::AsyncGuard<FileProbe>::take(ticket);
    if (self == nullptr) {
        return;
    }
    if (info && g_file_info_has_attribute(info.get(), G_FILE_ATTRIBUTE_STANDARD_SIZE)) {
        goffset size = g_file_info_get_size(info.get());
        self->total_ = size > 0 ? static_cast<std::uint64_t>(size) : 0;
    }
    self->read_next();
}

void FileProbe::read_next() {
    g_input_stream_read_bytes_async(G_INPUT_STREAM(stream_.get()), kBlockSize, G_PRIORITY_DEFAULT,
                                    cancellable_.get(), &FileProbe::on_read, guard_.ticket());
}

void FileProbe::on_read(GObject* source, GAsyncResult* result, gpointer ticket) {
    GError* raw = nullptr;
    // Full reference to the block (transfer full); an empty block marks the end of the file.
    std::unique_ptr<GBytes, decltype(&g_bytes_unref)> bytes(
        g_input_stream_read_bytes_finish(G_INPUT_STREAM(source), result, &raw), &g_bytes_unref);
    ErrorPtr error = take_error(raw);
    FileProbe* self = gtkx::AsyncGuard<FileProbe>::take(ticket);
    if (self == nullptr) {
        return;
    }
    if (error) {
        self->finish(Result{false, ScanResult{}, error->message});
        return;
    }
    gsize size = 0;
    const void* data = g_bytes_get_data(bytes.get(), &size);
    if (size == 0) {
        self->finish(Result{true, self->scanner_.finish(), std::string()});
        return;
    }
    self->scanner_.feed(static_cast<const char*>(data), size);
    self->read_ += size;
    if (self->total_ > 0) {
        int percent =
            static_cast<int>(std::min<std::uint64_t>(100, self->read_ * 100 / self->total_));
        if (percent != self->percent_) {
            self->percent_ = percent;
            self->callbacks_.progress(percent);
        }
    }
    self->read_next();
}

// The callback may destroy this object, so it runs last, from a local copy.
void FileProbe::finish(Result result) {
    stream_.reset();
    std::function<void(const Result&)> finished = std::move(callbacks_.finished);
    finished(result);
}

} // namespace vermilion::ui
