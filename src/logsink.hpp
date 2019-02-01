/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#include "caffeine.h"

#include "rtc_base/logging.h"

namespace {
// Protect against changes in underlying enum

constexpr bool isEnumEqual(caff_log_severity caff, rtc::LoggingSeverity rtc) {
  return caff == static_cast<caff_log_severity>(rtc);
}

static_assert(isEnumEqual(CAFF_LOG_SENSITIVE,
                          rtc::LoggingSeverity::LS_SENSITIVE),
              "mismatch between caff_log_severity and rtc::LoggingSeverity");
static_assert(isEnumEqual(CAFF_LOG_VERBOSE, rtc::LoggingSeverity::LS_VERBOSE),
              "mismatch between caff_log_severity and rtc::LoggingSeverity");
static_assert(isEnumEqual(CAFF_LOG_INFO, rtc::LoggingSeverity::LS_INFO),
              "mismatch between caff_log_severity and rtc::LoggingSeverity");
static_assert(isEnumEqual(CAFF_LOG_WARNING, rtc::LoggingSeverity::LS_WARNING),
              "mismatch between caff_log_severity and rtc::LoggingSeverity");
static_assert(isEnumEqual(CAFF_LOG_ERROR, rtc::LoggingSeverity::LS_ERROR),
              "mismatch between caff_log_severity and rtc::LoggingSeverity");
static_assert(isEnumEqual(CAFF_LOG_NONE, rtc::LoggingSeverity::LS_NONE),
              "mismatch between caff_log_severity and rtc::LoggingSeverity");
}  // anonymous namespace

namespace caff {

// Basic log sink class to call up into C clients
class LogSink : public rtc::LogSink {
 public:
  LogSink(caff_log_callback cb) : callback(cb) {}

  virtual void OnLogMessage(const std::string& message,
                            rtc::LoggingSeverity severity,
                            const char* tag) override;

  virtual void OnLogMessage(const std::string& message) override;

 private:
  caff_log_callback callback;
};

}  // namespace caff
