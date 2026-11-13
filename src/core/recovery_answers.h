// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// What the start-up question offers and says besides the question: buttons, text check, messages.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/recovery_file.h"
#include "core/recovery_offer.h"
#include "core/recovery_question.h"

namespace vermilion {

// What the user can answer to the question about one copy.
enum class RecoveryAnswer { Recover, Discard, RecoverAll, DecideLater };

// The text of the button: "Recover", "Discard", "Recover All" or "Decide Later".
const char* recovery_answer_label(RecoveryAnswer answer);

// The buttons of the question about item, in the order they are shown. The last one is always
// DecideLater: it is the dialog's Cancel, which Escape and the default key choose.
//   a valid copy:   Recover, Discard, RecoverAll (only when can_recover_all), DecideLater
//   a damaged copy: Discard, DecideLater (RecoverAll is never offered, whatever can_recover_all)
std::vector<RecoveryAnswer> recovery_answers(const RecoveryItem& item, bool can_recover_all);

// Why the body of a copy cannot be made a document, or nothing when it can: the text has a NUL
// byte, checked first ("the text contains a NUL character"), or is not UTF-8 ("the text is not
// valid UTF-8"; the check of GLib, which GTK's text buffer uses too, so what passes here the
// buffer takes without a warning). The empty text is fine.
std::optional<std::string> recovery_text_problem(std::string_view text);

// The second question, asked after Discard and before a file is deleted: the message is "Delete
// this copy?". The detail of a valid copy is "The recovery copy of <quoted name> will be deleted.
// This cannot be undone." (name from recovery_name(), quoted by quoted() of open_messages.h); of
// a damaged copy "The damaged recovery copy <id>.vrec will be deleted. This cannot be undone.".
RecoveryQuestion recovery_discard_question(const RecoveryItem& item);

// The status line when a copy of a file that is already open becomes an untitled document:
// "Recovered the text of <quoted name> as an untitled document, because that file is already
// open." The name is recovery_name() of the header.
std::string recovered_untitled_message(const RecoveryHeader& header);

// The status line when the start-up question ends and some copies stay on disk (kept of
// RecoveryOffer, plus the copies that could not be recovered): nothing for 0, "1 recovery copy
// was kept. It is offered again at the next start." for 1 and "N recovery copies were kept. They
// are offered again at the next start." for N above 1 (N in digits).
std::string recovery_kept_message(std::size_t kept);

} // namespace vermilion
