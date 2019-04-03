// Copyright 2019 Caffeine Inc. All rights reserved.

#define LIBCAFFEINE_LIBRARY

#include "caffeine.h"

#include <vector>

#include "Interface.hpp"
#include "LogSink.hpp"
#include "Stream.hpp"

#include "rtc_base/ssladapter.h"

#ifdef _WIN32
#include "rtc_base/win32socketinit.h"
#endif

using namespace caff;

CAFFEINE_API char const* caff_errorString(caff_Error error)
{
    RTC_DCHECK(error >= 0 && error < caff_ErrorEnd);

    switch (error) {
    case caff_ErrorNotSignedIn:
        return "Not signed in";
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
    case caff_ErrorEnd:
        return nullptr;
    }
}

CAFFEINE_API caff_InterfaceHandle caff_initialize(caff_LogCallback logCallback, caff_LogLevel minSeverity)
{
    RTC_DCHECK(logCallback);
    RTC_DCHECK(minSeverity >= 0 && minSeverity < caff_LogLevelEnd);

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

    auto interface = new Interface;
    return reinterpret_cast<caff_InterfaceHandle>(interface);
}

CAFFEINE_API caff_AuthResult caff_signin(
    caff_InterfaceHandle interfaceHandle,
    char const * username,
    char const * password,
    char const * otp)
{
    RTC_DCHECK(interfaceHandle);
    RTC_DCHECK(username);
    RTC_DCHECK(password);

    auto interface = reinterpret_cast<Interface*>(interfaceHandle);
    return interface->signin(username, password, otp);
}

CAFFEINE_API caff_AuthResult caff_refreshAuth(caff_InterfaceHandle interfaceHandle, char const * refreshToken)
{
    RTC_DCHECK(interfaceHandle);
    RTC_DCHECK(refreshToken);
    RTC_DCHECK(refreshToken[0]);

    auto interface = reinterpret_cast<Interface*>(interfaceHandle);
    return interface->refreshAuth(refreshToken);
}

CAFFEINE_API void caff_signout(caff_InterfaceHandle interfaceHandle)
{
    RTC_DCHECK(interfaceHandle);

    auto interface = reinterpret_cast<Interface*>(interfaceHandle);
    interface->signout();
}

CAFFEINE_API bool caff_isSignedIn(caff_InterfaceHandle interfaceHandle)
{
    RTC_DCHECK(interfaceHandle);

    auto interface = reinterpret_cast<Interface*>(interfaceHandle);
    return interface->isSignedIn();
}

CAFFEINE_API char const * caff_getRefreshToken(caff_InterfaceHandle interfaceHandle)
{
    RTC_DCHECK(interfaceHandle);

    auto interface = reinterpret_cast<Interface*>(interfaceHandle);
    return interface->getRefreshToken();
}

CAFFEINE_API char const * caff_getUsername(caff_InterfaceHandle interfaceHandle)
{
    RTC_DCHECK(interfaceHandle);

    auto interface = reinterpret_cast<Interface*>(interfaceHandle);
    return interface->getUsername();
}

CAFFEINE_API char const * caff_getStageId(caff_InterfaceHandle interfaceHandle)
{
    RTC_DCHECK(interfaceHandle);

    auto interface = reinterpret_cast<Interface*>(interfaceHandle);
    return interface->getStageId();
}

CAFFEINE_API bool caff_canBroadcast(caff_InterfaceHandle interfaceHandle)
{
    RTC_DCHECK(interfaceHandle);

    auto interface = reinterpret_cast<Interface*>(interfaceHandle);
    return interface->canBroadcast();
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

CAFFEINE_API caff_StreamHandle caff_startStream(
    caff_InterfaceHandle interfaceHandle,
    void * user_data,
    char const * title,
    caff_Rating rating,
    caff_StreamStartedCallback startedCallbackPtr,
    caff_StreamFailedCallback failedCallbackPtr)
{
    RTC_DCHECK(interfaceHandle);
    RTC_DCHECK(title);
    RTC_DCHECK(rating >= 0 && rating < caff_RatingEnd);
    RTC_DCHECK(startedCallbackPtr);
    RTC_DCHECK(failedCallbackPtr);

    // Encapsulate void * inside lambdas, and other C++ -> C translations
    auto startedCallback = [=] { startedCallbackPtr(user_data); };
    auto failedCallback = [=](caff_Error error) {
        failedCallbackPtr(user_data, error);
    };

    auto interface = reinterpret_cast<Interface *>(interfaceHandle);
    auto stream = interface->startStream(title, rating, startedCallback, failedCallback);

    return reinterpret_cast<caff_StreamHandle>(stream);
}

CAFFEINE_API void caff_sendAudio(
    caff_StreamHandle streamHandle,
    uint8_t* samples,
    size_t samples_per_channel)
{
    RTC_DCHECK(streamHandle);
    RTC_DCHECK(samples);
    RTC_DCHECK(samples_per_channel);
    auto stream = reinterpret_cast<Stream*>(streamHandle);
    stream->sendAudio(samples, samples_per_channel);
}

CAFFEINE_API void caff_sendVideo(
    caff_StreamHandle streamHandle,
    uint8_t const* frameData,
    size_t frameBytes,
    int32_t width,
    int32_t height,
    caff_VideoFormat format)
{
    RTC_DCHECK(frameData);
    RTC_DCHECK(frameBytes);
    RTC_DCHECK(width);
    RTC_DCHECK(height);
    RTC_DCHECK(format >= 0 && format < caff_VideoFormatEnd);

    auto stream = reinterpret_cast<Stream*>(streamHandle);
    stream->sendVideo(frameData, frameBytes, width, height, format);
}

CAFFEINE_API caff_ConnectionQuality caff_getConnectionQuality(caff_StreamHandle streamHandle)
{
    RTC_DCHECK(streamHandle);

    auto stream = reinterpret_cast<Stream*>(streamHandle);
    return stream->getConnectionQuality();
}

CAFFEINE_API void caff_endStream(caff_StreamHandle* streamHandle)
{
    RTC_DCHECK(streamHandle);
    RTC_DCHECK(*streamHandle);
    auto stream = reinterpret_cast<Stream*>(*streamHandle);
    delete stream;
    *streamHandle = nullptr;
    RTC_LOG(LS_INFO) << "Caffeine stream ended";
}

CAFFEINE_API void caff_deinitialize(caff_InterfaceHandle* interfaceHandle)
{
    RTC_DCHECK(interfaceHandle);
    RTC_DCHECK(*interfaceHandle);
    auto interface = reinterpret_cast<Interface*>(*interfaceHandle);
    delete interface;
    *interfaceHandle = nullptr;
    RTC_LOG(LS_INFO) << "Caffeine RTC deinitialized";
}
