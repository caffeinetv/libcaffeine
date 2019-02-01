/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logsink.hpp"

#include <iostream>
#include <sstream>

namespace caff {
void LogSink::OnLogMessage(const std::string& message,
                           rtc::LoggingSeverity severity,
                           const char* tag) {
  if (!callback)
    return;

  std::ostringstream outstr;
  outstr << '[' << tag << "]: " << message;
  auto finalMessage = outstr.str().c_str();

  callback(static_cast<caff_log_severity>(severity), finalMessage);
}

void LogSink::OnLogMessage(const std::string& message) {
  // unused helper called by default implementation of other overload
}
}  // namespace caff
