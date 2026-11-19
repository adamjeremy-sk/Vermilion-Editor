// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The command line of a launch: its files, the questions on the terminal, the files created.

#include "ui/command_line.h"

#include <unistd.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "core/create_questions.h"
#include "core/open_arguments.h"
#include "ui/gtkx/async_guard.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

namespace {

// A terminal shows a readable "fd" property; a pipe (or a launch without one) has no such
// property or a negative descriptor.
bool is_terminal(GInputStream* stream) {
    if (stream == nullptr) {
        return false;
    }
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(stream), "fd") == nullptr) {
        return false;
    }
    int fd = -1;
    g_object_get(stream, "fd", &fd, nullptr);
    return fd >= 0 && isatty(fd) == 1;
}

// The files named on the command line, in argument order, each resolved against the launch's own
// working folder. A location that is not a local file has no path.
std::vector<ArgumentFile> argument_files(GApplicationCommandLine* cmdline) {
    std::vector<ArgumentFile> arguments;
    int argc = 0;
    std::unique_ptr<gchar*, decltype(&g_strfreev)> argv(
        g_application_command_line_get_arguments(cmdline, &argc), &g_strfreev);
    for (int i = 1; i < argc; i++) {
        auto file = gtkx::ObjectPtr<GFile>::adopt(
            g_application_command_line_create_file_for_arg(cmdline, argv.get()[i]));
        ArgumentFile argument;
        argument.name = gtkx::take_string(g_filename_display_name(argv.get()[i]));
        std::string path = gtkx::take_string(g_file_get_path(file.get()));
        if (!path.empty()) {
            argument.path = path;
        }
        arguments.push_back(std::move(argument));
    }
    return arguments;
}

} // namespace

class CommandLineHandler::Launch {
public:
    Launch(CommandLineHandler& owner, GApplicationCommandLine* cmdline)
        : owner_(owner), cmdline_(gtkx::ObjectPtr<GApplicationCommandLine>::ref(cmdline)) {
        g_application_hold(owner_.app_);
    }

    ~Launch() { release(); }

    Launch(const Launch&) = delete;
    Launch& operator=(const Launch&) = delete;

    bool finished() const { return finished_; }

    void start();

private:
    static void on_line(GObject* source, GAsyncResult* result, gpointer ticket);

    void step();
    void create_current();
    void note(const std::string& message);
    void finish();
    void release();

    CommandLineHandler& owner_;
    gtkx::ObjectPtr<GApplicationCommandLine> cmdline_;
    gtkx::ObjectPtr<GDataInputStream> input_; // empty without a terminal
    OpenRequest request_;
    std::unique_ptr<CreateQuestions> questions_;
    std::vector<std::string> messages_;
    bool held_ = true;
    bool finished_ = false;
    gtkx::AsyncGuard<Launch> guard_{this}; // last member, it goes first
};

void CommandLineHandler::Launch::start() {
    request_ = make_open_request(argument_files(cmdline_.get()));
    for (const std::string& message : request_.messages) {
        note(message);
    }
    auto stdin_stream =
        gtkx::ObjectPtr<GInputStream>::adopt(g_application_command_line_get_stdin(cmdline_.get()));
    std::vector<MissingFile> to_ask;
    if (is_terminal(stdin_stream.get())) {
        input_ =
            gtkx::ObjectPtr<GDataInputStream>::adopt(g_data_input_stream_new(stdin_stream.get()));
        g_data_input_stream_set_newline_type(input_.get(), G_DATA_STREAM_NEWLINE_TYPE_ANY);
        to_ask = request_.missing;
    } else {
        for (const MissingFile& missing : request_.missing) {
            note(missing_no_terminal_message(missing.name));
        }
    }
    questions_ = std::make_unique<CreateQuestions>(std::move(to_ask));
    step();
}

void CommandLineHandler::Launch::step() {
    for (;;) {
        switch (questions_->action()) {
        case QuestionAction::Prompt:
            g_application_command_line_print(cmdline_.get(), "%s ", questions_->prompt().c_str());
            g_data_input_stream_read_line_async(input_.get(), G_PRIORITY_DEFAULT, nullptr,
                                                &Launch::on_line, guard_.ticket());
            return;
        case QuestionAction::Create:
            create_current();
            break;
        case QuestionAction::Finished:
            finish();
            return;
        }
    }
}

void CommandLineHandler::Launch::on_line(GObject* source, GAsyncResult* result, gpointer ticket) {
    GError* raw = nullptr;
    gsize length = 0;
    char* line =
        g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(source), result, &length, &raw);
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    const bool got_line = line != nullptr;
    std::string text = gtkx::take_string(line);
    Launch* self = gtkx::AsyncGuard<Launch>::take(ticket);
    if (self == nullptr || self->finished_) {
        return;
    }
    if (got_line) {
        self->questions_->answer(std::string_view(text));
    } else {
        self->questions_->answer(std::nullopt);
    }
    self->step();
}

void CommandLineHandler::Launch::create_current() {
    const std::string name = questions_->current()->name;
    const std::string path = questions_->current()->path;
    auto file = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(path.c_str()));
    GError* raw = nullptr;
    auto stream = gtkx::ObjectPtr<GFileOutputStream>::adopt(
        g_file_create(file.get(), G_FILE_CREATE_NONE, nullptr, &raw));
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    if (!stream) {
        note("Could not create " + name + ": " + (error ? error->message : "unknown error"));
        questions_->created(false);
        return;
    }
    g_output_stream_close(G_OUTPUT_STREAM(stream.get()), nullptr, nullptr);
    questions_->created(true);
}

void CommandLineHandler::Launch::note(const std::string& message) {
    g_application_command_line_printerr(cmdline_.get(), "%s\n", message.c_str());
    messages_.push_back(message);
}

void CommandLineHandler::Launch::finish() {
    if (finished_) {
        return;
    }
    finished_ = true;
    Result result;
    result.paths = paths_to_open(request_, questions_->results());
    result.messages = messages_;
    input_.reset();
    owner_.done_(result);
    g_application_command_line_set_exit_status(cmdline_.get(), 0);
    // The launching process may end now that the launch is done.
    cmdline_.reset();
    release();
}

void CommandLineHandler::Launch::release() {
    if (held_) {
        held_ = false;
        g_application_release(owner_.app_);
    }
}

CommandLineHandler::CommandLineHandler(GApplication* app, Done done)
    : app_(app), done_(std::move(done)) {}

CommandLineHandler::~CommandLineHandler() = default;

void CommandLineHandler::handle(GApplicationCommandLine* cmdline) {
    launches_.erase(
        std::remove_if(launches_.begin(), launches_.end(),
                       [](const std::unique_ptr<Launch>& launch) { return launch->finished(); }),
        launches_.end());
    launches_.push_back(std::make_unique<Launch>(*this, cmdline));
    launches_.back()->start();
}

} // namespace vermilion::ui
