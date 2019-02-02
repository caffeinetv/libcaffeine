// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "iceinfo.hpp"

#include "api/peerconnectioninterface.h"

#include <future>

namespace caff {

    class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
        RTC_DISALLOW_COPY_AND_ASSIGN(PeerConnectionObserver);

    public:
        PeerConnectionObserver();

        using Candidates = std::vector<IceInfo>;
        std::future<Candidates const&> GetFuture();

        virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
        virtual void OnRenegotiationNeeded() override;
        virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
        virtual void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
        virtual void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
        virtual void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
        virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
        virtual void OnIceCandidate(webrtc::IceCandidateInterface const* candidate) override;

    private:
        std::promise<Candidates const&> promise;
        Candidates gatheredCandidates;
    };

}  // namespace caff
