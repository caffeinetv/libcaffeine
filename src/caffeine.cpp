// Copyright 2019 Caffeine Inc. All rights reserved.

#define LIBCAFFEINE_LIBRARY

#include "caffeine.h"

#include <vector>

#include "interface.hpp"
#include "logsink.hpp"
#include "stream.hpp"

#include "rtc_base/ssladapter.h"

#ifdef _WIN32
#include "rtc_base/win32socketinit.h"
#endif

using namespace caff;

CAFFEINE_API char const* caff_error_string(caff_error error)
{
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

CAFFEINE_API caff_interface_handle caff_initialize(caff_log_callback log_callback, caff_log_severity min_severity)
{
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
        rtc::LogMessage::AddLogToStream(new LogSink(log_callback), static_cast<rtc::LoggingSeverity>(min_severity));

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

CAFFEINE_API bool caff_is_supported_version()
{
    return isSupportedVersion();
}

CAFFEINE_API caff_auth_response * caff_signin(char const * username, char const * password, char const * otp)
{
    return caffSignin(username, password, otp);
}

CAFFEINE_API caff_credentials_handle caff_refresh_auth(char const * refresh_token)
{
    RTC_DCHECK(refresh_token);
    return reinterpret_cast<caff_credentials_handle>(refreshAuth(refresh_token));
}

CAFFEINE_API void caff_free_credentials(caff_credentials_handle * credentials_handle)
{
    // TODO this may do bad things
    auto credentials = reinterpret_cast<Credentials **>(credentials_handle);
    if (credentials && credentials_handle) {
        delete *credentials;
        *credentials = nullptr;
    }
}

CAFFEINE_API void caff_free_auth_response(caff_auth_response ** auth_response)
{
    if (auth_response && *auth_response) {
        caff_free_credentials(&(*auth_response)->credentials);

        delete[](*auth_response)->next;
        delete[](*auth_response)->mfa_otp_method;

        delete *auth_response;
        *auth_response = nullptr;
    }
}

CAFFEINE_API char const * caff_refresh_token(caff_credentials_handle credentials_handle)
{
    RTC_DCHECK(credentials_handle);
    auto creds = reinterpret_cast<Credentials *>(credentials_handle);
    return creds->refreshToken.c_str();
}

CAFFEINE_API caff_user_info * caff_get_user_info(caff_credentials_handle credentials_handle)
{
    RTC_DCHECK(credentials_handle);
    return getUserInfo(reinterpret_cast<Credentials *>(credentials_handle));
}

CAFFEINE_API void caff_free_user_info(caff_user_info ** user_info)
{
    if (user_info && *user_info) {
        delete[](*user_info)->caid;
        delete[](*user_info)->username;
        delete[](*user_info)->stage_id;
        delete *user_info;
        *user_info = nullptr;
    }
}

CAFFEINE_API caff_games * caff_get_supported_games()
{
    return getSupportedGames();
}

CAFFEINE_API void caff_free_game_info(caff_game_info ** info)
{
    if (info && *info) {
        for (size_t i = 0; i < (*info)->num_process_names; ++i) {
            delete[](*info)->process_names[i];
            (*info)->process_names[i] = nullptr;
        }
        delete[](*info)->id;
        delete[](*info)->name;
        delete[](*info)->process_names;
        delete *info;
        *info = nullptr;
    }
}

CAFFEINE_API void caff_free_game_list(caff_games ** games)
{
    if (games && *games) {
        for (size_t i = 0; i < (*games)->num_games; ++i) {
            caff_free_game_info(&(*games)->game_infos[i]);
        }
        delete[](*games)->game_infos;
        delete *games;
        *games = nullptr;
    }
}

CAFFEINE_API caff_stream_handle caff_start_stream(
    caff_interface_handle interface_handle,
    void * user_data,
    caff_credentials_handle credentials_handle,
    char const * username,
    char const * title,
    caff_rating rating,
    caff_stream_started started_callback,
    caff_stream_failed failed_callback)
{
    RTC_DCHECK(interface_handle);
    RTC_DCHECK(started_callback);
    RTC_DCHECK(failed_callback);

    // Encapsulate void * inside lambdas, and other C++ -> C translations
    auto startedCallback = [=] { started_callback(user_data); };
    auto failedCallback = [=](caff_error error) {
        failed_callback(user_data, error);
    };

    auto interface = reinterpret_cast<Interface*>(interface_handle);
    auto credentials = reinterpret_cast<Credentials*>(credentials_handle);
    auto stream = interface->StartStream(credentials, username, title, rating, startedCallback, failedCallback);

    return reinterpret_cast<caff_stream_handle>(stream);
}

CAFFEINE_API void caff_send_audio(
    caff_stream_handle stream_handle,
    uint8_t* samples,
    size_t samples_per_channel)
{
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
    caff_format format)
{
    RTC_DCHECK(frame_data);
    RTC_DCHECK(frame_bytes);
    RTC_DCHECK(width);
    RTC_DCHECK(height);
    RTC_DCHECK(format);

    auto stream = reinterpret_cast<Stream*>(stream_handle);
    stream->SendVideo(frame_data, frame_bytes, width, height, format);
}

CAFFEINE_API caff_connection_quality caff_get_connection_quality(caff_stream_handle stream_handle)
{
    RTC_DCHECK(stream_handle);

    auto stream = reinterpret_cast<Stream*>(stream_handle);
    return stream->GetConnectionQuality();
}

CAFFEINE_API void caff_end_stream(caff_stream_handle* stream_handle)
{
    RTC_DCHECK(stream_handle);
    RTC_DCHECK(*stream_handle);
    auto stream = reinterpret_cast<Stream*>(*stream_handle);
    delete stream;
    *stream_handle = nullptr;
    RTC_LOG(LS_INFO) << "Caffeine stream ended";
}

CAFFEINE_API void caff_deinitialize(caff_interface_handle* interface_handle)
{
    RTC_DCHECK(interface_handle);
    RTC_DCHECK(*interface_handle);
    auto interface = reinterpret_cast<Interface*>(*interface_handle);
    delete interface;
    *interface_handle = nullptr;
    RTC_LOG(LS_INFO) << "Caffeine RTC deinitialized";
}
