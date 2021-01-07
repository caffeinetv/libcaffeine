// Copyright 2019 Caffeine Inc. All rights reserved.

#include "X264Encoder.hpp"

#include "ErrorLogging.hpp"

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "system_wrappers/include/metrics.h"

namespace caff {

    uint32_t const kLongStartcodeSize = 4;
    uint32_t const kShortStartcodeSize = 3;

    float const kHighQualityCrf = 18.0f;
    float const kNormalQualityCrf = 21.5f;

    // maximum height for high quality crf. above and equal to 720p will use normal quality
    int const kMaxFrameHeightHighQualityCrf = 720;

    // minimum bitrate for high quality crf. above 1000 kbps will use high quality crf if before 720p.
    int const kMinBitrateKbpsHighQualityCrf = 1000;

    // If the incoming fps is higher than 50, we will switch x264 to encode at 60fps. Otherwise, if the
    // incoming fps drops below 35, we will switch to encode at 30fps.
    // The default target framerate is 30.
    uint32_t const kFpsHighThreshold = 50;
    uint32_t const kFpsLowThreshold = 35;

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
            LOG_WARNING("Invalid frame type: %d", type);
            return webrtc::kEmptyFrame;
        }
    }

    // Helper method used by H264EncoderImpl::Encode.
    // Copies the encoded bytes from |info| to |encodedImage| and updates the
    // fragmentation information of |fragHeader|. The |encodedImage->_buffer| may
    // be deleted and reallocated if a bigger buffer is required.
    static void rtpFragmentize(
            webrtc::EncodedImage * encodedImage,
            std::unique_ptr<uint8_t[]> * encodedImageBuffer,
            webrtc::VideoFrameBuffer const & frameBuffer,
            uint32_t numNals,
            x264_nal_t * nal,
            webrtc::RTPFragmentationHeader * fragHeader) {
        // Calculate minimum buffer size required to hold encoded data.
        size_t requiredSize = 0;
        size_t fragmentsCount = 0;

        for (uint32_t idx = 0; idx < numNals; ++idx, ++fragmentsCount) {
            CHECK_POSITIVE(nal[idx].i_payload);
            // Ensure |requiredSize| will not overflow.
            CAFF_CHECK(nal[idx].i_payload <= std::numeric_limits<size_t>::max() - requiredSize);
            requiredSize += nal[idx].i_payload;
            // x264 uses 3 byte startcode instead, and uses 4 byte start codes only for
            // SPS and PPS. In the case of 3 byte start codes, we need to prepend an
            // extra 00 in front since WebRTC can only decode start codes with 00 00 00
            // 01.
            if (!nal[idx].b_long_startcode) {
                ++requiredSize;
            }
        }

        if (encodedImage->_size < requiredSize) {
            // Increase buffer size. Allocate enough to hold an unencoded image, this
            // should be more than enough to hold any encoded data of future frames of
            // the same size (avoiding possible future reallocation due to variations in
            // required size).
            encodedImage->_size =
                    webrtc::CalcBufferSize(webrtc::VideoType::kI420, frameBuffer.width(), frameBuffer.height());

            if (encodedImage->_size < requiredSize) {
                // Encoded data > unencoded data. Allocate required bytes.
                LOG_WARNING(
                        "Encoding produced more bytes than the original image data! Original: %zd, encoded: %zd",
                        encodedImage->_size,
                        requiredSize);
                encodedImage->_size = requiredSize;
            }
            encodedImage->_buffer = new uint8_t[encodedImage->_size];
            encodedImageBuffer->reset(encodedImage->_buffer);
        }

        // Iterate layers and NAL units, note each NAL unit as a fragment and copy
        // the data to |encodedImage->_buffer|.
        // Because x264 generates both 4 and 3 byte start code, we need to prepend 00
        // in the case where only a 3 byte start code is generated since WebRTC H.264
        // decoder requires 4 byte start codes only. For simplicity, we will just
        // prepend 00 00 00 01 in front of the NALU and remove the generated start
        // code by x264.
        uint8_t const startCode[4] = { 0, 0, 0, 1 };
        fragHeader->VerifyAndAllocateFragmentationHeader(fragmentsCount);
        size_t frag = 0;
        encodedImage->_length = 0;

        for (uint32_t idx = 0; idx < numNals; ++idx, ++frag) {
            CAFF_CHECK(nal[idx].i_payload >= 4);

            uint32_t offset = nal[idx].b_long_startcode ? kLongStartcodeSize : kShortStartcodeSize;
            uint32_t naluSize = nal[idx].i_payload - offset;

            // copy the start code first
            memcpy(encodedImage->_buffer + encodedImage->_length, startCode, sizeof(startCode));
            encodedImage->_length += sizeof(startCode);

            // copy the data without start code
            memcpy(encodedImage->_buffer + encodedImage->_length, nal[idx].p_payload + offset, naluSize);
            encodedImage->_length += naluSize;

            // offset to start of data. length is data without start code.
            fragHeader->fragmentationOffset[frag] = encodedImage->_length - naluSize;
            fragHeader->fragmentationLength[frag] = naluSize;
            fragHeader->fragmentationPlType[frag] = 0;
            fragHeader->fragmentationTimeDiff[frag] = 0;
        }
    }

    X264Encoder::X264Encoder(cricket::VideoCodec const & codec) {
        LOG_DEBUG("Using x264 encoder");
        std::string packetizationModeString;
        if (codec.GetParam(cricket::kH264FmtpPacketizationMode, &packetizationModeString) &&
            packetizationModeString == "1") {
            packetizationMode = webrtc::H264PacketizationMode::NonInterleaved;
        }
    }

    X264Encoder::~X264Encoder() { Release(); }

    static rtc::LoggingSeverity x264ToRtcLogLevel(int level) {
        switch (level) {
        case X264_LOG_INFO:
        case X264_LOG_DEBUG:
            return rtc::LS_INFO;
        case X264_LOG_WARNING:
            return rtc::LS_WARNING;
        case X264_LOG_ERROR:
            return rtc::LS_ERROR;
        default:
            return rtc::LS_NONE;
        }
    }

    static void x264Logger(void *, int level, char const * format, va_list args) {
        auto severity = x264ToRtcLogLevel(level);
        constexpr size_t bufferSize = 4096;
        char errorBuffer[bufferSize];
        std::vsnprintf(errorBuffer, bufferSize, format, args);
        RTC_LOG_V(severity) << errorBuffer;
    }

    int32_t X264Encoder::InitEncode(webrtc::VideoCodec const * codecSettings, int32_t numCores, size_t maxPayloadSize) {
        reportInit();

        if (!codecSettings || codecSettings->codecType != webrtc::kVideoCodecH264) {
            reportError();
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
        }

        if (codecSettings->maxFramerate == 0) {
            reportError();
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
        }

        if (codecSettings->width < 1 || codecSettings->height < 1) {
            reportError();
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
        }

        int32_t releaseRet = Release();
        if (releaseRet != WEBRTC_VIDEO_CODEC_OK) {
            reportError();
            return releaseRet;
        }
        CAFF_CHECK(!encoder);

        numberOfCores = numCores;

        width = codecSettings->width;
        height = codecSettings->height;
        maxFrameRate = static_cast<float>(codecSettings->maxFramerate);
        mode = codecSettings->mode;
        enableFrameDropping = codecSettings->H264().frameDroppingOn;
        keyFrameInterval = codecSettings->H264().keyFrameInterval;
        this->maxPayloadSize = maxPayloadSize;

        // encoder and codecSettings both use kbits/second
        targetKbps = codecSettings->maxBitrate;

        x264_param_t encoderParams;
        int32_t ret = x264_param_default_preset(&encoderParams, "veryfast", "zerolatency");
        if (0 != ret) {
            LOG_ERROR("Failed to create x264 param defaults");
            CAFF_CHECK(!encoder);
            reportError();
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        encoderParams.pf_log = x264Logger;
        encoderParams.i_log_level = X264_LOG_DEBUG;

        encoderParams.i_csp = X264_CSP_I420;
        encoderParams.b_annexb = 1;
        encoderParams.b_cabac = 0;

        encoderParams.i_width = width;
        encoderParams.i_height = height;
        encoderParams.i_fps_den = 1;
        encoderParams.i_fps_num = targetFps;
        encoderParams.i_level_idc = 42;

        // CPU settings
        // use single thread encoding since multi-threaded may cause some issue on
        // certain CPU and/or Windows
        encoderParams.i_threads = 1;
        encoderParams.b_sliced_threads = 0;

        // bitstream parameters
        encoderParams.b_intra_refresh = 1;
        encoderParams.i_bframe = 0;
        encoderParams.i_nal_hrd = X264_NAL_HRD_CBR;

        // rate control
        encoderParams.rc.i_rc_method = X264_RC_CRF;
        encoderParams.rc.i_bitrate = targetKbps;

        encoderParams.rc.f_rf_constant =
                (height >= kMaxFrameHeightHighQualityCrf || targetKbps < kMinBitrateKbpsHighQualityCrf)
                        ? kNormalQualityCrf
                        : kHighQualityCrf;
        encoderParams.rc.i_vbv_buffer_size = targetKbps;
        encoderParams.rc.i_vbv_max_bitrate = targetKbps;
        encoderParams.rc.f_vbv_buffer_init = 0.5;

        // if using single NALU, need to limit slice size.
        if (packetizationMode == webrtc::H264PacketizationMode::SingleNalUnit) {
            encoderParams.i_slice_max_size = static_cast<unsigned int>(maxPayloadSize);
        }

        x264_param_apply_profile(&encoderParams, "baseline");

        ret = x264_picture_alloc(&pictureIn, encoderParams.i_csp, encoderParams.i_width, encoderParams.i_height);
        if (0 != ret) {
            LOG_ERROR("Failed to allocate picture. errno: %d", ret);
            CAFF_CHECK(!encoder);
            reportError();
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        // picture setup
        pictureIn.img.i_plane = 3;

        // create encoder
        encoder = x264_encoder_open(&encoderParams);
        if (!encoder) {
            LOG_ERROR("Failed to open x264 encoder");

            x264_picture_clean(&pictureIn);
            encoder = nullptr;
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        // Initialize encoded image. Default buffer size: size of unencoded data.
        encodedImage._size =
                webrtc::CalcBufferSize(webrtc::VideoType::kI420, codecSettings->width, codecSettings->height);
        encodedImage._buffer = new uint8_t[encodedImage._size];
        encodedImageBuffer.reset(encodedImage._buffer);
        encodedImage._completeFrame = true;
        encodedImage._encodedWidth = 0;
        encodedImage._encodedHeight = 0;
        encodedImage._length = 0;

        frameCount = 0;
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t X264Encoder::Release() {
        if (encoder) {
            x264_encoder_close(encoder);
            encoder = nullptr;
        }

        encodedImage._buffer = nullptr;
        encodedImageBuffer.reset();

        frameCount = 0;
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t X264Encoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback * callback) {
        encodedImageCallback = callback;
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t X264Encoder::SetRateAllocation(webrtc::BitrateAllocation const & bitrateAllocation, uint32_t framerate) {
        if (bitrateAllocation.get_sum_bps() <= 0 || framerate <= 0) {
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
        }

        targetKbps = bitrateAllocation.get_sum_kbps();
        maxFrameRate = static_cast<float>(framerate);

        if (frameCount == 0 || nullptr == encoder) {
            // Don't adjust the framerate until we start encoding.
            LOG_DEBUG("No frames encoded or encoder setup yet. Ignore resetting encoder parameters.");
            return WEBRTC_VIDEO_CODEC_OK;
        }

        // If incoming fps is greater than high threshold and we still at 30fps, switch to 60fps.
        if (framerate > kFpsHighThreshold && targetFps == 30) {
            LOG_DEBUG("Resetting target framerate to 60");
            targetFps = 60;
        } else if (framerate < kFpsLowThreshold && targetFps == 60) {
            LOG_DEBUG("Resetting target framerate to 30");
            targetFps = 30;
        } else {
            // No changes needed.
            return WEBRTC_VIDEO_CODEC_OK;
        }

        x264_param_t encoderParams;
        x264_encoder_parameters(encoder, &encoderParams);

        // Reset the fps.
        encoderParams.i_fps_num = targetFps;

        int ret = x264_encoder_reconfig(encoder, &encoderParams);
        if (ret < 0) {
            LOG_ERROR("Failed to reconfig encoder; error code: %d", ret);
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
        }

        // TODO: need to change x264 target_bps on the fly?
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t X264Encoder::Encode(
            webrtc::VideoFrame const & inputFrame,
            webrtc::CodecSpecificInfo const * codecSpecificInfo,
            std::vector<webrtc::FrameType> const * frameTypes) {
        if (!isInitialized()) {
            reportError();
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        if (!encodedImageCallback) {
            LOG_WARNING("InitEncode() has been called, but a callback function "
                        "has not been set with RegisterEncodeCompleteCallback()");
            reportError();
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        // check if frame size has been changed by the AdaptFrame API.
        if (inputFrame.height() != height) {
            x264_param_t * encoderParams = nullptr;
            x264_encoder_parameters(encoder, encoderParams);

            if (nullptr != encoderParams) {
                encoderParams->rc.f_rf_constant =
                        (inputFrame.height() < kMaxFrameHeightHighQualityCrf) ? kHighQualityCrf : kNormalQualityCrf;

                int ret = x264_encoder_reconfig(encoder, encoderParams);
                if (ret < 0) {
                    LOG_ERROR("Failed to reconfig encoder; error code: %d", ret);
                }
            }
            height = inputFrame.height();
        }

        bool forceKeyFrame = false;
        if (frameTypes != nullptr) {
            // We only support a single stream.
            CAFF_CHECK(frameTypes->size() == 1);
            // Skip frame?
            if ((*frameTypes)[0] == webrtc::kEmptyFrame) {
                return WEBRTC_VIDEO_CODEC_OK;
            }
            // Force key frame?
            forceKeyFrame = (*frameTypes)[0] == webrtc::kVideoFrameKey;
        }

        x264_picture_t pictureOut = { 0 };

        pictureIn.i_type = forceKeyFrame ? X264_TYPE_IDR : X264_TYPE_AUTO;

        rtc::scoped_refptr<webrtc::I420BufferInterface const> frameBuffer = inputFrame.video_frame_buffer()->ToI420();

        pictureIn.img.plane[0] = const_cast<uint8_t *>(frameBuffer->DataY());
        pictureIn.img.plane[1] = const_cast<uint8_t *>(frameBuffer->DataU());
        pictureIn.img.plane[2] = const_cast<uint8_t *>(frameBuffer->DataV());
        pictureIn.i_pts = frameCount;

        x264_nal_t * nal = nullptr;
        int32_t numNals = 0;
        int32_t encodedFrameSize = x264_encoder_encode(encoder, &nal, &numNals, &pictureIn, &pictureOut);

        if (encodedFrameSize < 0) {
            LOG_ERROR("x264 frame encoding failed. x264_encoder_encode returned: %d", encodedFrameSize);
            reportError();
            x264_encoder_close(encoder);
            encoder = nullptr;
            x264_picture_clean(&pictureIn);
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        encodedImage._encodedWidth = frameBuffer->width();
        encodedImage._encodedHeight = frameBuffer->height();
        encodedImage._timeStamp = inputFrame.timestamp();
        encodedImage.ntp_time_ms_ = inputFrame.ntp_time_ms();
        encodedImage.capture_time_ms_ = inputFrame.render_time_ms();
        encodedImage.rotation_ = inputFrame.rotation();
        encodedImage.content_type_ = (mode == webrtc::VideoCodecMode::kScreensharing)
                                             ? webrtc::VideoContentType::SCREENSHARE
                                             : webrtc::VideoContentType::UNSPECIFIED;
        encodedImage.timing_.flags = webrtc::VideoSendTiming::kInvalid;
        encodedImage._frameType = ConvertToWebrtcFrameType(pictureOut.i_type);

        // Split encoded image up into fragments. This also updates |encodedImage|.
        webrtc::RTPFragmentationHeader fragHeader;
        rtpFragmentize(&encodedImage, &encodedImageBuffer, *frameBuffer, numNals, nal, &fragHeader);

        // Encoder can skip frames to save bandwidth in which case
        // |encodedImage._length| == 0.
        if (encodedImage._length > 0) {
            // Parse QP.
            bitstreamParser.ParseBitstream(encodedImage._buffer, encodedImage._length);
            bitstreamParser.GetLastSliceQp(&encodedImage.qp_);

            // Deliver encoded image.
            webrtc::CodecSpecificInfo codecSpecificInfo;
            codecSpecificInfo.codecType = webrtc::kVideoCodecH264;
            codecSpecificInfo.codecSpecific.H264.packetization_mode = packetizationMode;
            encodedImageCallback->OnEncodedImage(encodedImage, &codecSpecificInfo, &fragHeader);
        }
        ++frameCount;

        return WEBRTC_VIDEO_CODEC_OK;
    }

    char const * X264Encoder::ImplementationName() const {
        // implementation name.
        return "libx264";
    }

    bool X264Encoder::isInitialized() const {
        // return if initialized.
        return encoder != nullptr;
    }

    void X264Encoder::reportInit() {
        if (hasReportedInit) {
            return;
        }
        RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.X264Encoder.Event", kX264EncoderEventInit, kX264EncoderEventMax);
        hasReportedInit = true;
    }

    void X264Encoder::reportError() {
        if (hasReportedError) {
            return;
        }
        RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.X264Encoder.Event", kX264EncoderEventError, kX264EncoderEventMax);
        hasReportedError = true;
    }

    int32_t X264Encoder::SetChannelParameters(uint32_t packetLoss, int64_t rtt) { return WEBRTC_VIDEO_CODEC_OK; }

    // webrtc::VideoEncoder::ScalingSettings X264Encoder::GetScalingSettings() const {
    //     return webrtc::VideoEncoder::ScalingSettings::kOff;
    // }
} // namespace caff
