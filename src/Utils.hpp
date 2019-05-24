// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include <algorithm>
#include <cctype>
#include <locale>
#include <string>

namespace caff {
    inline bool isEmpty(char const * cstr) { return cstr == nullptr || cstr[0] == '\0'; }

    // Adapted from https://stackoverflow.com/a/217605
    // trim from start (in place)
    inline void ltrim(std::string & s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
    }

    // trim from end (in place)
    inline void rtrim(std::string & s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
    }

    // trim from both ends (in place)
    inline void trim(std::string & s) {
        ltrim(s);
        rtrim(s);
    }
} // namespace caff
