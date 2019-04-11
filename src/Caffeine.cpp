// Copyright 2019 Caffeine Inc. All rights reserved.

#define LIBCAFFEINE_LIBRARY

#include "caffeine.h"

#include <vector>

#include "CaffeineHelpers.hpp"
#include "Instance.hpp"
#include "LogSink.hpp"
#include "Broadcast.hpp"

#include "rtc_base/ssladapter.h"

#ifdef _WIN32
#include "rtc_base/win32socketinit.h"
#endif

using namespace caff;

#define CATCHALL_RETURN(ret) \
    catch (...) { \
        RTC_LOG(LS_ERROR) << "Unhandled exception"; \
        return ret; \
    }

#define CATCHALL CATCHALL_RETURN()


CAFFEINE_API char const* caff_resultString(caff_Result error)
try {
    INVALID_ENUM_RETURN(nullptr, caff_Result, error);

    switch (error) {
    case caff_ResultSuccess:
        return "Success";
    case caff_ResultRequestFailed:
        return "Request failed";
    case caff_ResultInvalidArgument:
        return "Invalid argument";
    case caff_ResultInvalidState:
        return "Invalid state";

    case caff_ResultOldVersion:
        return "Old version";
    case caff_ResultInfoIncorrect:
        return "Incorrect signin info";
    case caff_ResultLegalAcceptanceRequired:
        return "User must accept Terms of Service";
    case caff_ResultEmailVerificationRequired:
        return "User must verify email address";
    case caff_ResultMfaOtpRequired:
        return "One-time password required";
    case caff_ResultMfaOtpIncorrect:
        return "One-time password incorrect";

    case caff_ResultNotSignedIn:
        return "Not signed in";
    case caff_ResultOutOfCapacity:
        return "Out of capacity";
    case caff_ResultTakeover:
        return "Broadcast takeover";
    case caff_ResultDisconnected:
        return "Disconnected from server";
    case caff_ResultBroadcastFailed:
        return "Broadcast failed";
    }
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API caff_InstanceHandle caff_initialize(caff_LogCallback logCallback, caff_LogLevel minSeverity)
try {
    RTC_DCHECK(logCallback);
    INVALID_ENUM_RETURN(nullptr, caff_LogLevel, minSeverity);

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
CATCHALL_RETURN(nullptr)


CAFFEINE_API caff_Result caff_signIn(
    caff_InstanceHandle instanceHandle,
    char const * username,
    char const * password,
    char const * otp)
try {
    RTC_DCHECK(instanceHandle);
    RTC_DCHECK(username);
    RTC_DCHECK(password);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->signIn(username, password, otp);
}
CATCHALL_RETURN(caff_ResultRequestFailed)


CAFFEINE_API caff_Result caff_refreshAuth(caff_InstanceHandle instanceHandle, char const * refreshToken)
try {
    RTC_DCHECK(instanceHandle);
    RTC_DCHECK(refreshToken);
    RTC_DCHECK(refreshToken[0]);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->refreshAuth(refreshToken);
}
CATCHALL_RETURN(caff_ResultRequestFailed)


CAFFEINE_API void caff_signOut(caff_InstanceHandle instanceHandle)
try {
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    instance->signOut();
}
CATCHALL


CAFFEINE_API bool caff_isSignedIn(caff_InstanceHandle instanceHandle)
try {
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->isSignedIn();
}
CATCHALL_RETURN(false)


CAFFEINE_API char const * caff_getRefreshToken(caff_InstanceHandle instanceHandle)
try {
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->getRefreshToken();
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API char const * caff_getUsername(caff_InstanceHandle instanceHandle)
try {
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->getUsername();
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API char const * caff_getStageId(caff_InstanceHandle instanceHandle)
try {
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->getStageId();
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API bool caff_canBroadcast(caff_InstanceHandle instanceHandle)
try {
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->canBroadcast();
}
CATCHALL_RETURN(false)


CAFFEINE_API caff_GameList * caff_getGameList()
try {
    return getSupportedGames();
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API void caff_freeGameInfo(caff_GameInfo ** info)
try {
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
CATCHALL


CAFFEINE_API void caff_freeGameList(caff_GameList ** games)
try {
    if (games && *games) {
        for (size_t i = 0; i < (*games)->numGames; ++i) {
            caff_freeGameInfo(&(*games)->gameInfos[i]);
        }
        delete[](*games)->gameInfos;
        delete *games;
        *games = nullptr;
    }
}
CATCHALL


CAFFEINE_API caff_Result caff_startBroadcast(
    caff_InstanceHandle instanceHandle,
    void * user_data,
    char const * title,
    caff_Rating rating,
    caff_BroadcastStartedCallback broadcastStartedCallback,
    caff_BroadcastFailedCallback broadcastFailedCallback)
try {
    RTC_DCHECK(instanceHandle);
    RTC_DCHECK(title);
    INVALID_ENUM_RETURN(caff_ResultInvalidArgument, caff_Rating, rating);
    RTC_DCHECK(broadcastStartedCallback);
    RTC_DCHECK(broadcastFailedCallback);

    // Encapsulate void * inside lambdas, and other C++ -> C translations
    auto startedCallback = [=] { broadcastStartedCallback(user_data); };
    auto failedCallback = [=](caff_Result error) {
        broadcastFailedCallback(user_data, error);
    };

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->startBroadcast(title, rating, startedCallback, failedCallback);
}
CATCHALL_RETURN(caff_ResultBroadcastFailed)


CAFFEINE_API void caff_sendAudio(
    caff_InstanceHandle instanceHandle,
    uint8_t * samples,
    size_t samplesPerChannel)
try {
    RTC_DCHECK(instanceHandle);
    RTC_DCHECK(samples);
    RTC_DCHECK(samplesPerChannel);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto broadcast = instance->getBroadcast();
    if (broadcast) {
        broadcast->sendAudio(samples, samplesPerChannel);
    }
}
CATCHALL


CAFFEINE_API void caff_sendVideo(
    caff_InstanceHandle instanceHandle,
    uint8_t const * frameData,
    size_t frameBytes,
    int32_t width,
    int32_t height,
    caff_VideoFormat format)
try {
    RTC_DCHECK(frameData);
    RTC_DCHECK(frameBytes);
    RTC_DCHECK(width);
    RTC_DCHECK(height);
    INVALID_ENUM_CHECK(caff_VideoFormat, format);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto broadcast = instance->getBroadcast();
    if (broadcast) {
        broadcast->sendVideo(frameData, frameBytes, width, height, format);
    }
}
CATCHALL


CAFFEINE_API caff_ConnectionQuality caff_getConnectionQuality(caff_InstanceHandle instanceHandle)
try {
    RTC_DCHECK(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto broadcast = instance->getBroadcast();
    if (broadcast) {
        return broadcast->getConnectionQuality();
    }
    else {
        return caff_ConnectionQualityUnknown;
    }
}
CATCHALL_RETURN(caff_ConnectionQualityUnknown)


CAFFEINE_API void caff_endBroadcast(caff_InstanceHandle instanceHandle)
try {
    RTC_DCHECK(instanceHandle);
    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    instance->endBroadcast();
    RTC_LOG(LS_INFO) << "Caffeine broadcast ended";
}
CATCHALL


CAFFEINE_API void caff_deinitialize(caff_InstanceHandle * instanceHandle)
try {
    RTC_DCHECK(instanceHandle);
    RTC_DCHECK(*instanceHandle);
    auto instance = reinterpret_cast<Instance *>(*instanceHandle);
    delete instance;
    *instanceHandle = nullptr;
    RTC_LOG(LS_INFO) << "Caffeine RTC deinitialized";
}
CATCHALL
