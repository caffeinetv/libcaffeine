// Copyright 2019 Caffeine Inc. All rights reserved.

#define LIBCAFFEINE_LIBRARY

#include "caffeine.h"

#include <vector>

#include "CaffeineHelpers.hpp"
#include "Instance.hpp"
#include "LogSink.hpp"
#include "Stream.hpp"

#include "rtc_base/ssladapter.h"

#ifdef _WIN32
#include "rtc_base/win32socketinit.h"
#endif

using namespace caff;

CAFFEINE_API char const* caff_errorString(caff_Error error)
{
    if (!IS_VALID_ENUM_VALUE(caff_Error, error)) {
        RTC_LOG(LS_ERROR) << "Invalid caff_Error enum value";
        return nullptr;
    }

    switch (error) {
    case caff_ErrorNone:
        return "Success";
    case caff_ErrorInvalid:
        return "Invalid call";
    case caff_ErrorNotSignedIn:
        return "Not signed in";
    case caff_ErrorOutOfCapacity:
        return "Out of capacity";
    case caff_ErrorSdpOffer:
        return "Error making SDP offer";
    case caff_ErrorSdpAnswer:
        return "Error reading SDP answer";
    case caff_ErrorIceTrickle:
        return "Error during ICE negotiation";
    case caff_ErrorTakeover:
        return "Stream takeover";
    case caff_ErrorDisconnected:
        return "Disconnected from server";
    case caff_ErrorBroadcastFailed:
        return "Broadcast failed";
    case caff_ErrorUnknown:
        return "Unknown error";
    }
}

CAFFEINE_API caff_InstanceHandle caff_initialize(caff_LogCallback logCallback, caff_LogLevel minSeverity)
{
    RTC_DCHECK(logCallback);
    if (!IS_VALID_ENUM_VALUE(caff_LogLevel, minSeverity)) {
        RTC_LOG(LS_ERROR) << "Invalid caff_LogLevel enum value";
        return nullptr;
    }

    // TODO: make this thread safe
    static bool firstInit = true;
    if (firstInit) {
        // Set up logging
        rtc::LogMessage::LogThreads(true);
        rtc::LogMessage::LogTimestamps(true);

        // Send logs only to our log sink. Not to stderr, windows debugger, etc
        // rtc::LogMessage::LogToDebug(rtc::LoggingSeverity::LS_NONE);
        // rtc::LogMessage::SetLogToStderr(false);

        // TODO: Figure out why this log sink isn't working and uncomment above two
        // lines
        rtc::LogMessage::AddLogToStream(new LogSink(logCallback), static_cast<rtc::LoggingSeverity>(minSeverity));

        // Initialize WebRTC

#ifdef _WIN32
        rtc::EnsureWinsockInit();
#endif
        if (!rtc::InitializeSSL()) {
            RTC_LOG(LS_ERROR) << "Caffeine RTC failed to initialize SSL";
            return nullptr;
        }

        RTC_LOG(LS_INFO) << "Caffeine RTC initialized";
        firstInit = false;
    }

    auto instance = new Instance;
    return reinterpret_cast<caff_InstanceHandle>(instance);
}

CAFFEINE_API caff_AuthResult caff_signIn(
    caff_InstanceHandle instanceHandle,
    char const * username,
    char const * password,
    char const * otp)
{
    RTC_DCHECK(instanceHandle);
    RTC_DCHECK(username);
    RTC_DCHECK(password);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->signIn(username, password, otp);
}

CAFFEINE_API caff_AuthResult caff_refreshAuth(caff_InstanceHandle instanceHandle, char const * refreshToken)
{
    RTC_DCHECK(instanceHandle);
    RTC_DCHECK(refreshToken);
    RTC_DCHECK(refreshToken[0]);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->refreshAuth(refreshToken);
}

CAFFEINE_API void caff_signOut(caff_InstanceHandle instanceHandle)
{
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    instance->signOut();
}

CAFFEINE_API bool caff_isSignedIn(caff_InstanceHandle instanceHandle)
{
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->isSignedIn();
}

CAFFEINE_API char const * caff_getRefreshToken(caff_InstanceHandle instanceHandle)
{
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->getRefreshToken();
}

CAFFEINE_API char const * caff_getUsername(caff_InstanceHandle instanceHandle)
{
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->getUsername();
}

CAFFEINE_API char const * caff_getStageId(caff_InstanceHandle instanceHandle)
{
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->getStageId();
}

CAFFEINE_API bool caff_canBroadcast(caff_InstanceHandle instanceHandle)
{
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->canBroadcast();
}

CAFFEINE_API caff_GameList * caff_getGameList()
{
    return getSupportedGames();
}

CAFFEINE_API void caff_freeGameInfo(caff_GameInfo ** info)
{
    if (info && *info) {
        for (size_t i = 0; i < (*info)->numProcessNames; ++i) {
            delete[](*info)->processNames[i];
            (*info)->processNames[i] = nullptr;
        }
        delete[](*info)->id;
        delete[](*info)->name;
        delete[](*info)->processNames;
        delete *info;
        *info = nullptr;
    }
}

CAFFEINE_API void caff_freeGameList(caff_GameList ** games)
{
    if (games && *games) {
        for (size_t i = 0; i < (*games)->numGames; ++i) {
            caff_freeGameInfo(&(*games)->gameInfos[i]);
        }
        delete[](*games)->gameInfos;
        delete *games;
        *games = nullptr;
    }
}

CAFFEINE_API caff_Error caff_startStream(
    caff_InstanceHandle instanceHandle,
    void * user_data,
    char const * title,
    caff_Rating rating,
    caff_StreamStartedCallback startedCallbackPtr,
    caff_StreamFailedCallback failedCallbackPtr)
{
    RTC_DCHECK(instanceHandle);
    RTC_DCHECK(title);
    if (!IS_VALID_ENUM_VALUE(caff_Rating, rating)) {
        RTC_LOG(LS_ERROR) << "Invalid caff_Rating enum value";
        return caff_ErrorInvalid;
    }
    RTC_DCHECK(startedCallbackPtr);
    RTC_DCHECK(failedCallbackPtr);

    // Encapsulate void * inside lambdas, and other C++ -> C translations
    auto startedCallback = [=] { startedCallbackPtr(user_data); };
    auto failedCallback = [=](caff_Error error) {
        failedCallbackPtr(user_data, error);
    };

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->startStream(title, rating, startedCallback, failedCallback);
}

CAFFEINE_API void caff_sendAudio(
    caff_InstanceHandle instanceHandle,
    uint8_t * samples,
    size_t samples_per_channel)
{
    RTC_DCHECK(instanceHandle);
    RTC_DCHECK(samples);
    RTC_DCHECK(samples_per_channel);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto stream = instance->getStream();
    if (stream) {
        stream->sendAudio(samples, samples_per_channel);
    }
}

CAFFEINE_API void caff_sendVideo(
    caff_InstanceHandle instanceHandle,
    uint8_t const * frameData,
    size_t frameBytes,
    int32_t width,
    int32_t height,
    caff_VideoFormat format)
{
    RTC_DCHECK(frameData);
    RTC_DCHECK(frameBytes);
    RTC_DCHECK(width);
    RTC_DCHECK(height);
    if (!IS_VALID_ENUM_VALUE(caff_VideoFormat, format)) {
        RTC_LOG(LS_ERROR) << "Invalid caff_VideoFormat enum value";
        return;
    }

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto stream = instance->getStream();
    if (stream) {
        stream->sendVideo(frameData, frameBytes, width, height, format);
    }
}

CAFFEINE_API caff_ConnectionQuality caff_getConnectionQuality(caff_InstanceHandle instanceHandle)
{
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto stream = instance->getStream();
    if (stream) {
        return stream->getConnectionQuality();
    }
    else {
        return caff_ConnectionQualityUnknown;
    }
}

CAFFEINE_API void caff_endStream(caff_InstanceHandle instanceHandle)
{
    RTC_DCHECK(instanceHandle);
    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    instance->endStream();
    RTC_LOG(LS_INFO) << "Caffeine stream ended";
}

CAFFEINE_API void caff_deinitialize(caff_InstanceHandle * instanceHandle)
{
    RTC_DCHECK(instanceHandle);
    RTC_DCHECK(*instanceHandle);
    auto instance = reinterpret_cast<Instance *>(*instanceHandle);
    delete instance;
    *instanceHandle = nullptr;
    RTC_LOG(LS_INFO) << "Caffeine RTC deinitialized";
}
