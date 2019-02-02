// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include <string>

#include "caffeine.h"

namespace caff {

// RAII friendly version of caff_ice_info
struct IceInfo {
  std::string sdp;
  std::string sdpMid;
  int sdpMLineIndex;

  IceInfo(std::string&& sdp, std::string&& sdpMid, int sdpMLineIndex)
      : sdp(std::move(sdp)), sdpMid(std::move(sdpMid)), sdpMLineIndex(sdpMLineIndex) {}

  operator caff_ice_info() const {
    return {sdp.c_str(), sdpMid.c_str(), sdpMLineIndex};
  }
};

}  // namespace caff
