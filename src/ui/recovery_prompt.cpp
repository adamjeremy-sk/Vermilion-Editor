// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The start-up question about recovery copies: the walk through them and what each answer does.

#include "ui/recovery_prompt.h"

#include <utility>

#include "core/log.h"
#include "core/recovery_answers.h"
#include "core/recovery_offer.h"
#include "core/recovery_question.h"
#include "ui/recovery_io.h"

namespace vermilion::ui {

struct RecoveryPrompt::Core : std::enable_shared_from_this<Core> {
    Core(RecoveryStore store, Host host)
        : store(std::move(store)), host(std::move(host)), offer({}) {}

    void start() {
        if (started) {
            return;
        }
        started = true;
        offer = RecoveryOffer(scan_recovery(store));
        next();
    }

    bool finished() const { return finished_; }

    // The copy to ask about now, or the end of the walk. A valid copy that belongs to a local
    // file is shown with the file's time, read off the main loop; every other copy is shown at
    // once with no time.
    void next() {
        if (offer.finished()) {
            finish();
            return;
        }
        RecoveryItem item = *offer.current();
        if (!item.damaged) {
            std::optional<std::string> path = recovery_path(item.header);
            if (path.has_value()) {
                std::weak_ptr<Core> weak = weak_from_this();
                std::string file = std::move(*path);
                query_file_time(std::move(file), [weak, item = std::move(item)](
                                                     std::optional<std::int64_t> mtime) mutable {
                    std::shared_ptr<Core> self = weak.lock();
                    if (!self) {
                        return;
                    }
                    self->show(std::move(item), std::move(mtime));
                });
                return;
            }
        }
        show(std::move(item), std::nullopt);
    }

    // Asks the question about one copy and carries out the answer when it comes.
    void show(RecoveryItem item, std::optional<std::int64_t> mtime) {
        RecoveryQuestion question = recovery_question(item, offer.number(), offer.total(), mtime);
        std::vector<RecoveryAnswer> answers = recovery_answers(item, offer.can_recover_all());
        std::vector<std::string> labels;
        labels.reserve(answers.size());
        for (RecoveryAnswer answer : answers) {
            labels.emplace_back(recovery_answer_label(answer));
        }
        std::weak_ptr<Core> weak = weak_from_this();
        host.ask(question.message, question.detail, labels,
                 [weak, item = std::move(item), mtime = std::move(mtime),
                  answers = std::move(answers)](int index) mutable {
                     std::shared_ptr<Core> self = weak.lock();
                     if (!self) {
                         return;
                     }
                     self->answered(std::move(item), mtime, self->answer_at(answers, index));
                 });
    }

    // The answer to the question about one copy, carried out.
    void answered(RecoveryItem item, std::optional<std::int64_t> mtime, RecoveryAnswer answer) {
        switch (answer) {
        case RecoveryAnswer::Recover:
            recover(std::move(item));
            break;
        case RecoveryAnswer::Discard:
            confirm_discard(std::move(item), std::move(mtime));
            break;
        case RecoveryAnswer::RecoverAll:
            recover_all();
            break;
        case RecoveryAnswer::DecideLater:
            offer.decide_later();
            next();
            break;
        }
    }

    // The second question, asked after Discard and before a file is deleted.
    void confirm_discard(RecoveryItem item, std::optional<std::int64_t> mtime) {
        RecoveryQuestion question = recovery_discard_question(item);
        std::weak_ptr<Core> weak = weak_from_this();
        host.ask(question.message, question.detail, {"Delete", "Cancel"},
                 [weak, item = std::move(item), mtime = std::move(mtime)](int index) mutable {
                     std::shared_ptr<Core> self = weak.lock();
                     if (!self) {
                         return;
                     }
                     self->discard_answered(std::move(item), mtime, index);
                 });
    }

    // The answer to the delete question: Delete removes the copy, Cancel asks the first question
    // again.
    void discard_answered(RecoveryItem item, std::optional<std::int64_t> mtime, int index) {
        if (index != 0) {
            show(std::move(item), mtime);
            return;
        }
        Status removed = store.remove(item.id);
        if (removed.is_ok()) {
            offer.answered();
        } else {
            logging::warning("recovery: cannot delete the copy " + item.id + ": " +
                             removed.error().message);
            offer.decide_later();
        }
        next();
    }

    // Reads the copy and, when it is whole, makes it a document.
    void recover(RecoveryItem item) {
        std::string id = item.id;
        std::weak_ptr<Core> weak = weak_from_this();
        read_recovery_copy(store, id, [weak, item = std::move(item)](CopyRead read) mutable {
            std::shared_ptr<Core> self = weak.lock();
            if (!self) {
                return;
            }
            self->read_done(std::move(item), std::move(read));
        });
    }

    // The result of reading a copy for Recover: a whole copy becomes a document, a bad one is
    // shown again as damaged.
    void read_done(RecoveryItem item, CopyRead read) {
        if (!read.copy.has_value()) {
            logging::warning("recovery: cannot read the copy " + item.id + ": " + read.problem);
            RecoveryItem stand_in = item;
            stand_in.damaged = true;
            stand_in.problem = std::move(read.problem);
            show(std::move(stand_in), std::nullopt);
            return;
        }
        bool adopted = open(std::move(item), std::move(*read.copy));
        if (adopted) {
            offer.answered();
        } else {
            offer.decide_later();
        }
        next();
    }

    // Recover All: every remaining valid copy is recovered in order, with no further question.
    void recover_all() {
        queue = offer.take_valid();
        queue_next = 0;
        recover_next();
    }

    // The next copy of Recover All, or the end of the walk when the queue is used up.
    void recover_next() {
        if (queue_next >= queue.size()) {
            finish();
            return;
        }
        RecoveryItem item = queue[queue_next];
        std::string id = item.id;
        std::weak_ptr<Core> weak = weak_from_this();
        read_recovery_copy(store, id, [weak, item = std::move(item)](CopyRead read) mutable {
            std::shared_ptr<Core> self = weak.lock();
            if (!self) {
                return;
            }
            if (!read.copy.has_value()) {
                logging::warning("recovery: cannot read the copy " + item.id + ": " + read.problem);
                ++self->skipped;
            } else if (!self->open(std::move(item), std::move(*read.copy))) {
                ++self->skipped;
            }
            ++self->queue_next;
            self->recover_next();
        });
    }

    // Makes a document from a whole copy and gives it the copy's id. True when the copy was taken
    // over, so it is dealt with; false when it was not, so it stays on disk and counts as kept.
    bool open(RecoveryItem item, RecoveryCopy copy) {
        if (!blank_closed) {
            blank_closed = true;
            host.close_blank();
        }
        std::optional<std::string> path = recovery_path(copy.header);
        bool had_path = path.has_value();
        Document document;
        document.path = std::move(path);
        document.format = copy.header.format;
        document.mixed_line_endings = copy.header.mixed_line_endings;
        document.file_mtime_us = copy.header.file_mtime_us;
        document.text = std::move(copy.text);
        Added added = host.add_document(std::move(document));
        bool adopted = host.adopt(added.id, item.id, copy.header.file_mtime_us);
        if (had_path && !added.has_path) {
            host.message(recovered_untitled_message(copy.header));
        }
        return adopted;
    }

    // The end of the walk: the closing message, when any copy was kept.
    void finish() {
        finished_ = true;
        std::string message = recovery_kept_message(offer.kept() + skipped);
        if (!message.empty()) {
            host.message(message);
        }
    }

    // The answer of the button at this index; an index outside the list is Decide Later.
    static RecoveryAnswer answer_at(const std::vector<RecoveryAnswer>& answers, int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= answers.size()) {
            return RecoveryAnswer::DecideLater;
        }
        return answers[static_cast<std::size_t>(index)];
    }

    RecoveryStore store;
    Host host;
    RecoveryOffer offer;
    bool started = false;
    bool finished_ = false;
    bool blank_closed = false;
    std::size_t skipped = 0;
    std::vector<RecoveryItem> queue;
    std::size_t queue_next = 0;
};

RecoveryPrompt::RecoveryPrompt(RecoveryStore store, Host host)
    : core_(std::make_shared<Core>(std::move(store), std::move(host))) {}

RecoveryPrompt::~RecoveryPrompt() = default;

void RecoveryPrompt::start() {
    core_->start();
}

bool RecoveryPrompt::finished() const {
    return core_->finished();
}

} // namespace vermilion::ui
