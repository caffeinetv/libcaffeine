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

        virtual void OnLogMessage(std::string const & message, rtc::LoggingSeverity severity) override;
        virtual void OnLogMessage(std::string const & message) override;

    private:
        caff_LogCallback callback;
    };

}  // namespace caff
