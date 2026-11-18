// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for FileWatcher: events on a real file turn into one "look again" after a pause.

#include <gtest/gtest.h>

#include <glib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>

#include "ui/debounce_timer.h"
#include "ui/file_watcher.h"
#include "ui/glib_timer.h"

namespace {

namespace fs = std::filesystem;

using vermilion::ui::DebounceTimer;
using vermilion::ui::FileWatcher;
using vermilion::ui::GlibTimer;

// Runs the default main context for `ms` milliseconds.
void pump(int ms) {
    gint64 end = g_get_monotonic_time() + static_cast<gint64>(ms) * 1000;
    while (g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

// Runs the main context until `done()` is true or 5 seconds pass; returns whether it became true.
bool pump_until(const std::function<bool()>& done) {
    gint64 end = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    while (!done() && g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
    return done();
}

// A directory that is removed at the end of the test.
class TempDir {
public:
    TempDir() {
        char pattern[] = "/tmp/vermilion-watcher-XXXXXX";
        const char* made = mkdtemp(pattern);
        dir_ = made != nullptr ? made : "";
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
    std::string path(const std::string& name) const { return dir_ + "/" + name; }

private:
    std::string dir_;
};

void write_file(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

void append_file(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    out << text;
}

// A timer that only records what it was asked to do; the test fires it by hand.
struct FakeTimerState {
    int starts = 0;
    int stops = 0;
    unsigned last_delay = 0;
    std::function<void()> fire;
};

class FakeTimer : public DebounceTimer {
public:
    explicit FakeTimer(FakeTimerState* state) : state_(state) {}
    void start(unsigned delay_ms, std::function<void()> fire) override {
        ++state_->starts;
        state_->last_delay = delay_ms;
        state_->fire = std::move(fire);
    }
    void stop() override {
        ++state_->stops;
        state_->fire = nullptr;
    }

private:
    FakeTimerState* state_;
};

constexpr unsigned kShort = 40;

std::unique_ptr<FileWatcher> make_watcher(int* looks, unsigned delay = kShort) {
    return std::make_unique<FileWatcher>(
        std::make_unique<GlibTimer>(), [looks] { ++*looks; }, delay);
}

TEST(FileWatcher, DefaultDelayIs300Milliseconds) {
    EXPECT_EQ(FileWatcher::kDefaultDelayMs, 300u);
}

TEST(FileWatcher, StartsWithNothingWatched) {
    int looks = 0;
    auto watcher = make_watcher(&looks);
    EXPECT_FALSE(watcher->watching());
    EXPECT_TRUE(watcher->path().empty());
}

TEST(FileWatcher, WatchRemembersThePath) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("a.txt"));
    EXPECT_TRUE(watcher->watching());
    EXPECT_EQ(watcher->path(), dir.path("a.txt"));
    pump(100);
    EXPECT_EQ(looks, 0) << "watching alone is not a change";
}

TEST(FileWatcher, EmptyPathStops) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("a.txt"));
    watcher->watch("");
    EXPECT_FALSE(watcher->watching());
    EXPECT_TRUE(watcher->path().empty());
}

TEST(FileWatcher, AppendLooksOnce) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("a.txt"));
    pump(50);
    append_file(dir.path("a.txt"), "two\n");
    ASSERT_TRUE(pump_until([&] { return looks >= 1; }));
    pump(200);
    EXPECT_EQ(looks, 1);
}

TEST(FileWatcher, ABurstOfWritesLooksOnce) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    auto watcher = make_watcher(&looks, 150);
    watcher->watch(dir.path("a.txt"));
    pump(50);
    for (int i = 0; i < 5; ++i) {
        append_file(dir.path("a.txt"), "line\n");
        pump(10);
    }
    ASSERT_TRUE(pump_until([&] { return looks >= 1; }));
    pump(400);
    EXPECT_EQ(looks, 1);
}

TEST(FileWatcher, TouchLooks) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("a.txt"));
    pump(50);
    struct timeval times[2] = {{1000000000, 0}, {1000000000, 0}};
    ASSERT_EQ(utimes(dir.path("a.txt").c_str(), times), 0);
    EXPECT_TRUE(pump_until([&] { return looks >= 1; }));
}

TEST(FileWatcher, RenameOverTheFileLooks) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    write_file(dir.path("b.txt"), "other\n");
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("a.txt"));
    pump(50);
    fs::rename(dir.path("b.txt"), dir.path("a.txt"));
    EXPECT_TRUE(pump_until([&] { return looks >= 1; }));
}

TEST(FileWatcher, DeleteLooks) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("a.txt"));
    pump(50);
    fs::remove(dir.path("a.txt"));
    EXPECT_TRUE(pump_until([&] { return looks >= 1; }));
}

TEST(FileWatcher, KeepsWatchingAfterDeleteAndCreateAgain) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("a.txt"));
    pump(50);
    fs::remove(dir.path("a.txt"));
    ASSERT_TRUE(pump_until([&] { return looks >= 1; }));
    pump(100);
    write_file(dir.path("a.txt"), "back\n");
    EXPECT_TRUE(pump_until([&] { return looks >= 2; }));
    pump(100);
    int after_create = looks;
    append_file(dir.path("a.txt"), "more\n");
    EXPECT_TRUE(pump_until([&] { return looks > after_create; }));
}

TEST(FileWatcher, WatchesAFileThatDoesNotExistYet) {
    TempDir dir;
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("later.txt"));
    EXPECT_TRUE(watcher->watching());
    pump(50);
    write_file(dir.path("later.txt"), "hello\n");
    EXPECT_TRUE(pump_until([&] { return looks >= 1; }));
}

TEST(FileWatcher, OtherFilesInTheFolderAreIgnored) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("a.txt"));
    pump(50);
    write_file(dir.path("other.txt"), "x\n");
    append_file(dir.path("other.txt"), "y\n");
    pump(300);
    EXPECT_EQ(looks, 0);
}

TEST(FileWatcher, StopMeansNoMoreLooks) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("a.txt"));
    pump(50);
    watcher->stop();
    EXPECT_FALSE(watcher->watching());
    append_file(dir.path("a.txt"), "two\n");
    pump(300);
    EXPECT_EQ(looks, 0);
}

TEST(FileWatcher, StopDropsALookThatIsWaiting) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    auto watcher = make_watcher(&looks, 200);
    watcher->watch(dir.path("a.txt"));
    pump(50);
    append_file(dir.path("a.txt"), "two\n");
    pump(80);
    watcher->stop();
    pump(400);
    EXPECT_EQ(looks, 0);
}

TEST(FileWatcher, WatchingAnotherPathReplacesTheFirst) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    write_file(dir.path("b.txt"), "two\n");
    int looks = 0;
    auto watcher = make_watcher(&looks);
    watcher->watch(dir.path("a.txt"));
    watcher->watch(dir.path("b.txt"));
    EXPECT_EQ(watcher->path(), dir.path("b.txt"));
    pump(50);
    append_file(dir.path("a.txt"), "x\n");
    pump(300);
    EXPECT_EQ(looks, 0) << "the old path is no longer watched";
    append_file(dir.path("b.txt"), "y\n");
    EXPECT_TRUE(pump_until([&] { return looks >= 1; }));
}

TEST(FileWatcher, ReplacingThePathDropsAWaitingLook) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    write_file(dir.path("b.txt"), "two\n");
    int looks = 0;
    auto watcher = make_watcher(&looks, 200);
    watcher->watch(dir.path("a.txt"));
    pump(50);
    append_file(dir.path("a.txt"), "x\n");
    pump(80);
    watcher->watch(dir.path("b.txt"));
    pump(400);
    EXPECT_EQ(looks, 0);
}

TEST(FileWatcher, WatchingTheSamePathAgainChangesNothing) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    FakeTimerState state;
    int looks = 0;
    FileWatcher watcher(std::make_unique<FakeTimer>(&state), [&] { ++looks; }, 123);
    watcher.watch(dir.path("a.txt"));
    int stops = state.stops;
    watcher.watch(dir.path("a.txt"));
    EXPECT_TRUE(watcher.watching());
    EXPECT_EQ(state.stops, stops) << "the same path must not stop the timer";
}

TEST(FileWatcher, AnEventStartsTheTimerWithTheDelayAndFiringLooks) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    FakeTimerState state;
    int looks = 0;
    FileWatcher watcher(std::make_unique<FakeTimer>(&state), [&] { ++looks; }, 123);
    watcher.watch(dir.path("a.txt"));
    pump(50);
    append_file(dir.path("a.txt"), "two\n");
    ASSERT_TRUE(pump_until([&] { return state.starts >= 1; }));
    EXPECT_EQ(state.last_delay, 123u);
    EXPECT_EQ(looks, 0) << "nothing looks before the timer fires";
    ASSERT_TRUE(static_cast<bool>(state.fire));
    state.fire();
    EXPECT_EQ(looks, 1);
}

TEST(FileWatcher, DestroyedWatcherNeverLooks) {
    TempDir dir;
    write_file(dir.path("a.txt"), "one\n");
    int looks = 0;
    {
        auto watcher = make_watcher(&looks, 200);
        watcher->watch(dir.path("a.txt"));
        pump(50);
        append_file(dir.path("a.txt"), "two\n");
        pump(80);
    }
    pump(400);
    EXPECT_EQ(looks, 0);
}

} // namespace
