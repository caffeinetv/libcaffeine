// Copyright 2019 Caffeine Inc. All rights reserved.

#include "X264Encoder.hpp"

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/metrics.h"

namespace caff {

    const uint32_t kLongStartcodeSize = 4;
    const uint32_t kShortStartcodeSize = 3;

    const float kHighQualityCrf = 18.0f;
    const float kNormalQualityCrf = 21.5f;

    // maximum height for high quality crf. above and equal to 720p will use normal
    // quality
    const int kMaxFrameHeightHighQualityCrf = 720;

    // minimum bitrate for high quality crf. above 1000 kbps will use high quality
    // crf if before 720p.
    const int kMinBitrateKbpsHighQualityCrf = 1000;

    // Used by histograms. Values of entries should not be changed.
    enum X264EncoderEvent {
        kX264EncoderEventInit = 0,
        kX264EncoderEventError = 1,
        kX264EncoderEventMax = 16,
    };

    webrtc::FrameType ConvertToWebrtcFrameType(int type) {
        switch (type) {
        case X264_TYPE_IDR:
            return webrtc::kVideoFrameKey;
        case X264_TYPE_I:
        case X264_TYPE_P:
        case X264_TYPE_B:
        case X264_TYPE_AUTO:
            return webrtc::kVideoFrameDelta;
        default:
            break;
        }
        RTC_NOTREACHED() << "Invalid frame type: " << type;
        return webrtc::kEmptyFrame;
    }

    // Helper method used by H264EncoderImpl::Encode.
    // Copies the encoded bytes from |info| to |encoded_image| and updates the
    // fragmentation information of |frag_header|. The |encoded_image->_buffer| may
    // be deleted and reallocated if a bigger buffer is required.
    static void RtpFragmentize(
        webrtc::EncodedImage* encoded_image,
        std::unique_ptr<uint8_t[]>* encoded_image_buffer,
        const webrtc::VideoFrameBuffer& frame_buffer,
        uint32_t n_nals,
        x264_nal_t* nal,
        webrtc::RTPFragmentationHeader* frag_header) {

        // Calculate minimum buffer size required to hold encoded data.
        size_t required_size = 0;
        size_t fragments_count = 0;

        for (uint32_t idx = 0; idx < n_nals; ++idx, ++fragments_count) {
            RTC_CHECK_GE(nal[idx].i_payload, 0);
            // Ensure |required_size| will not overflow.
            RTC_CHECK_LE(nal[idx].i_payload, std::numeric_limits<size_t>::max() - required_size);
            required_size += nal[idx].i_payload;
            // x264 uses 3 byte startcode instead, and uses 4 byte start codes only for
            // SPS and PPS. In the case of 3 byte start codes, we need to prepend an
            // extra 00 in front since WebRTC can only decode start codes with 00 00 00
            // 01.
            if (!nal[idx].b_long_startcode) {
                ++required_size;
            }
        }

        if (encoded_image->_size < required_size) {
            // Increase buffer size. Allocate enough to hold an unencoded image, this
            // should be more than enough to hold any encoded data of future frames of
            // the same size (avoiding possible future reallocation due to variations in
            // required size).
            encoded_image->_size = webrtc::CalcBufferSize(
                webrtc::VideoType::kI420, frame_buffer.width(), frame_buffer.height());

            if (encoded_image->_size < required_size) {
                // Encoded data > unencoded data. Allocate required bytes.
                RTC_LOG(LS_WARNING)
                    << "Encoding produced more bytes than the original image data! Original bytes: "
                    << encoded_image->_size << ", encoded bytes: " << required_size << ".";
                encoded_image->_size = required_size;
            }
            encoded_image->_buffer = new uint8_t[encoded_image->_size];
            encoded_image_buffer->reset(encoded_image->_buffer);
        }

        // Iterate layers and NAL units, note each NAL unit as a fragment and copy
        // the data to |encoded_image->_buffer|.
        // Because x264 generates both 4 and 3 byte start code, we need to prepend 00
        // in the case where only a 3 byte start code is generated since WebRTC H.264
        // decoder requires 4 byte start codes only. For simplicity, we will just
        // prepend 00 00 00 01 in front of the NALU and remove the generated start
        // code by x264.
        const uint8_t start_code[4] = { 0, 0, 0, 1 };
        frag_header->VerifyAndAllocateFragmentationHeader(fragments_count);
        size_t frag = 0;
        encoded_image->_length = 0;

        for (uint32_t idx = 0; idx < n_nals; ++idx, ++frag) {
            RTC_DCHECK_GE(nal[idx].i_payload, 4);

            uint32_t offset = nal[idx].b_long_startcode ? kLongStartcodeSize : kShortStartcodeSize;
            uint32_t nalu_size = nal[idx].i_payload - offset;

            // copy the start code first
            memcpy(encoded_image->_buffer + encoded_image->_length, start_code, sizeof(start_code));
            encoded_image->_length += sizeof(start_code);

            // copy the data without start code
            memcpy(encoded_image->_buffer + encoded_image->_length, nal[idx].p_payload + offset, nalu_size);
            encoded_image->_length += nalu_size;

            // offset to start of data. length is data without start code.
            frag_header->fragmentationOffset[frag] = encoded_image->_length - nalu_size;
            frag_header->fragmentationLength[frag] = nalu_size;
            frag_header->fragmentationPlType[frag] = 0;
            frag_header->fragmentationTimeDiff[frag] = 0;
        }
    }

    X264Encoder::X264Encoder(const cricket::VideoCodec& codec) {
        RTC_LOG(LS_INFO) << "Using x264 encoder";
        std::string packetization_mode_string;
        if (codec.GetParam(cricket::kH264FmtpPacketizationMode, &packetization_mode_string)
            && packetization_mode_string == "1") {
            _packetization_mode = webrtc::H264PacketizationMode::NonInterleaved;
        }
    }

    X264Encoder::~X264Encoder() {
        // Release x264_encoder
        Release();
    }

    int32_t X264Encoder::InitEncode(
        const webrtc::VideoCodec* codec_settings,
        int32_t number_of_cores,
        size_t max_payload_size) {
        ReportInit();

        if (!codec_settings || codec_settings->codecType != webrtc::kVideoCodecH264) {
            ReportError();
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
        }

        if (codec_settings->maxFramerate == 0) {
            ReportError();
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
        }

        if (codec_settings->width < 1 || codec_settings->height < 1) {
            ReportError();
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
        }

        int32_t release_ret = Release();
        if (release_ret != WEBRTC_VIDEO_CODEC_OK) {
            ReportError();
            return release_ret;
        }
        RTC_DCHECK(!_x264_encoder);

        _number_of_cores = number_of_cores;

        _width = codec_settings->width;
        _height = codec_settings->height;
        _max_frame_rate = static_cast<float>(codec_settings->maxFramerate);
        _mode = codec_settings->mode;
        _frame_dropping_on = codec_settings->H264().frameDroppingOn;
        _key_frame_interval = codec_settings->H264().keyFrameInterval;
        _max_payload_size = max_payload_size;

        // encoder and codec_settings both use kbits/second
        _target_kbps = codec_settings->maxBitrate;

        x264_param_t encoder_params;
        int32_t ret = x264_param_default_preset(&encoder_params, "veryfast", "zerolatency");
        if (0 != ret) {
            RTC_LOG(LS_ERROR) << "Failed to create x264 param defaults";
            RTC_DCHECK(!_x264_encoder);
            ReportError();
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        encoder_params.i_csp = X264_CSP_I420;
        encoder_params.b_annexb = 1;
        encoder_params.b_cabac = 0;

        encoder_params.i_width = _width;
        encoder_params.i_height = _height;
        encoder_params.i_fps_den = 1;
        encoder_params.i_fps_num = 30;
        encoder_params.i_level_idc = 31;

        // CPU settings
        // use single thread encoding since multi-threaded may cause some issue on
        // certain CPU and/or Windows
        encoder_params.i_threads = 1;
        encoder_params.b_sliced_threads = 0;

        // bitstream parameters
        encoder_params.b_intra_refresh = 1;
        encoder_params.i_bframe = 0;
        encoder_params.i_nal_hrd = X264_NAL_HRD_CBR;

        // rate control
        encoder_params.rc.i_rc_method = X264_RC_CRF;
        encoder_params.rc.i_bitrate = _target_kbps;

        encoder_params.rc.f_rf_constant =
            (_height >= kMaxFrameHeightHighQualityCrf ||
                _target_kbps < kMinBitrateKbpsHighQualityCrf)
            ? kNormalQualityCrf
            : kHighQualityCrf;
        encoder_params.rc.i_vbv_buffer_size = _target_kbps;
        encoder_params.rc.i_vbv_max_bitrate = _target_kbps;
        encoder_params.rc.f_vbv_buffer_init = 0.5;

        // if using single NALU, need to limit slice size.
        if (_packetization_mode == webrtc::H264PacketizationMode::SingleNalUnit) {
            encoder_params.i_slice_max_size = static_cast<unsigned int>(_max_payload_size);
        }

        x264_param_apply_profile(&encoder_params, "baseline");

        ret = x264_picture_alloc(&_pic_in, encoder_params.i_csp, encoder_params.i_width, encoder_params.i_height);
        if (0 != ret) {
            RTC_LOG(LS_ERROR) << "Failed to allocate picture. errno: " << ret;
            RTC_DCHECK(!_x264_encoder);
            ReportError();
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        // picture setup
        _pic_in.img.i_plane = 3;

        // create encoder
        _x264_encoder = x264_encoder_open(&encoder_params);
        if (!_x264_encoder) {
            RTC_LOG(LS_ERROR) << "Failed to open x264 encoder";

            x264_picture_clean(&_pic_in);
            _x264_encoder = nullptr;
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        // Initialize encoded image. Default buffer size: size of unencoded data.
        _encoded_image._size = webrtc::CalcBufferSize(
            webrtc::VideoType::kI420, codec_settings->width, codec_settings->height);
        _encoded_image._buffer = new uint8_t[_encoded_image._size];
        _encoded_image_buffer.reset(_encoded_image._buffer);
        _encoded_image._completeFrame = true;
        _encoded_image._encodedWidth = 0;
        _encoded_image._encodedHeight = 0;
        _encoded_image._length = 0;

        _frame_count = 0;
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t X264Encoder::Release() {
        if (_x264_encoder) {
            x264_encoder_close(_x264_encoder);
            _x264_encoder = nullptr;
        }

        _encoded_image._buffer = nullptr;
        _encoded_image_buffer.reset();

        _frame_count = 0;
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t X264Encoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) {
        _encoded_image_callback = callback;
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t X264Encoder::SetRateAllocation(const webrtc::BitrateAllocation& bitrate_allocation, uint32_t framerate) {
        if (bitrate_allocation.get_sum_bps() <= 0 || framerate <= 0) {
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
        }

        _target_kbps = bitrate_allocation.get_sum_kbps();
        _max_frame_rate = static_cast<float>(framerate);

        // TODO: need to change x264 target_bps and framerate on the fly?
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t X264Encoder::Encode(
        const webrtc::VideoFrame& input_frame,
        const webrtc::CodecSpecificInfo* codec_specific_info,
        const std::vector<webrtc::FrameType>* frame_types) {
        if (!IsInitialized()) {
            ReportError();
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        if (!_encoded_image_callback) {
            RTC_LOG(LS_WARNING)
                << "InitEncode() has been called, but a callback function "
                << "has not been set with RegisterEncodeCompleteCallback()";
            ReportError();
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        // check if frame size has been changed by the AdaptFrame API.
        if (input_frame.height() != _height) {
            x264_param_t* encoder_params = nullptr;
            x264_encoder_parameters(_x264_encoder, encoder_params);

            if (nullptr != encoder_params) {
                encoder_params->rc.f_rf_constant =
                    (input_frame.height() < kMaxFrameHeightHighQualityCrf)
                    ? kHighQualityCrf
                    : kNormalQualityCrf;

                int ret = x264_encoder_reconfig(_x264_encoder, encoder_params);
                if (ret < 0) {
                    RTC_LOG(LS_ERROR) << "Failed to reconfig encoder; error code: " << ret;
                }
            }
            _height = input_frame.height();
        }

        bool force_key_frame = false;
        if (frame_types != nullptr) {
            // We only support a single stream.
            RTC_DCHECK_EQ(frame_types->size(), 1);
            // Skip frame?
            if ((*frame_types)[0] == webrtc::kEmptyFrame) {
                return WEBRTC_VIDEO_CODEC_OK;
            }
            // Force key frame?
            force_key_frame = (*frame_types)[0] == webrtc::kVideoFrameKey;
        }

        x264_picture_t pic_out = { 0 };

        _pic_in.i_type = force_key_frame ? X264_TYPE_IDR : X264_TYPE_AUTO;

        rtc::scoped_refptr<const webrtc::I420BufferInterface> frame_buffer = input_frame.video_frame_buffer()->ToI420();

        _pic_in.img.plane[0] = const_cast<uint8_t*>(frame_buffer->DataY());
        _pic_in.img.plane[1] = const_cast<uint8_t*>(frame_buffer->DataU());
        _pic_in.img.plane[2] = const_cast<uint8_t*>(frame_buffer->DataV());
        _pic_in.i_pts = _frame_count;

        x264_nal_t* nal = nullptr;
        int32_t n_nal = 0;
        int32_t enc_frame_size = x264_encoder_encode(_x264_encoder, &nal, &n_nal, &_pic_in, &pic_out);

        if (enc_frame_size < 0) {
            RTC_LOG(LS_ERROR) << "x264 frame encoding failed. x264_encoder_encode returned: " << enc_frame_size;
            ReportError();
            x264_encoder_close(_x264_encoder);
            _x264_encoder = nullptr;
            x264_picture_clean(&_pic_in);
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        _encoded_image._encodedWidth = frame_buffer->width();
        _encoded_image._encodedHeight = frame_buffer->height();
        _encoded_image._timeStamp = input_frame.timestamp();
        _encoded_image.ntp_time_ms_ = input_frame.ntp_time_ms();
        _encoded_image.capture_time_ms_ = input_frame.render_time_ms();
        _encoded_image.rotation_ = input_frame.rotation();
        _encoded_image.content_type_ =
            (_mode == webrtc::VideoCodecMode::kScreensharing)
            ? webrtc::VideoContentType::SCREENSHARE
            : webrtc::VideoContentType::UNSPECIFIED;
        _encoded_image.timing_.flags = webrtc::VideoSendTiming::kInvalid;
        _encoded_image._frameType = ConvertToWebrtcFrameType(pic_out.i_type);

        // Split encoded image up into fragments. This also updates |_encoded_image|.
        webrtc::RTPFragmentationHeader frag_header;
        RtpFragmentize(&_encoded_image, &_encoded_image_buffer, *frame_buffer, n_nal, nal, &frag_header);

        // Encoder can skip frames to save bandwidth in which case
        // |_encoded_image._length| == 0.
        if (_encoded_image._length > 0) {
            // Parse QP.
            _h264_bitstream_parser.ParseBitstream(_encoded_image._buffer, _encoded_image._length);
            _h264_bitstream_parser.GetLastSliceQp(&_encoded_image.qp_);

            // Deliver encoded image.
            webrtc::CodecSpecificInfo codec_specific;
            codec_specific.codecType = webrtc::kVideoCodecH264;
            codec_specific.codecSpecific.H264.packetization_mode = _packetization_mode;
            _encoded_image_callback->OnEncodedImage(_encoded_image, &codec_specific, &frag_header);
        }
        ++_frame_count;

        return WEBRTC_VIDEO_CODEC_OK;
    }

    const char* X264Encoder::ImplementationName() const {
        // implementation name.
        return "libx264";
    }

    bool X264Encoder::IsInitialized() const {
        // return if initialized.
        return _x264_encoder != nullptr;
    }

    void X264Encoder::ReportInit() {
        if (_has_reported_init) {
            return;
        }
        RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.X264Encoder.Event", kX264EncoderEventInit, kX264EncoderEventMax);
        _has_reported_init = true;
    }

    void X264Encoder::ReportError() {
        if (_has_reported_error) {
            return;
        }
        RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.X264Encoder.Event", kX264EncoderEventError, kX264EncoderEventMax);
        _has_reported_error = true;
    }

    int32_t X264Encoder::SetChannelParameters(uint32_t packet_loss, int64_t rtt) {
        return WEBRTC_VIDEO_CODEC_OK;
    }

    webrtc::VideoEncoder::ScalingSettings X264Encoder::GetScalingSettings() const {
        return webrtc::VideoEncoder::ScalingSettings::kOff;
    }
}  // namespace caff
