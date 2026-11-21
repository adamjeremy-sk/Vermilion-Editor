// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for AppearanceController with a recording host and a hand-driven timer: no GTK at all.

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/colour_mode.h"
#include "core/editor_look.h"
#include "core/editor_settings.h"
#include "ui/appearance_controller.h"
#include "ui/debounce_timer.h"

namespace {

using vermilion::ColourMode;
using vermilion::default_settings;
using vermilion::EditorLook;
using vermilion::EditorSettings;
using vermilion::SystemHints;
using vermilion::ui::AppearanceController;
using vermilion::ui::DebounceTimer;

// What the fake timer was asked, shared with the test because the controller owns the timer.
struct TimerState {
    std::function<void()> fire; // empty when nothing waits
    unsigned delay_ms = 0;
    int starts = 0;
    int stops = 0;
    bool destroyed = false;

    // The timer runs out: the waiting call runs once.
    void expire() {
        std::function<void()> call = std::move(fire);
        fire = nullptr;
        if (call) {
            call();
        }
    }
};

class FakeTimer : public DebounceTimer {
public:
    explicit FakeTimer(TimerState* state) : state_(state) {}
    ~FakeTimer() override { state_->destroyed = true; }

    void start(unsigned delay_ms, std::function<void()> fire) override {
        ++state_->starts;
        state_->delay_ms = delay_ms;
        state_->fire = std::move(fire);
    }

    void stop() override {
        ++state_->stops;
        state_->fire = nullptr;
    }

private:
    TimerState* state_;
};

// Writes every call down as one line of text.
class RecordingHost : public AppearanceController::Host {
public:
    void apply_palette(bool dark) override {
        calls.push_back(dark ? "palette:dark" : "palette:light");
    }

    void apply_look(const EditorLook& look) override {
        looks.push_back(look);
        calls.push_back(std::string("look:") + (look.dark ? "dark" : "light") + "," +
                        (look.highlight_current_line ? "line" : "noline") + "," +
                        std::to_string(look.tab_width) + "," + look.font);
    }

    void apply_long_line_limit(std::uint64_t limit) override {
        calls.push_back("limit:" + std::to_string(limit));
    }

    std::vector<std::string> calls;
    std::vector<EditorLook> looks;
};

class AppearanceControllerTest : public ::testing::Test {
protected:
    AppearanceControllerTest() : controller_(host_, std::make_unique<FakeTimer>(&timer_)) {}

    RecordingHost host_;
    TimerState timer_;
    AppearanceController controller_;
};

TEST_F(AppearanceControllerTest, StartGivesTheHostAllThreePartsAtOnce) {
    controller_.start(default_settings());
    ASSERT_EQ(host_.calls.size(), std::size_t{3});
    EXPECT_EQ(host_.calls[0], "palette:light");
    EXPECT_EQ(host_.calls[1], "look:light,line,4,Fantasque Sans Mono 12");
    EXPECT_EQ(host_.calls[2], "limit:350000");
    EXPECT_FALSE(controller_.pending());
}

TEST_F(AppearanceControllerTest, ApplyWaitsOneHundredMilliseconds) {
    controller_.start(default_settings());
    host_.calls.clear();
    EditorSettings next = default_settings();
    next.tab_width = 8;
    controller_.apply(next);
    EXPECT_TRUE(host_.calls.empty());
    EXPECT_TRUE(controller_.pending());
    EXPECT_EQ(timer_.delay_ms, 100u);
    EXPECT_EQ(AppearanceController::kDelayMs, 100u);
    timer_.expire();
    ASSERT_EQ(host_.calls.size(), std::size_t{1});
    EXPECT_EQ(host_.calls[0], "look:light,line,8,Fantasque Sans Mono 12");
    EXPECT_FALSE(controller_.pending());
}

TEST_F(AppearanceControllerTest, TheLastValuesWin) {
    controller_.start(default_settings());
    host_.calls.clear();
    EditorSettings first = default_settings();
    first.tab_width = 8;
    first.font = "Mono 9";
    EditorSettings second = default_settings();
    second.font = "Mono 12";
    controller_.apply(first);
    controller_.apply(second);
    EXPECT_EQ(timer_.starts, 2);
    timer_.expire();
    ASSERT_EQ(host_.calls.size(), std::size_t{1});
    EXPECT_EQ(host_.calls[0], "look:light,line,4,Mono 12");
}

TEST_F(AppearanceControllerTest, OnlyTheChangedPartsAreApplied) {
    controller_.start(default_settings());
    host_.calls.clear();
    EditorSettings next = default_settings();
    next.long_line_limit = 20000;
    controller_.apply(next);
    timer_.expire();
    ASSERT_EQ(host_.calls.size(), std::size_t{1});
    EXPECT_EQ(host_.calls[0], "limit:20000");
}

TEST_F(AppearanceControllerTest, NothingChangedNothingIsApplied) {
    controller_.start(default_settings());
    host_.calls.clear();
    controller_.apply(default_settings());
    EXPECT_TRUE(controller_.pending());
    timer_.expire();
    EXPECT_TRUE(host_.calls.empty());
}

TEST_F(AppearanceControllerTest, ChangingTheColourModeChangesThePaletteAndTheLook) {
    controller_.start(default_settings());
    host_.calls.clear();
    EditorSettings next = default_settings();
    next.colour_mode = ColourMode::Dark;
    controller_.apply(next);
    timer_.expire();
    ASSERT_EQ(host_.calls.size(), std::size_t{2});
    EXPECT_EQ(host_.calls[0], "palette:dark");
    EXPECT_EQ(host_.calls[1], "look:dark,line,4,Fantasque Sans Mono 12");
}

TEST_F(AppearanceControllerTest, StartInDarkAppliesTheDarkPaletteFirst) {
    EditorSettings dark = default_settings();
    dark.colour_mode = ColourMode::Dark;
    controller_.start(dark);
    ASSERT_EQ(host_.calls.size(), std::size_t{3});
    EXPECT_EQ(host_.calls[0], "palette:dark");
    EXPECT_EQ(host_.calls[1], "look:dark,line,4,Fantasque Sans Mono 12");
}

TEST_F(AppearanceControllerTest, FollowSystemWithoutHintsIsLight) {
    controller_.start(default_settings());
    EXPECT_EQ(host_.calls[0], "palette:light");
}

TEST_F(AppearanceControllerTest, FollowSystemUsesTheHints) {
    controller_.start(default_settings());
    host_.calls.clear();
    SystemHints hints;
    hints.theme_name = "Greybird-dark";
    controller_.set_hints(hints);
    EXPECT_TRUE(host_.calls.empty());
    EXPECT_TRUE(controller_.pending());
    EXPECT_EQ(timer_.delay_ms, 100u);
    timer_.expire();
    ASSERT_EQ(host_.calls.size(), std::size_t{2});
    EXPECT_EQ(host_.calls[0], "palette:dark");
    EXPECT_EQ(host_.calls[1], "look:dark,line,4,Fantasque Sans Mono 12");
}

TEST_F(AppearanceControllerTest, HintsBeforeStartAreKeptAndUsed) {
    SystemHints hints;
    hints.portal_dark = true;
    controller_.set_hints(hints);
    EXPECT_FALSE(controller_.pending());
    EXPECT_EQ(timer_.starts, 0);
    EXPECT_TRUE(host_.calls.empty());
    controller_.start(default_settings());
    EXPECT_EQ(host_.calls[0], "palette:dark");
}

TEST_F(AppearanceControllerTest, LightAndDarkIgnoreTheHints) {
    EditorSettings light = default_settings();
    light.colour_mode = ColourMode::Light;
    controller_.start(light);
    host_.calls.clear();
    SystemHints hints;
    hints.portal_dark = true;
    controller_.set_hints(hints);
    timer_.expire();
    EXPECT_TRUE(host_.calls.empty());
}

TEST_F(AppearanceControllerTest, ApplyBeforeStartOnlyKeepsTheValues) {
    EditorSettings next = default_settings();
    next.tab_width = 8;
    controller_.apply(next);
    EXPECT_EQ(controller_.settings().tab_width, 8);
    EXPECT_FALSE(controller_.pending());
    EXPECT_EQ(timer_.starts, 0);
    EXPECT_TRUE(host_.calls.empty());
    controller_.start(next);
    EXPECT_EQ(host_.calls[1], "look:light,line,8,Fantasque Sans Mono 12");
}

TEST_F(AppearanceControllerTest, StartDropsAChangeThatIsWaiting) {
    controller_.start(default_settings());
    host_.calls.clear();
    EditorSettings waiting = default_settings();
    waiting.font = "Mono 9";
    controller_.apply(waiting);
    EditorSettings now = default_settings();
    now.font = "Mono 14";
    controller_.start(now);
    EXPECT_FALSE(controller_.pending());
    EXPECT_FALSE(static_cast<bool>(timer_.fire));
    ASSERT_EQ(host_.calls.size(), std::size_t{1});
    EXPECT_EQ(host_.calls[0], "look:light,line,4,Mono 14");
    timer_.expire();
    EXPECT_EQ(host_.calls.size(), std::size_t{1});
}

TEST_F(AppearanceControllerTest, TheSettingsAreTheLatestEvenBeforeTheyAreApplied) {
    controller_.start(default_settings());
    EditorSettings next = default_settings();
    next.highlight_current_line = false;
    controller_.apply(next);
    EXPECT_FALSE(controller_.settings().highlight_current_line);
    EXPECT_TRUE(controller_.pending());
}

TEST_F(AppearanceControllerTest, TheValuesAreNormalised) {
    EditorSettings odd = default_settings();
    odd.tab_width = 5;
    odd.long_line_limit = 3;
    odd.font = " Mono 10 ";
    controller_.start(odd);
    EXPECT_EQ(controller_.settings().tab_width, 4);
    EXPECT_EQ(controller_.settings().long_line_limit, std::uint64_t{10000});
    EXPECT_EQ(controller_.settings().font, std::string("Mono 10"));
    ASSERT_EQ(host_.calls.size(), std::size_t{3});
    EXPECT_EQ(host_.calls[1], "look:light,line,4,Mono 10");
    EXPECT_EQ(host_.calls[2], "limit:10000");
}

TEST_F(AppearanceControllerTest, ApplyNormalisesToo) {
    controller_.start(default_settings());
    host_.calls.clear();
    EditorSettings odd = default_settings();
    odd.long_line_limit = 3;
    odd.tab_width = 7;
    controller_.apply(odd);
    EXPECT_EQ(controller_.settings().long_line_limit, std::uint64_t{10000});
    EXPECT_EQ(controller_.settings().tab_width, 4);
    timer_.expire();
    ASSERT_EQ(host_.calls.size(), std::size_t{1});
    EXPECT_EQ(host_.calls[0], "limit:10000");
}

TEST_F(AppearanceControllerTest, DestroyingTheControllerStopsTheTimer) {
    auto state = std::make_unique<TimerState>();
    RecordingHost host;
    {
        AppearanceController controller(host, std::make_unique<FakeTimer>(state.get()));
        controller.start(default_settings());
        EditorSettings next = default_settings();
        next.tab_width = 8;
        controller.apply(next);
        EXPECT_TRUE(static_cast<bool>(state->fire));
    }
    EXPECT_TRUE(state->destroyed);
    EXPECT_FALSE(static_cast<bool>(state->fire));
    EXPECT_GE(state->stops, 1);
    EXPECT_EQ(host.calls.size(), std::size_t{3});
}

} // namespace
