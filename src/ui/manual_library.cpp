// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Reads and parses the manual once, on a worker thread, and keeps the result for the session.

#include "ui/manual_library.h"

#include <gio/gio.h>

#include <memory>
#include <string>
#include <utility>

#include "core/manual_spans.h"
#include "core/manual_text.h"

namespace vermilion::ui {
namespace {

constexpr const char* kManualPrefix = "/io/github/adamjeremy_sk/Vermilion/manual";
constexpr const char* kIndexTopic = "index.md";
constexpr gsize kMaxTopicBytes = 1024 * 1024;
// The width the pages are laid out for when only their headings are wanted (the headings and the
// links do not depend on it).
constexpr std::size_t kOutlineColumns = 80;

using BytesPtr = std::unique_ptr<GBytes, decltype(&g_bytes_unref)>;
using NamesPtr = std::unique_ptr<char*, decltype(&g_strfreev)>;

} // namespace

std::optional<std::string> topic_text(const std::string& name) {
    if (!is_topic_name(name)) {
        return std::nullopt;
    }
    const std::string path = std::string(kManualPrefix) + "/" + name;
    BytesPtr bytes(g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
                   &g_bytes_unref); // full reference; nullptr when the topic is not in the program
    if (bytes == nullptr) {
        return std::nullopt;
    }
    gsize size = 0;
    const void* data = g_bytes_get_data(bytes.get(), &size);
    if (size > kMaxTopicBytes) {
        return std::nullopt;
    }
    return std::string(static_cast<const char*>(data), size);
}

ManualData make_manual_data(const std::map<std::string, std::string>& texts) {
    ManualData data;
    std::map<std::string, ManualText> laid_out;
    for (const auto& [name, text] : texts) {
        ManualPage page = parse_manual(text);
        laid_out.emplace(name, build_manual_text(page, kOutlineColumns));
        data.pages.emplace(name, std::move(page));
    }
    data.outline = build_outline(kIndexTopic, laid_out);
    return data;
}

ManualData load_manual() {
    std::map<std::string, std::string> texts;
    NamesPtr names(
        g_resources_enumerate_children(kManualPrefix, G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
        &g_strfreev); // nullptr when the folder is not in the program
    if (names != nullptr) {
        for (char** name = names.get(); *name != nullptr; ++name) {
            std::optional<std::string> text = topic_text(*name);
            if (text.has_value()) {
                texts.emplace(*name, std::move(*text));
            }
        }
    }
    return make_manual_data(texts);
}

std::vector<SearchPage> manual_search_pages(const ManualData& data, std::size_t columns) {
    std::vector<SearchPage> pages;
    for (const OutlineTopic& topic : data.outline) {
        const auto page = data.pages.find(topic.file);
        if (page != data.pages.end()) {
            pages.push_back(
                {topic.file, topic.title, build_manual_text(page->second, columns).text});
        }
    }
    return pages;
}

ManualLibrary::Wait::Wait(std::weak_ptr<Callbacks> callbacks, std::uint64_t id)
    : callbacks_(std::move(callbacks)), id_(id) {}

ManualLibrary::Wait::Wait(Wait&& other) noexcept
    : callbacks_(std::move(other.callbacks_)), id_(std::exchange(other.id_, 0)) {}

ManualLibrary::Wait& ManualLibrary::Wait::operator=(Wait&& other) noexcept {
    if (this != &other) {
        release();
        callbacks_ = std::move(other.callbacks_);
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}

ManualLibrary::Wait::~Wait() {
    release();
}

void ManualLibrary::Wait::release() {
    if (id_ != 0) {
        const std::shared_ptr<Callbacks> callbacks =
            callbacks_.lock(); // empty when the library is gone
        if (callbacks != nullptr) {
            callbacks->by_id.erase(id_);
        }
    }
    id_ = 0;
    callbacks_.reset();
}

ManualLibrary::ManualLibrary() = default;

ManualLibrary::~ManualLibrary() {
    if (idle_source_ != 0) {
        g_source_remove(idle_source_);
    }
}

void ManualLibrary::prepare() {
    if (started_) {
        return;
    }
    started_ = true;
    scope_.run<ManualData>([](const CancelCheck&) { return load_manual(); },
                           [this](ManualData data) { finish(std::move(data)); });
}

void ManualLibrary::prepare_when_idle() {
    if (started_ || idle_source_ != 0) {
        return;
    }
    idle_source_ = g_idle_add_full(G_PRIORITY_LOW, &ManualLibrary::on_idle, this, nullptr);
}

gboolean ManualLibrary::on_idle(gpointer data) {
    ManualLibrary* library = static_cast<ManualLibrary*>(data);
    library->idle_source_ = 0;
    library->prepare();
    return G_SOURCE_REMOVE;
}

ManualLibrary::Wait ManualLibrary::on_ready(std::function<void()> callback) {
    const std::uint64_t id = waiting_->next_id++;
    waiting_->by_id.emplace(id, std::move(callback));
    return Wait(waiting_, id);
}

void ManualLibrary::finish(ManualData data) {
    data_ = std::move(data);
    // A callback may end the Wait of another one, so each is looked up again before it is called.
    std::vector<std::uint64_t> ids;
    for (const auto& entry : waiting_->by_id) {
        ids.push_back(entry.first);
    }
    for (const std::uint64_t id : ids) {
        const auto found = waiting_->by_id.find(id);
        if (found == waiting_->by_id.end()) {
            continue;
        }
        std::function<void()> callback = std::move(found->second);
        waiting_->by_id.erase(found);
        callback();
    }
}

} // namespace vermilion::ui
