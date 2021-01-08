// Copyright 2019 Caffeine Inc. All rights reserved.
#ifndef X264ENCODER_H_
#define X264ENCODER_H_

#include <cstdint>

#include "common_video/h264/h264_bitstream_parser.h"
#include "modules/video_coding/codecs/h264/include/h264.h"

#include "x264.h"

namespace caff {

    class X264Encoder : public webrtc::H264Encoder {
    public:
        explicit X264Encoder(cricket::VideoCodec const & codec);
        virtual ~X264Encoder();

        virtual int32_t InitEncode(
                webrtc::VideoCodec const * codec_settings, int32_t number_of_cores, size_t max_payload_size) override;
        virtual int32_t Release() override;

        virtual int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback * callback) override;
        virtual int32_t SetRateAllocation(
                webrtc::BitrateAllocation const & bitrate_allocation, uint32_t framerate) override;

        virtual int32_t Encode(
                webrtc::VideoFrame const & frame,
                webrtc::CodecSpecificInfo const * codec_specific_info,
                std::vector<webrtc::FrameType> const * frame_types) override;

        virtual char const * ImplementationName() const override;
        
        virtual webrtc::VideoEncoder::ScalingSettings GetScalingSettings() const override;

        virtual int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;

    private:
        bool isInitialized() const;

        webrtc::H264BitstreamParser bitstreamParser;

        void reportInit();
        void reportError();

        x264_t * encoder = nullptr;
        x264_picture_t pictureIn;

        int width = 0;
        int height = 0;
        float maxFrameRate = 0.0f;
        uint32_t targetKbps = 0;
        uint32_t targetFps = 30;
        webrtc::VideoCodecMode mode = webrtc::VideoCodecMode::kRealtimeVideo;

        // H.264 specifc parameters
        bool enableFrameDropping = false;
        int keyFrameInterval = 0;
        webrtc::H264PacketizationMode packetizationMode = webrtc::H264PacketizationMode::SingleNalUnit;

        size_t maxPayloadSize = 0;
        int32_t numberOfCores = 0;

        webrtc::EncodedImage encodedImage;
        std::unique_ptr<uint8_t[]> encodedImageBuffer;
        webrtc::EncodedImageCallback * encodedImageCallback = nullptr;

        bool hasReportedInit = false;
        bool hasReportedError = false;

        uint64_t frameCount = 0;
    };

}  // namespace caff
#endif
