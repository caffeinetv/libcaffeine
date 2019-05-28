// Copyright 2019 Caffeine Inc. All rights reserved.

#include "LogSink.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

namespace caff {
    caff_LogLevel rtcToCaffLogLevel(rtc::LoggingSeverity rtcSeverity) {
        switch (rtcSeverity) {
        case rtc::LS_SENSITIVE:
        case rtc::LS_VERBOSE:
            return caff_LogLevelAll;
        case rtc::LS_INFO:
            return caff_LogLevelDebug;
        case rtc::LS_WARNING:
            return caff_LogLevelWarning;
        case rtc::LS_ERROR:
            return caff_LogLevelError;
        case rtc::LS_NONE:
            return caff_LogLevelNone;
        }
    }

    rtc::LoggingSeverity caffToRtcSeverity(caff_LogLevel caffLogLevel) {
        switch (caffLogLevel) {
        case caff_LogLevelAll:
            return rtc::LS_SENSITIVE;
        case caff_LogLevelDebug:
            return rtc::LS_INFO;
        case caff_LogLevelWarning:
            return rtc::LS_WARNING;
        case caff_LogLevelError:
            return rtc::LS_ERROR;
        case caff_LogLevelNone:
            return rtc::LS_NONE;
        }
    }

    void LogSink::OnLogMessage(const std::string & messageIn, rtc::LoggingSeverity severity, const char * tag) {
        if (!callback)
            return;

        std::ostringstream outstr;
        if (tag && tag[0]) {
            outstr << '[' << tag << "]: ";
        }
        outstr << messageIn;
        auto messageOut = outstr.str();

        // Remove trailing newline
        messageOut.erase(
                std::find_if(messageOut.rbegin(), messageOut.rend(), [](int ch) { return !std::isspace(ch); }).base(),
                messageOut.end());

        callback(rtcToCaffLogLevel(severity), messageOut.c_str());
    }

    void LogSink::OnLogMessage(const std::string & message) { OnLogMessage(message, rtc::LS_INFO, nullptr); }
} // namespace caff
