// Copyright 2019 Caffeine Inc. All rights reserved.

#ifndef LIBCAFFEINE_CAFFEINE_H
#define LIBCAFFEINE_CAFFEINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#   define CAFFEINE_LINKAGE extern "C"
#else
#   define CAFFEINE_LINKAGE
#endif

#if defined(LIBCAFFEINE_LIBRARY)
#   if defined(_WIN32)
#       define CAFFEINE_API CAFFEINE_LINKAGE __declspec(dllexport)
#   else
#       define CAFFEINE_API CAFFEINE_LINKAGE __attribute__((visibility("default")))
#   endif
#elif defined(_WIN32)
#   define CAFFEINE_API CAFFEINE_LINKAGE __declspec(dllimport)
#else
#   define CAFFEINE_API CAFFEINE_LINKAGE
#endif

/* Log severities */
typedef enum caff_Severity {
    caff_SeverityAll,
    caff_SeverityDebug,
    caff_SeverityWarning,
    caff_SeverityError,
    caff_SeverityNone,
    caff_SeverityLast = caff_SeverityNone
} caff_Severity;

/* Raw video formats supported by WebRTC */
typedef enum caff_VideoFormat {
    caff_VideoFormatUnknown,
    caff_VideoFormatI420,
    caff_VideoFormatIyuv,
    caff_VideoFormatRgb24,
    caff_VideoFormatAbgr,
    caff_VideoFormatArgb,
    caff_VideoFormatArgb4444,
    caff_VideoFormatRgb565,
    caff_VideoFormatArgb1555,
    caff_VideoFormatYuy2,
    caff_VideoFormatYv12,
    caff_VideoFormatUyvy,
    caff_VideoFormatMjpeg,
    caff_VideoFormatNv21,
    caff_VideoFormatNv12,
    caff_VideoFormatBgra,
    caff_VideoFormatLast = caff_VideoFormatBgra
} caff_VideoFormat;

/* Status results and errors */
typedef enum caff_Result {
    // General
    caff_ResultSuccess = 0,
    caff_ResultFailure,

    // Auth
    caff_ResultOldVersion,
    caff_ResultInfoIncorrect,
    caff_ResultLegalAcceptanceRequired,
    caff_ResultEmailVerificationRequired,
    caff_ResultMfaOtpRequired,
    caff_ResultMfaOtpIncorrect,

    // Broadcast
    caff_ResultNotSignedIn,
    caff_ResultOutOfCapacity,
    caff_ResultTakeover,
    caff_ResultAlreadyBroadcasting,
    caff_ResultDisconnected,
    caff_ResultBroadcastFailed,

    caff_ResultLast = caff_ResultBroadcastFailed
} caff_Result;

/* Content rating for broadcasts */
typedef enum caff_Rating {
    caff_RatingNone,
    caff_RatingSeventeenPlus,
    caff_RatingLast = caff_RatingSeventeenPlus
} caff_Rating;

/* Rough measure of connection quality, reported by Caffeine's back-end */
typedef enum caff_ConnectionQuality {
    caff_ConnectionQualityGood,
    caff_ConnectionQualityPoor,
    caff_ConnectionQualityUnknown
} caff_ConnectionQuality;

/* Opaque handle to Caffeine instance */
typedef struct caff_Instance * caff_InstanceHandle;

/* TODO: is there a way to encapsulate game process detection without touching on privacy issues? */
/* Supported game detection info */
typedef struct caff_GameInfo {
    char * id;
    char * name;
    char ** processNames;
    size_t numProcessNames;
} caff_GameInfo;

typedef struct caff_GameList {
    caff_GameInfo ** gameInfos;
    size_t numGames;
} caff_GameList;

/* Callback type for WebRTC log messages */
typedef void(*caff_LogCallback)(caff_Severity severity, char const * message);

/* Callback types for starting broadcast */
typedef void(*caff_BroadcastStartedCallback)(void * userData);
typedef void(*caff_BroadcastFailedCallback)(void * userData, caff_Result error);

/* Get string representation of error enum
 *
 * TODO: localization support
 *
 * error: the error code
 *
 * Returns a string representing the given error or "Unknown" if out of range
 */
CAFFEINE_API char const * caff_resultString(caff_Result error);

/* Initialize the Caffeine library
 *
 * This should only be called once during application startup. Subsequent calls will be ignored and return the same
 * result as the first call.
 *
 * minSeverity: sets the minimum severity for log messages to be reported.
 *     caff_SeverityDebug will enable all messages
 *     caff_SeverityNone will disable all messages
 * logCallback: if specified, this function will be called with all log messages _instead of_ outputing to stderr
 *
 * Returns: caff_ResultSuccess on success
 */
CAFFEINE_API caff_Result caff_initialize(caff_Severity minSeverity, caff_LogCallback logCallback);

/* Create a caffeine instance */
CAFFEINE_API caff_InstanceHandle caff_createInstance();

/* start broadcast on Caffeine
 *
 * Sets up the WebRTC connection with Caffeine asynchronously. Calls
 * into broadcastStartedCallback or broadcastFailedCallback with the result. This may
 * happen on a different thread than the caller.
 *
 * instanceHandle: handle to the caffeine instance from caff_initialize
 * userData: an optional pointer passed blindly to the callbacks
 * credentials: authentication credentials obtained from signIn/refreshAuth
 * username: the username of the broadcaster
 * title: the raw (untagged) broadcast title
 * rating: the content rating (17+ or none)
 * broadcastStartedCallback: called when broadcast successfully starts
 * broadcastFailedCallback: called when broadcast fails to start
 */
CAFFEINE_API caff_Result caff_startBroadcast(
    caff_InstanceHandle instanceHandle,
    void * userData,
    char const * title,
    caff_Rating rating,
    caff_BroadcastStartedCallback broadcastStartedCallback,
    caff_BroadcastFailedCallback broadcastFailedCallback);

/* TODO pass format, channels, etc */
CAFFEINE_API void caff_sendAudio(caff_InstanceHandle instanceHandle, uint8_t * samples, size_t samplesPerChannel);

/* todo pass pixel format */
CAFFEINE_API void caff_sendVideo(
    caff_InstanceHandle streamHandle,
    uint8_t const * frameData,
    size_t frameBytes,
    int32_t width,
    int32_t height,
    caff_VideoFormat format);

CAFFEINE_API caff_ConnectionQuality caff_getConnectionQuality(caff_InstanceHandle instanceHandle);

/* End a Caffeine broadcast
 *
 * This signals the server to end the broadcast and closes the RTC connection.
 */
CAFFEINE_API void caff_endBroadcast(caff_InstanceHandle instanceHandle);

/* Deinitialize Caffeine library
 *
 * This destroys the internal factory objects, shuts down worker threads, etc.
 *
 * instanceHandle: the instance handle received from caff_initialize. This
 *     handle will no longer be valid after the function returns.
 */
CAFFEINE_API void caff_freeInstance(caff_InstanceHandle * instanceHandle);

/* TODO: sort these into above, and document */
/* TODO: have the Instance own more of these to reduce API footprint */

CAFFEINE_API caff_Result caff_signIn(
    caff_InstanceHandle instanceHandle,
    char const * username,
    char const * password,
    char const * otp);

CAFFEINE_API caff_Result caff_refreshAuth(caff_InstanceHandle instanceHandle, char const * refreshToken);

CAFFEINE_API void caff_signOut(caff_InstanceHandle instanceHandle);

CAFFEINE_API bool caff_isSignedIn(caff_InstanceHandle instanceHandle);

CAFFEINE_API char const * caff_getRefreshToken(caff_InstanceHandle instanceHandle);
CAFFEINE_API char const * caff_getUsername(caff_InstanceHandle instanceHandle);
CAFFEINE_API char const * caff_getStageId(caff_InstanceHandle instanceHandle);
CAFFEINE_API bool caff_canBroadcast(caff_InstanceHandle instanceHandle);

CAFFEINE_API caff_GameList * caff_getGameList();
CAFFEINE_API void caff_freeGameInfo(caff_GameInfo ** info);
CAFFEINE_API void caff_freeGameList(caff_GameList ** games);

#endif /* LIBCAFFEINE_CAFFEINE_H */
