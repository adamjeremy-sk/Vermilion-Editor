// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for redact_secrets: rules A (named fields), B (bearer tokens) and C (key prefixes).

#include <gtest/gtest.h>

#include <string>

#include "core/redact.h"

using vermilion::redact_secrets;

TEST(CoreRedact, PlainTextUnchanged) {
    const std::string in = "n_tokens=128 slot idle; max_tokens: 4096 ok";
    EXPECT_EQ(redact_secrets(in), in);
}

TEST(CoreRedact, EmptyValuesUnchanged) {
    EXPECT_EQ(redact_secrets(std::string("")), std::string(""));
    const std::string a = "token=";
    EXPECT_EQ(redact_secrets(a), a);
    const std::string b = "token=&x=1";
    EXPECT_EQ(redact_secrets(b), b);
}

TEST(CoreRedact, AuthorizationHeaderLine) {
    const std::string in = "Authorization: Basic dXNlcjpwYXNz\nnext";
    const std::string want = "Authorization: [REDACTED]\nnext";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, AuthorizationInJson) {
    const std::string in = R"({"Authorization":"Bearer x12345678","a":1})";
    const std::string want = R"({"Authorization":"[REDACTED]","a":1})";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, BearerInText) {
    const std::string in = "header Bearer abcdef123456 sent";
    const std::string want = "header Bearer [REDACTED] sent";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, ShortBearerWordKept) {
    const std::string in = "bearer of news";
    EXPECT_EQ(redact_secrets(in), in);
}

TEST(CoreRedact, SkKeyRedacted) {
    const std::string in = "key sk-ant-api03-abcdefghijklmnop end";
    const std::string want = "key [REDACTED] end";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, SkPrefixInsideWordKept) {
    const std::string a = "task-force-abcdefghijklmnopqrstuvwxyz";
    EXPECT_EQ(redact_secrets(a), a);
    const std::string b = "sk-1";
    EXPECT_EQ(redact_secrets(b), b);
}

TEST(CoreRedact, GoogleKeyRedacted) {
    const std::string in = std::string("use ") + "AIza" + std::string(35, 'x') + " now";
    const std::string want = "use [REDACTED] now";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, JsonFieldRedacted) {
    const std::string in = R"({"api_key":"abc123","model":"m"})";
    const std::string want = R"({"api_key":"[REDACTED]","model":"m"})";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, EnvStyleRedacted) {
    const std::string in = "OPENAI_API_KEY=abc123 next";
    const std::string want = "OPENAI_API_KEY=[REDACTED] next";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, UrlQueryRedacted) {
    const std::string in = "GET /v1?api_key=def&x=1";
    const std::string want = "GET /v1?api_key=[REDACTED]&x=1";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, SpacesAndQuotes) {
    const std::string in = R"(password = "hunter2" ok)";
    const std::string want = R"(password = "[REDACTED]" ok)";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, MultipleFields) {
    const std::string in = "token=a1 secret=b2";
    const std::string want = "token=[REDACTED] secret=[REDACTED]";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, Idempotent) {
    const std::string inputs[] = {
        R"({"Authorization":"Bearer x12345678","a":1})",
        "header Bearer abcdef123456 sent",
        R"({"api_key":"abc123","model":"m"})",
        "token=a1 secret=b2",
    };
    for (const std::string& in : inputs) {
        const std::string once = redact_secrets(in);
        EXPECT_EQ(redact_secrets(once), once);
    }
}

TEST(CoreRedact, VerticalTabAndFormFeedEndValue) {
    const std::string in1 = "token=abc\vnext";
    const std::string want1 = "token=[REDACTED]\vnext";
    EXPECT_EQ(redact_secrets(in1), want1);
    const std::string in2 = "secret=xyz\fmore";
    const std::string want2 = "secret=[REDACTED]\fmore";
    EXPECT_EQ(redact_secrets(in2), want2);
}

TEST(CoreRedact, SingleQuotedValue) {
    const std::string in1 = "password='hunter2' ok";
    const std::string want1 = "password='[REDACTED]' ok";
    EXPECT_EQ(redact_secrets(in1), want1);
    const std::string in2 = "'token': 'abc'";
    const std::string want2 = "'token': '[REDACTED]'";
    EXPECT_EQ(redact_secrets(in2), want2);
}

TEST(CoreRedact, TabSeparators) {
    const std::string in = "token\t=\tabc123\tnext";
    const std::string want = "token\t=\t[REDACTED]\tnext";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, AuthorizationEndsAtComma) {
    const std::string in = "Authorization: Basic abc, next";
    const std::string want = "Authorization: [REDACTED], next";
    EXPECT_EQ(redact_secrets(in), want);
}

TEST(CoreRedact, NameAtEndOfInputUnchanged) {
    const std::string a = "token";
    EXPECT_EQ(redact_secrets(a), a);
    const std::string b = "my password";
    EXPECT_EQ(redact_secrets(b), b);
    const std::string c = "Authorization:";
    EXPECT_EQ(redact_secrets(c), c);
}
