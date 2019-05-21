// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "api/peerconnectioninterface.h"

namespace caff {
    class SharedCredentials;

    class StatsObserver : public webrtc::StatsObserver {
    public:
        StatsObserver(SharedCredentials & sharedCredentials);

        virtual void OnComplete(webrtc::StatsReports const & reports) override;

    private:
        SharedCredentials & sharedCredentials;
        rtc::TaskQueue sendQueue;
    };

} // namespace caff
