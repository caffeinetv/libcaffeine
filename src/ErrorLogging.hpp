// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include <cstring>
#include <stdexcept>
#include <string>

#include "rtc_base/logging.h"

// Format strings are easier on the eyes than c++ << streaming << operations
#define LOG_IMPL(severity, format, ...)                                                                                \
    constexpr size_t bufferSize = 4096;                                                                                \
    char errorBuffer[bufferSize];                                                                                      \
    std::snprintf(errorBuffer, bufferSize, format, ##__VA_ARGS__);                                                     \
    RTC_LOG(severity) << errorBuffer

#define LOG_DEBUG(format, ...)                                                                                         \
    do {                                                                                                               \
        LOG_IMPL(LS_INFO, format, ##__VA_ARGS__);                                                                      \
    } while (false)
#define LOG_WARNING(format, ...)                                                                                       \
    do {                                                                                                               \
        LOG_IMPL(LS_WARNING, format, ##__VA_ARGS__);                                                                   \
    } while (false)
#define LOG_ERROR(format, ...)                                                                                         \
    do {                                                                                                               \
        LOG_IMPL(LS_ERROR, format, ##__VA_ARGS__);                                                                     \
    } while (false)

#define CAFF_CHECK(condition)                                                                                          \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            LOG_IMPL(LS_ERROR, "Check failed [%s]: ", #condition);                                                     \
            throw std::logic_error(errorBuffer);                                                                       \
        }                                                                                                              \
    } while (false)

#define CHECK_PTR(ptr) CAFF_CHECK(ptr != nullptr)

#define CHECK_CSTR(cstr) CAFF_CHECK(cstr != nullptr && cstr[0] != '\0')

#define CHECK_POSITIVE(num) CAFF_CHECK(num > 0)

// Used to protect against changes in the underlying enums
#define ASSERT_MATCH(left, right)                                                                                      \
    static_assert((left) == static_cast<decltype(left)>((right)), "Mismatch between " #left " and " #right)

#define IS_VALID_ENUM_VALUE(prefix, value) ((value) >= 0 && (value) <= (prefix##Last))

#define CHECK_ENUM(prefix, value) CAFF_CHECK(IS_VALID_ENUM_VALUE(prefix, value))
