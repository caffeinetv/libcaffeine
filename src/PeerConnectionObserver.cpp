// Copyright 2019 Caffeine Inc. All rights reserved.

#include "PeerConnectionObserver.hpp"

namespace caff {

    PeerConnectionObserver::PeerConnectionObserver() {}

    std::future<PeerConnectionObserver::Candidates const&> PeerConnectionObserver::getFuture() {
        return promise.get_future();
    }

    void PeerConnectionObserver::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState) {}

    void PeerConnectionObserver::OnRenegotiationNeeded() {}

    void PeerConnectionObserver::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) {}

    void PeerConnectionObserver::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}

    void PeerConnectionObserver::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}

    void PeerConnectionObserver::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel) {}

    void PeerConnectionObserver::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState newState) {
        using State = webrtc::PeerConnectionInterface::IceGatheringState;
        switch (newState) {
        case State::kIceGatheringNew:
            RTC_LOG(LS_INFO) << "ICE gathering: new";
            break;
        case State::kIceGatheringGathering:
            RTC_LOG(LS_INFO) << "ICE gathering: gathering";
            break;
        case State::kIceGatheringComplete:
            RTC_LOG(LS_INFO) << "ICE gathering: complete";
            promise.set_value(gatheredCandidates);
            break;
        default:
            RTC_LOG(LS_WARNING) << "ICE gathering: unrecognized state [" << newState << "]";
            break;
        }
    }

    void PeerConnectionObserver::OnIceCandidate(webrtc::IceCandidateInterface const* candidate) {
        RTC_DCHECK(candidate);

        std::string sdpString;
        if (!candidate->ToString(&sdpString)) {
            RTC_LOG(LS_ERROR) << "Failed parsing ICE candidate";
            return;
        }

        gatheredCandidates.emplace_back(IceInfo{
            std::move(sdpString),
            candidate->sdp_mid(),
            candidate->sdp_mline_index()
        });
        RTC_LOG(LS_INFO) << "ICE candidate added";
    }

}  // namespace caff
