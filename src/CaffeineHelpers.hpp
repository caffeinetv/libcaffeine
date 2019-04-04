// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

// Used to protect against changes in the underlying enums
#define ASSERT_MATCH(left, right) static_assert( \
    (left) == static_cast<decltype(left)>((right)), \
    "Mismatch between " #left " and " #right)

// Used to protect against invalid enum values coming from C
#define IS_VALID_ENUM_VALUE(enumPrefix, value) \
    ((value) >= 0 && (value) <= (enumPrefix ## _Last))
