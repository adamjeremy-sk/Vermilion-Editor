// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)

#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <variant>

namespace vermilion {

struct Error {
    std::string message;
};

struct Unit {};

template <typename T>
class Result {
    static_assert(!std::is_same_v<T, Error>, "Result<T>: T must not be Error");

    std::variant<T, Error> storage_;

    explicit Result(std::variant<T, Error> v) : storage_(std::move(v)) {}

    [[noreturn]] static void die(const char* what) {
        std::fputs(what, stderr);
        std::fputs("\n", stderr);
        std::abort();
    }

public:
    static Result ok(T value) {
        return Result(std::variant<T, Error>(std::in_place_index<0>, std::move(value)));
    }

    static Result err(Error error) {
        return Result(std::variant<T, Error>(std::in_place_index<1>, std::move(error)));
    }

    bool is_ok() const {
        return storage_.index() == 0;
    }

    explicit operator bool() const {
        return is_ok();
    }

    T& value() & {
        if (storage_.index() != 0)
            die("Result::value() called on an error");
        return std::get<0>(storage_);
    }

    const T& value() const& {
        if (storage_.index() != 0)
            die("Result::value() called on an error");
        return std::get<0>(storage_);
    }

    T value() && {
        if (storage_.index() != 0)
            die("Result::value() called on an error");
        return std::move(std::get<0>(storage_));
    }

    const Error& error() const {
        if (storage_.index() != 1)
            die("Result::error() called on a success");
        return std::get<1>(storage_);
    }

    T value_or(T fallback) const& {
        if (is_ok())
            return std::get<0>(storage_);
        return std::move(fallback);
    }
};

using Status = Result<Unit>;

inline Status ok_status() {
    return Status::ok(Unit{});
}

}  // namespace vermilion
