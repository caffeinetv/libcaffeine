// Copyright 2019 Caffeine Inc. All rights reserved.

#include "LogSink.hpp"

#include "Utils.hpp"

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

    void LogSink::OnLogMessage(std::string const & messageIn, rtc::LoggingSeverity severity) {
        if (!callback)
            return;

        // Remove trailing newline
        auto messageOut = messageIn;
        rtrim(messageOut);

        callback(rtcToCaffLogLevel(severity), messageOut.c_str());
    }

    void LogSink::OnLogMessage(std::string const & message) { OnLogMessage(message, rtc::LS_INFO); }
} // namespace caff
