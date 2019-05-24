// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "caffeine.h"

#include "rtc_base/logging.h"

namespace caff {

    caff_LogLevel rtcToCaffLogLevel(rtc::LoggingSeverity rtcSeverity);
    rtc::LoggingSeverity caffToRtcSeverity(caff_LogLevel caffLogLevel);

    // Basic log sink class to call up into C clients
    class LogSink : public rtc::LogSink {
    public:
        LogSink(caff_LogCallback cb) : callback(cb) {}

        virtual void OnLogMessage(
                const std::string & message, rtc::LoggingSeverity severity, const char * tag) override;
        virtual void OnLogMessage(const std::string & message) override;

    private:
        caff_LogCallback callback;
    };

}  // namespace caff
