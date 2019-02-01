#pragma once

#include <cstdint>

#include "common_video/h264/h264_bitstream_parser.h"
#include "modules/video_coding/codecs/h264/include/h264.h"

#include "x264.h"

namespace caff {

class X264Encoder : public webrtc::H264Encoder {
 public:
  explicit X264Encoder(cricket::VideoCodec const& codec);
  virtual ~X264Encoder();

  virtual int32_t InitEncode(webrtc::VideoCodec const* codec_settings,
                             int32_t number_of_cores,
                             size_t max_payload_size) override;
  virtual int32_t Release() override;

  virtual int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  virtual int32_t SetRateAllocation(
      const webrtc::BitrateAllocation& bitrate_allocation,
      uint32_t framerate) override;

  virtual int32_t Encode(
      const webrtc::VideoFrame& frame,
      const webrtc::CodecSpecificInfo* codec_specific_info,
      const std::vector<webrtc::FrameType>* frame_types) override;

  virtual const char* ImplementationName() const override;

  virtual webrtc::VideoEncoder::ScalingSettings GetScalingSettings()
      const override;

  virtual int32_t SetChannelParameters(uint32_t packet_loss,
                                       int64_t rtt) override;

 private:
  bool IsInitialized() const;

  webrtc::H264BitstreamParser _h264_bitstream_parser;

  void ReportInit();
  void ReportError();

  x264_t* _x264_encoder = nullptr;
  x264_picture_t _pic_in;

  int _width = 0;
  int _height = 0;
  float _max_frame_rate = 0.0f;
  uint32_t _target_kbps = 0;
  webrtc::VideoCodecMode _mode = webrtc::VideoCodecMode::kRealtimeVideo;

  // H.264 specifc parameters
  bool _frame_dropping_on = false;
  int _key_frame_interval = 0;
  webrtc::H264PacketizationMode _packetization_mode =
      webrtc::H264PacketizationMode::SingleNalUnit;

  size_t _max_payload_size = 0;
  int32_t _number_of_cores = 0;

  webrtc::EncodedImage _encoded_image;
  std::unique_ptr<uint8_t[]> _encoded_image_buffer;
  webrtc::EncodedImageCallback* _encoded_image_callback = nullptr;

  bool _has_reported_init = false;
  bool _has_reported_error = false;

  uint64_t _frame_count = 0;
};

}  // namespace caff
