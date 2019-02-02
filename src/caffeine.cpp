// Copyright 2019 Caffeine Inc. All rights reserved.

#define CAFFEINE_RTC_LIBRARY

#include "caffeine.h"

#include <vector>

#include "iceinfo.hpp"
#include "interface.hpp"
#include "logsink.hpp"
#include "stream.hpp"

#include "rtc_base/ssladapter.h"

#ifdef _WIN32
#include "rtc_base/win32socketinit.h"
#endif

namespace caff {
    extern "C" {

        CAFFEINE_API char const* caff_error_string(caff_error error) {
            switch (error) {
            case CAFF_ERROR_SDP_OFFER:
                return "Error making SDP offer";
            case CAFF_ERROR_SDP_ANSWER:
                return "Error reading SDP answer";
            case CAFF_ERROR_ICE_TRICKLE:
                return "Error during ICE negotiation";
            case CAFF_ERROR_DISCONNECTED:
                return "Disconnected from server";
            case CAFF_ERROR_TAKEOVER:
                return "Stream takeover";
            case CAFF_ERROR_UNKNOWN:
            default:
                return "Unknown error";
            }
        }

        CAFFEINE_API caff_interface_handle caff_initialize(
            caff_log_callback log_callback,
            caff_log_severity min_severity) {

            RTC_DCHECK(log_callback);

            // TODO: make this thread safe?
            static bool first_init = true;
            if (first_init) {
                // Set up logging
                rtc::LogMessage::LogThreads(true);
                rtc::LogMessage::LogTimestamps(true);

                // Send logs only to our log sink. Not to stderr, windows debugger, etc
                // rtc::LogMessage::LogToDebug(rtc::LoggingSeverity::LS_NONE);
                // rtc::LogMessage::SetLogToStderr(false);

                // TODO: Figure out why this log sink isn't working and uncomment above two
                // lines
                rtc::LogMessage::AddLogToStream(
                    new LogSink(log_callback),
                    static_cast<rtc::LoggingSeverity>(min_severity));

                // Initialize WebRTC

#ifdef _WIN32
                rtc::EnsureWinsockInit();
#endif
                if (!rtc::InitializeSSL()) {
                    RTC_LOG(LS_ERROR) << "Caffeine RTC failed to initialize SSL";
                    return nullptr;
                }

                RTC_LOG(LS_INFO) << "Caffeine RTC initialized";
                first_init = false;
            }

            auto interface = new Interface;
            return reinterpret_cast<caff_interface_handle>(interface);
        }

        CAFFEINE_API caff_stream_handle caff_start_stream(
            caff_interface_handle interface_handle,
            void* user_data,
            caff_offer_generated offer_generated_callback,
            caff_ice_gathered ice_gathered_callback,
            caff_stream_started started_callback,
            caff_stream_failed failed_callback) {

            RTC_DCHECK(interface_handle);
            RTC_DCHECK(offer_generated_callback);
            RTC_DCHECK(ice_gathered_callback);
            RTC_DCHECK(started_callback);
            RTC_DCHECK(failed_callback);

            // Encapsulate void * inside lambdas, and other C++ -> C translations
            auto offerGeneratedCallback = [=](std::string const& offer) {
                auto answer = offer_generated_callback(user_data, offer.c_str());
                return answer ? std::string(answer) : std::string();
            };

            auto iceGatheredCallback = [=](std::vector<IceInfo> const& candidatesVector) {
                std::vector<caff_ice_info> c_candidatesVector{ candidatesVector.begin(),
                                                              candidatesVector.end() };
                auto candidates =
                    reinterpret_cast<caff_ice_info const*>(&c_candidatesVector[0]);
                return ice_gathered_callback(user_data, candidates,
                    c_candidatesVector.size());
            };

            auto startedCallback = [=] { started_callback(user_data); };
            auto failedCallback = [=](caff_error error) {
                failed_callback(user_data, error);
            };

            auto interface = reinterpret_cast<Interface*>(interface_handle);
            auto stream =
                interface->StartStream(offerGeneratedCallback, iceGatheredCallback,
                    startedCallback, failedCallback);

            return reinterpret_cast<caff_stream_handle>(stream);
        }

        CAFFEINE_API void caff_send_audio(
            caff_stream_handle stream_handle,
            uint8_t* samples,
            size_t samples_per_channel) {

            RTC_DCHECK(stream_handle);
            RTC_DCHECK(samples);
            RTC_DCHECK(samples_per_channel);
            auto stream = reinterpret_cast<Stream*>(stream_handle);
            stream->SendAudio(samples, samples_per_channel);
        }

        CAFFEINE_API void caff_send_video(
            caff_stream_handle stream_handle,
            uint8_t const* frame_data,
            size_t frame_bytes,
            int32_t width,
            int32_t height,
            caff_format format) {

            RTC_DCHECK(frame_data);
            RTC_DCHECK(frame_bytes);
            RTC_DCHECK(width);
            RTC_DCHECK(height);
            RTC_DCHECK(format);

            auto stream = reinterpret_cast<Stream*>(stream_handle);
            stream->SendVideo(frame_data, frame_bytes, width, height, format);
        }

        CAFFEINE_API void caff_end_stream(caff_stream_handle* stream_handle) {
            RTC_DCHECK(stream_handle);
            RTC_DCHECK(*stream_handle);
            auto stream = reinterpret_cast<Stream*>(*stream_handle);
            delete stream;
            *stream_handle = nullptr;
            RTC_LOG(LS_INFO) << "Caffeine stream ended";
        }

        CAFFEINE_API void caff_deinitialize(caff_interface_handle* interface_handle) {
            RTC_DCHECK(interface_handle);
            RTC_DCHECK(*interface_handle);
            auto interface = reinterpret_cast<Interface*>(*interface_handle);
            delete interface;
            *interface_handle = nullptr;
            RTC_LOG(LS_INFO) << "Caffeine RTC deinitialized";
        }

    }
}  // namespace caff { extern "C" {
