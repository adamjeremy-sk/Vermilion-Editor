// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of the shortcut table of the main window: every key is set and each key starts one command.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <vector>

#include "ui/window_shortcuts.h"

namespace {

struct Expected {
    const char* action;
    std::vector<std::string> accels;
};

// The table of ARCHITECTURE.md 7.10 as it stands (written out again here on purpose, so that a
// change of a key is a change of this test too).
const std::vector<Expected>& expected_table() {
    static const std::vector<Expected> table = {
        {"win.new-file", {"<Control>n"}},
        {"win.open", {"<Control>o"}},
        {"win.save", {"<Control>s"}},
        {"win.save-as", {"<Control><Shift>s"}},
        {"win.reload-from-disk", {"<Control>r"}},
        {"win.close-tab", {"<Control>w"}},
        {"win.quit", {"<Control>q"}},
        {"win.undo", {"<Control>z"}},
        {"win.redo", {"<Control>y", "<Control><Shift>z"}},
        {"win.cut", {"<Control>x"}},
        {"win.copy", {"<Control>c"}},
        {"win.paste", {"<Control>v"}},
        {"win.select-all", {"<Control>a"}},
        {"win.find", {"<Control>f"}},
        {"win.find-next", {"F3"}},
        {"win.find-previous", {"<Shift>F3"}},
        {"win.replace", {"<Control>h"}},
        {"win.go-to-line", {"<Control>g"}},
        {"win.next-tab", {"<Control>Page_Down"}},
        {"win.previous-tab", {"<Control>Page_Up"}},
        {"win.settings", {"<Control>comma"}},
        {"win.help-index", {"F1"}},
    };
    return table;
}

// The key as GTK spells it after reading it (so that "<Shift><Control>s" and "<Control><Shift>s"
// compare equal); empty when GTK cannot read the text.
std::string spelled(const std::string& accel) {
    guint key = 0;
    GdkModifierType modifiers = static_cast<GdkModifierType>(0);
    if (gtk_accelerator_parse(accel.c_str(), &key, &modifiers) == FALSE || key == 0) {
        return std::string();
    }
    char* name = gtk_accelerator_name(key, modifiers); // owned here
    std::string result = name != nullptr ? name : "";
    g_free(name);
    return result;
}

std::vector<std::string> take(char** strv) {
    std::vector<std::string> result;
    for (char** at = strv; at != nullptr && *at != nullptr; ++at) {
        result.emplace_back(*at);
    }
    g_strfreev(strv);
    return result;
}

class WindowShortcutsTest : public ::testing::Test {
protected:
    void SetUp() override {
        app_.reset(gtk_application_new("io.github.adamjeremy_sk.VermilionShortcutsTest",
                                       G_APPLICATION_DEFAULT_FLAGS));
        ASSERT_NE(app_.get(), nullptr);
        vermilion::ui::set_shortcuts(app_.get());
    }

    std::vector<std::string> accels_of(const char* action) const {
        return take(gtk_application_get_accels_for_action(app_.get(), action));
    }
    std::vector<std::string> actions_of(const std::string& accel) const {
        return take(gtk_application_get_actions_for_accel(app_.get(), accel.c_str()));
    }

    std::unique_ptr<GtkApplication, void (*)(gpointer)> app_{nullptr, &g_object_unref};
};

TEST_F(WindowShortcutsTest, ControlCommaOpensTheSettings) {
    const std::vector<std::string> accels = accels_of("win.settings");
    ASSERT_EQ(accels.size(), 1U);
    EXPECT_EQ(accels[0], spelled("<Control>comma"));
    EXPECT_FALSE(accels[0].empty());
}

TEST_F(WindowShortcutsTest, EveryKeyOfTheTableIsSet) {
    for (const Expected& entry : expected_table()) {
        SCOPED_TRACE(entry.action);
        std::vector<std::string> want;
        for (const std::string& accel : entry.accels) {
            ASSERT_FALSE(spelled(accel).empty()) << accel;
            want.push_back(spelled(accel));
        }
        EXPECT_EQ(accels_of(entry.action), want);
    }
}

TEST_F(WindowShortcutsTest, EachKeyStartsOneCommand) {
    for (const Expected& entry : expected_table()) {
        for (const std::string& accel : entry.accels) {
            SCOPED_TRACE(accel);
            const std::vector<std::string> actions = actions_of(accel);
            ASSERT_EQ(actions.size(), 1U);
            EXPECT_EQ(actions[0], entry.action);
        }
    }
}

TEST_F(WindowShortcutsTest, CommandsWithoutAKeyStayWithoutOne) {
    EXPECT_TRUE(accels_of("win.about").empty());
    EXPECT_TRUE(accels_of("win.word-wrap").empty());
}

} // namespace
