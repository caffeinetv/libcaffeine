// Copyright 2019 Caffeine Inc. All rights reserved.

#include "logsink.hpp"

#include <iostream>
#include <sstream>

namespace caff {
    void LogSink::OnLogMessage(const std::string& message, rtc::LoggingSeverity severity, const char* tag) {
        if (!callback) return;

        std::ostringstream outstr;
        outstr << '[' << tag << "]: " << message;
        auto finalMessage = outstr.str().c_str();

        callback(static_cast<caff_log_severity>(severity), finalMessage);
    }

    void LogSink::OnLogMessage(const std::string& message) {
        // TODO: do implement this because lower levels don't call the other one in every case
        // Alternatively, don't bother dealing with rtc::loggingseverity
    }
}  // namespace caff
