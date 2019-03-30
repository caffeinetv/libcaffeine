// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "caffeine.h"
#include "CaffeineHelpers.hpp"

#include "rtc_base/logging.h"

ASSERT_MATCH(caff_LogLevel_Sensitive, rtc::LoggingSeverity::LS_SENSITIVE);
ASSERT_MATCH(caff_LogLevel_Verbose, rtc::LoggingSeverity::LS_VERBOSE);
ASSERT_MATCH(caff_LogLevel_Info, rtc::LoggingSeverity::LS_INFO);
ASSERT_MATCH(caff_LogLevel_Warning, rtc::LoggingSeverity::LS_WARNING);
ASSERT_MATCH(caff_LogLevel_Error, rtc::LoggingSeverity::LS_ERROR);
ASSERT_MATCH(caff_LogLevel_None, rtc::LoggingSeverity::LS_NONE);

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
