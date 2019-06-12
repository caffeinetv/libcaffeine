// Copyright 2019 Caffeine Inc. All rights reserved.

#include "StatsObserver.hpp"

#include "RestApi.hpp"
#include "Serialization.hpp"

namespace caff {

    StatsObserver::StatsObserver(SharedCredentials & sharedCredentials)
        : sharedCredentials(sharedCredentials), sendQueue("caffeine-stats") {}

    void StatsObserver::OnComplete(webrtc::StatsReports const & reports) {
        // StatsReports data disallows copying, so we serialize immediately and make the request(s) asynchronously
        Json toSend = serializeWebrtcStats(reports);

        sendQueue.PostTask([stats{ std::move(toSend) }, this] { sendWebrtcStats(sharedCredentials, stats); });
    }

} // namespace caff
