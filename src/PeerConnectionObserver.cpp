// Copyright 2019 Caffeine Inc. All rights reserved.

#include "PeerConnectionObserver.hpp"

#include "ErrorLogging.hpp"

namespace caff {

    PeerConnectionObserver::PeerConnectionObserver(std::function<void(caff_Result)> failedCallback)
		: failedCallback(failedCallback)
	{}

    std::future<PeerConnectionObserver::Candidates const &> PeerConnectionObserver::getFuture() {
        return promise.get_future();
    }

    void PeerConnectionObserver::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState) {}

    void PeerConnectionObserver::OnRenegotiationNeeded() {}

    void PeerConnectionObserver::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) {
        using State = webrtc::PeerConnectionInterface::IceConnectionState;
        switch (newState) {
        case State::kIceConnectionFailed:
            LOG_DEBUG("ICE connection: failed");
            failedCallback(caff_ResultFailure);
            break;
        case State::kIceConnectionDisconnected:
            LOG_DEBUG("ICE connection: disconnected");
            failedCallback(caff_ResultFailure);
            break;
        default:
            break;
        }
    }

    void PeerConnectionObserver::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}

    void PeerConnectionObserver::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}

    void PeerConnectionObserver::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel) {}

    void PeerConnectionObserver::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState newState) {
        using State = webrtc::PeerConnectionInterface::IceGatheringState;
        switch (newState) {
        case State::kIceGatheringNew:
            LOG_DEBUG("ICE gathering: new");
            break;
        case State::kIceGatheringGathering:
            LOG_DEBUG("ICE gathering: gathering");
            break;
        case State::kIceGatheringComplete:
            LOG_DEBUG("ICE gathering: complete");
            promise.set_value(gatheredCandidates);
            break;
        default:
            LOG_WARNING("ICE gathering: unrecognized state [%lld]", static_cast<long long>(newState));
            break;
        }
    }

    void PeerConnectionObserver::OnIceCandidate(webrtc::IceCandidateInterface const * candidate) {
        CHECK_PTR(candidate);

        std::string sdpString;
        if (!candidate->ToString(&sdpString)) {
            LOG_ERROR("Failed parsing ICE candidate");
            return;
        }

        gatheredCandidates.emplace_back(
                IceInfo{ std::move(sdpString), candidate->sdp_mid(), candidate->sdp_mline_index() });
        LOG_DEBUG("ICE candidate added");
    }

} // namespace caff
