// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "caffeine.h"
#include "CaffeineHelpers.hpp"

#include "rtc_base/logging.h"

ASSERT_MATCH(CAFF_LOG_SENSITIVE, rtc::LoggingSeverity::LS_SENSITIVE);
ASSERT_MATCH(CAFF_LOG_VERBOSE, rtc::LoggingSeverity::LS_VERBOSE);
ASSERT_MATCH(CAFF_LOG_INFO, rtc::LoggingSeverity::LS_INFO);
ASSERT_MATCH(CAFF_LOG_WARNING, rtc::LoggingSeverity::LS_WARNING);
ASSERT_MATCH(CAFF_LOG_ERROR, rtc::LoggingSeverity::LS_ERROR);
ASSERT_MATCH(CAFF_LOG_NONE, rtc::LoggingSeverity::LS_NONE);

namespace caff {

    // Basic log sink class to call up into C clients
    class LogSink : public rtc::LogSink {
    public:
        LogSink(caff_log_callback cb) : callback(cb) {}

        virtual void OnLogMessage(const std::string& message, rtc::LoggingSeverity severity, const char* tag) override;
        virtual void OnLogMessage(const std::string& message) override;

    private:
        caff_log_callback callback;
    };

}  // namespace caff
