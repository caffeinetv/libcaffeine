// Copyright 2019 Caffeine Inc. All rights reserved.

#define LIBCAFFEINE_LIBRARY

#include "caffeine.h"

#include <vector>

#include "Broadcast.hpp"
#include "ErrorLogging.hpp"
#include "Instance.hpp"
#include "LogSink.hpp"
#include "Utils.hpp"

#include "rtc_base/ssladapter.h"

#ifdef _WIN32
#    include "rtc_base/win32socketinit.h"
#endif

using namespace caff;

#define CATCHALL_RETURN(ret)                                                                                           \
    catch (std::logic_error ex) {                                                                                      \
        LOG_ERROR("Logic error. See logs.");                                                                           \
        return ret;                                                                                                    \
    }                                                                                                                  \
    catch (...) {                                                                                                      \
        LOG_ERROR("Unhandled exception. See logs.");                                                                   \
        return ret;                                                                                                    \
    }

#define CATCHALL CATCHALL_RETURN()

CAFFEINE_API char const * caff_resultString(caff_Result result) try {
    CHECK_ENUM(caff_Result, result);

    switch (result) {
    case caff_ResultSuccess:
        return "Success";
    case caff_ResultFailure:
        return "Failure";

    case caff_ResultUsernameRequired:
        return "Username required";
    case caff_ResultPasswordRequired:
        return "Password required";
    case caff_ResultRefreshTokenRequired:
        return "Refresh token required";
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

    case caff_ResultOldVersion:
        return "Old version";
    case caff_ResultNotSignedIn:
        return "Not signed in";
    case caff_ResultOutOfCapacity:
        return "Out of capacity";
    case caff_ResultTakeover:
        return "Broadcast takeover";
    case caff_ResultAlreadyBroadcasting:
        return "Already broadcasting";
    case caff_ResultAspectTooNarrow:
        return "Aspect ratio too narrow";
    case caff_ResultAspectTooWide:
        return "Aspect ratio too wide";
    case caff_ResultDisconnected:
        return "Disconnected from server";
    case caff_ResultBroadcastFailed:
        return "Broadcast failed";
    case caff_ResultInternetDisconnected:
        return "Internet Connection Lost";
    case caff_ResultCaffeineUnreachable:
        return "Caffeine unreachable";
    }
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API caff_Result caff_initialize(
        char const * clientType,
        char const * clientVersion,
        caff_LogLevel minLogLevel,
        caff_LogCallback logCallback) try {
    CHECK_ENUM(caff_LogLevel, minLogLevel);

    // Thread-safe single-init
    static caff_Result result = ([=] {
        // Set up logging
        rtc::LogMessage::LogThreads(true);
        rtc::LogMessage::LogTimestamps(true);

        if (logCallback) {
            // Only send logs to the given callback
            rtc::LogMessage::LogToDebug(rtc::LS_NONE);
            rtc::LogMessage::SetLogToStderr(false);
            rtc::LogMessage::AddLogToStream(new LogSink(logCallback), caffToRtcSeverity(minLogLevel));
        } else {
#ifdef NDEBUG
            rtc::LogMessage::LogToDebug(rtc::LS_NONE);
#else
            rtc::LogMessage::LogToDebug(rtc::LS_INFO);
#endif
        }

        if (!clientType || !clientType[0]) {
            LOG_ERROR("Missing clientType");
            return caff_ResultFailure;
        }
        caff::clientType = clientType;

        if (!clientVersion || !clientVersion[0]) {
            LOG_ERROR("Missing clientVersion");
            return caff_ResultFailure;
        }
        caff::clientVersion = clientVersion;

        // Initialize WebRTC
#ifdef _WIN32
        rtc::EnsureWinsockInit();
#endif
        if (!rtc::InitializeSSL()) {
            LOG_ERROR("Libcaffeine failed to initialize SSL");
            return caff_ResultFailure;
        }

        LOG_DEBUG("Libcaffeine initialized");
        return caff_ResultSuccess;
    })();

    return result;
}
CATCHALL_RETURN(caff_ResultFailure)


CAFFEINE_API caff_Result caff_checkVersion() try { return checkVersion(); }
CATCHALL_RETURN(caff_ResultFailure)

CAFFEINE_API caff_Result caff_checkInternetConnection() try { return checkInternetConnection(); }
CATCHALL_RETURN(caff_ResultFailure)

CAFFEINE_API caff_Result caff_checkCaffeineConnection() try { return checkCaffeineConnection(); }
CATCHALL_RETURN(caff_ResultFailure)

CAFFEINE_API caff_InstanceHandle caff_createInstance() try {
    auto instance = new Instance;
    return reinterpret_cast<caff_InstanceHandle>(instance);
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API caff_Result caff_signIn(
        caff_InstanceHandle instanceHandle, char const * username, char const * password, char const * otp) try {
    CHECK_PTR(instanceHandle);
    if (isEmpty(username)) {
        return caff_ResultUsernameRequired;
    }
    if (isEmpty(password)) {
        return caff_ResultPasswordRequired;
    }
    // check internet here
    if(caff_checkInternetConnection() == caff_ResultInternetDisconnected) {
        return caff_ResultInternetDisconnected;
    }
    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->signIn(username, password, otp);
}
CATCHALL_RETURN(caff_ResultFailure)


CAFFEINE_API caff_Result caff_refreshAuth(caff_InstanceHandle instanceHandle, char const * refreshToken) try {
    CHECK_PTR(instanceHandle);
    if (isEmpty(refreshToken)) {
        return caff_ResultRefreshTokenRequired;
    }

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->refreshAuth(refreshToken);
}
CATCHALL_RETURN(caff_ResultFailure)


CAFFEINE_API void caff_signOut(caff_InstanceHandle instanceHandle) try {
    CHECK_PTR(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    instance->signOut();
}
CATCHALL


CAFFEINE_API bool caff_isSignedIn(caff_InstanceHandle instanceHandle) try {
    CHECK_PTR(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->isSignedIn();
}
CATCHALL_RETURN(false)


CAFFEINE_API char const * caff_getRefreshToken(caff_InstanceHandle instanceHandle) try {
    CHECK_PTR(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->getRefreshToken();
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API char const * caff_getAccessToken(caff_InstanceHandle instanceHandle) try {
    CHECK_PTR(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->getAccessToken();
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API char const * caff_getCredential(caff_InstanceHandle instanceHandle) try {
    CHECK_PTR(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->getCredential();
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API char const * caff_getUsername(caff_InstanceHandle instanceHandle) try {
    CHECK_PTR(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->getUsername();
}
CATCHALL_RETURN(nullptr)


CAFFEINE_API bool caff_canBroadcast(caff_InstanceHandle instanceHandle) try {
    CHECK_PTR(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->canBroadcast();
}
CATCHALL_RETURN(false)


CAFFEINE_API caff_Result
        caff_enumerateGames(caff_InstanceHandle instanceHandle, void * userData, caff_GameEnumerator enumerator) try {
    CHECK_PTR(instanceHandle);
    CHECK_PTR(enumerator);

    auto enumeratorWrapper = [=](char const * processName, char const * gameId, char const * gameName) {
        enumerator(userData, processName, gameId, gameName);
    };
    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->enumerateGames(enumeratorWrapper);
}
CATCHALL_RETURN(caff_ResultFailure)


CAFFEINE_API caff_Result caff_startBroadcast(
        caff_InstanceHandle instanceHandle,
        void * user_data,
        char const * title,
        caff_Rating rating,
        char const * gameId,
        caff_BroadcastStartedCallback broadcastStartedCallback,
        caff_BroadcastFailedCallback broadcastFailedCallback) try {
    CHECK_PTR(instanceHandle);
    CHECK_ENUM(caff_Rating, rating);
    CHECK_PTR(broadcastStartedCallback);
    CHECK_PTR(broadcastFailedCallback);

    std::string titleStr;
    if (title) {
        titleStr = title;
    }

    std::string gameIdStr;
    if (gameId) {
        gameIdStr = gameId;
    }

    // Encapsulate void * inside lambdas, and other C++ -> C translations
    auto startedCallback = [=] { broadcastStartedCallback(user_data); };
    auto failedCallback = [=](caff_Result error) { broadcastFailedCallback(user_data, error); };

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    return instance->startBroadcast(std::move(titleStr), rating, std::move(gameIdStr), startedCallback, failedCallback);
}
CATCHALL_RETURN(caff_ResultBroadcastFailed)


CAFFEINE_API void caff_setGameId(caff_InstanceHandle instanceHandle, char const * gameId) try {
    CHECK_PTR(instanceHandle);
    std::string idStr;
    if (gameId) {
        idStr = gameId;
    }

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto broadcast = instance->getBroadcast();
    if (broadcast) {
        broadcast->setGameId(std::move(idStr));
    } else {
        LOG_DEBUG("Setting game ID without an active broadcast. (This is probably OK if the stream just ended)");
    }
}
CATCHALL


CAFFEINE_API void caff_setTitle(caff_InstanceHandle instanceHandle, char const * title) try {
    CHECK_PTR(instanceHandle);
    std::string titleStr;
    if (title) {
        titleStr = title;
    }

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto broadcast = instance->getBroadcast();
    if (broadcast) {
        broadcast->setTitle(std::move(titleStr));
    } else {
        LOG_DEBUG("Setting title without an active broadcast. (This is probably OK if the stream just ended)");
    }
}
CATCHALL


CAFFEINE_API void caff_setRating(caff_InstanceHandle instanceHandle, caff_Rating rating) try {
    CHECK_PTR(instanceHandle);
    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto broadcast = instance->getBroadcast();
    if (broadcast) {
        broadcast->setRating(rating);
    } else {
        LOG_DEBUG("Setting rating without an active broadcast. (This is probably OK if the stream just ended)");
    }
}
CATCHALL


CAFFEINE_API void caff_sendAudio(caff_InstanceHandle instanceHandle, uint8_t * samples, size_t samplesPerChannel) try {
    CHECK_PTR(instanceHandle);
    CHECK_PTR(samples);
    CHECK_POSITIVE(samplesPerChannel);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto broadcast = instance->getBroadcast();
    if (broadcast) {
        broadcast->sendAudio(samples, samplesPerChannel);
    } else {
        LOG_DEBUG("Sending audio without an active broadcast. (This is probably OK if the stream just ended)");
    }
}
CATCHALL


CAFFEINE_API void caff_sendVideo(
        caff_InstanceHandle instanceHandle,
        caff_VideoFormat format,
        uint8_t const * frameData,
        size_t frameBytes,
        int32_t width,
        int32_t height,
        int64_t timestampMicros) try {
    CHECK_PTR(frameData);
    CHECK_POSITIVE(frameBytes);
    CHECK_POSITIVE(width);
    CHECK_POSITIVE(height);
    CHECK_ENUM(caff_VideoFormat, format);

    if (timestampMicros == caff_TimestampGenerate) {
        timestampMicros = rtc::TimeMicros();
    }
    auto timestamp = std::chrono::microseconds(timestampMicros);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto broadcast = instance->getBroadcast();
    if (broadcast) {
        broadcast->sendVideo(format, frameData, frameBytes, width, height, timestamp);
    } else {
        LOG_DEBUG("Sending video without an active broadcast. (This is probably OK if the stream just ended)");
    }
}
CATCHALL


CAFFEINE_API caff_ConnectionQuality caff_getConnectionQuality(caff_InstanceHandle instanceHandle) try {
    CHECK_PTR(instanceHandle);

    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    auto broadcast = instance->getBroadcast();
    if (broadcast) {
        return broadcast->getConnectionQuality();
    } else {
        return caff_ConnectionQualityUnknown;
    }
}
CATCHALL_RETURN(caff_ConnectionQualityUnknown)


CAFFEINE_API void caff_endBroadcast(caff_InstanceHandle instanceHandle) try {
    CHECK_PTR(instanceHandle);
    auto instance = reinterpret_cast<Instance *>(instanceHandle);
    instance->endBroadcast();
    LOG_DEBUG("Caffeine broadcast ended");
}
CATCHALL


CAFFEINE_API void caff_freeInstance(caff_InstanceHandle * instanceHandle) try {
    CHECK_PTR(instanceHandle);
    CHECK_PTR(*instanceHandle);
    auto instance = reinterpret_cast<Instance *>(*instanceHandle);
    delete instance;
    *instanceHandle = nullptr;
    LOG_DEBUG("Caffeine instance freed");
}
CATCHALL
