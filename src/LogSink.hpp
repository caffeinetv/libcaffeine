// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "caffeine.h"
#include "CaffeineHelpers.hpp"

#include "rtc_base/logging.h"

ASSERT_MATCH(caff_LogLevelSensitive, rtc::LoggingSeverity::LS_SENSITIVE);
ASSERT_MATCH(caff_LogLevelVerbose, rtc::LoggingSeverity::LS_VERBOSE);
ASSERT_MATCH(caff_LogLevelInfo, rtc::LoggingSeverity::LS_INFO);
ASSERT_MATCH(caff_LogLevelWarning, rtc::LoggingSeverity::LS_WARNING);
ASSERT_MATCH(caff_LogLevelError, rtc::LoggingSeverity::LS_ERROR);
ASSERT_MATCH(caff_LogLevelNone, rtc::LoggingSeverity::LS_NONE);

namespace caff {

    // Basic log sink class to call up into C clients
    class LogSink : public rtc::LogSink {
    public:
        LogSink(caff_LogCallback cb) : callback(cb) {}

        virtual void OnLogMessage(const std::string& message, rtc::LoggingSeverity severity, const char* tag) override;
        virtual void OnLogMessage(const std::string& message) override;

    private:
        caff_LogCallback callback;
    };

}  // namespace caff
