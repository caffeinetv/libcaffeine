// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

// Used to protect against changes in the underlying enums
#define ASSERT_MATCH(left, right) static_assert( \
    (left) == static_cast<decltype(left)>((right)), \
    "Mismatch between " #left " and " #right)

// Used to protect against invalid enum values coming from C
#define IS_VALID_ENUM_VALUE(prefix, value) \
    ((value) >= 0 && (value) <= (prefix ## Last))

#define INVALID_ENUM_RETURN(ret, prefix, value) \
    do { \
        if (!IS_VALID_ENUM_VALUE(prefix, value)) { \
            RTC_LOG(LS_ERROR) << "Invalid " #prefix " enum value"; \
            return ret; \
        } \
    } while (false)

#define INVALID_ENUM_CHECK(prefix, value) INVALID_ENUM_RETURN( , prefix, value)
