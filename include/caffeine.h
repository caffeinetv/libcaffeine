// Copyright 2019 Caffeine Inc. All rights reserved.

#ifndef LIBCAFFEINE_CAFFEINE_H
#define LIBCAFFEINE_CAFFEINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define CAFFEINE_LINKAGE extern "C"
#else
#define CAFFEINE_LINKAGE
#endif

#if defined(LIBCAFFEINE_LIBRARY)
#if defined(_WIN32)
#define CAFFEINE_API CAFFEINE_LINKAGE __declspec(dllexport)
#else
#define CAFFEINE_API CAFFEINE_LINKAGE __attribute__((visibility("default")))
#endif
#elif defined(_WIN32)
#define CAFFEINE_API CAFFEINE_LINKAGE __declspec(dllimport)
#else
#define CAFFEINE_API CAFFEINE_LINKAGE
#endif

/* Log severities mapped from WebRTC */
typedef enum caff_LogLevel {
    caff_LogLevel_Sensitive,
    caff_LogLevel_Verbose,
    caff_LogLevel_Info,
    caff_LogLevel_Warning,
    caff_LogLevel_Error,
    caff_LogLevel_None,

    caff_LogLevel_End  /* Bounds check. Do not use. */
} caff_LogLevel;

/* Video formats mapped from WebRTC */
typedef enum caff_VideoFormat {
    caff_VideoFormat_Unknown,
    caff_VideoFormat_I420,
    caff_VideoFormat_Iyuv,
    caff_VideoFormat_Rgb24,
    caff_VideoFormat_Abgr,
    caff_VideoFormat_Argb,
    caff_VideoFormat_Argb4444,
    caff_VideoFormat_Rgb565,
    caff_VideoFormat_Argb1555,
    caff_VideoFormat_Yuy2,
    caff_VideoFormat_Yv12,
    caff_VideoFormat_Uyvy,
    caff_VideoFormat_Mjpeg,
    caff_VideoFormat_Nv21,
    caff_VideoFormat_Nv12,
    caff_VideoFormat_Bgra,

    caff_VideoFormat_End  /* Bounds check. Do not use. */
} caff_VideoFormat;

/* Errors generated by caffeine layer */
typedef enum caff_Error {
    caff_Error_NotSignedIn,
    caff_Error_SdpOffer,
    caff_Error_SdpAnswer,
    caff_Error_IceTrickle,
    caff_Error_Takeover,
    caff_Error_Disconnected,
    caff_Error_BroadcastFailed,
    caff_Error_Unknown,

    caff_Error_End  /* Bounds check. Do not use. */
} caff_Error;

/* Content rating for broadcasts */
typedef enum caff_Rating {
    caff_Rating_None,
    caff_Rating_SeventeenPlus,

    caff_Rating_End  /* Bounds check. Do not use. */
} caff_Rating;

/* Rough measure of connection quality, reported by Caffeine's back-end */
typedef enum caff_ConnectionQuality {
    caff_ConnectionQuality_Good,
    caff_ConnectionQuality_Bad,
    caff_ConnectionQuality_Unknown
} caff_ConnectionQuality;

/* Opaque handles to internal objects */
struct caff_Interface;
typedef struct caff_Interface* caff_InterfaceHandle;

struct caff_Stream;
typedef struct caff_Stream* caff_StreamHandle;

/* Result of sign in attempt */
typedef enum caff_AuthResult {
    caff_AuthResult_Success,
    caff_AuthResult_OldVersion,
    caff_AuthResult_InfoIncorrect,
    caff_AuthResult_LegalAcceptanceRequired,
    caff_AuthResult_EmailVerificationRequired,
    caff_AuthResult_MfaOtpRequired,
    caff_AuthResult_MfaOtpIncorrect,
    caff_AuthResult_RequestFailed
} caff_AuthResult;

/* TODO: is there a way to encapsulate this without touching on privacy issues? */

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
typedef void(*caff_LogCallback)(caff_LogLevel severity, char const* message);

/* Callback types for starting stream */
typedef void(*caff_StreamStartedCallback)(void* userData);
typedef void(*caff_StreamFailedCallback)(void* userData, caff_Error error);

/* Get string representation of error enum
 *
 * TODO: localization support
 *
 * error: the error code
 *
 * Returns a string representing the given error or "Unknown" if out of range
 */
CAFFEINE_API char const* caff_errorString(caff_Error error);

/* Initialize the Caffeine library
 *
 * logCallback: a function to call for WebRTC generated log messages
 * minSeverity: sets the lowest log severity required for the callback to fire
 *
 * Returns a handle to the caffeine management object to be passed into other
 * functions. If there is an error during initialization this will be NULL
 */
CAFFEINE_API caff_InterfaceHandle caff_initialize(caff_LogCallback logCallback, caff_LogLevel minSeverity);

/* start stream on Caffeine
 *
 * Sets up the WebRTC connection with Caffeine asynchronously. Calls
 * into streamStartedCallback or streamFailedCallback with the result. This may
 * happen on a different thread than the caller.
 *
 * interfaceHandle: handle to the caffeine interface from caff_initialize
 * userData: an optional pointer passed blindly to the callbacks
 * credentials: authentication credentials obtained from signin/refreshAuth
 * username: the username of the broadcaster
 * title: the raw (untagged) broadcast title
 * rating: the content rating (17+ or none)
 * streamStartedCallback: called when stream successfully starts
 * streamFailedCallback: called when stream fails to start
 *
 * returns a handle to the stream. If an error occurs before starting
 * the asynchronous operation, the handle will be NULL and the
 * streamFailedCallback will NOT be called
 */
CAFFEINE_API caff_StreamHandle caff_startStream(
    caff_InterfaceHandle interfaceHandle,
    void* userData,
    char const * title,
    caff_Rating rating,
    caff_StreamStartedCallback streamStartedCallback,
    caff_StreamFailedCallback streamFailedCallback);

/* TODO pass format, channels, etc */
CAFFEINE_API void caff_sendAudio(caff_StreamHandle streamHandle, uint8_t* samples, size_t samplesPerChannel);

/* todo pass pixel format */
CAFFEINE_API void caff_sendVideo(
    caff_StreamHandle streamHandle,
    uint8_t const* frameData,
    size_t frameBytes,
    int32_t width,
    int32_t height,
    caff_VideoFormat format);

CAFFEINE_API caff_ConnectionQuality caff_getConnectionQuality(caff_StreamHandle streamHandle);

/* End a Caffeine stream
 *
 * This signals the server to end the stream and closes the RTC connection.
 *
 * streamHandle: the stream handle received from caff_startStream.
 *     This handle will no longer be valid after the function returns.
 */
CAFFEINE_API void caff_endStream(caff_StreamHandle* streamHandle);

/* Deinitialize Caffeine library
 *
 * This destroys the internal factory objects, shuts down worker threads, etc.
 *
 * interfaceHandle: the interface handle received from caff_initialize. This
 *     handle will no longer be valid after the function returns.
 */
CAFFEINE_API void caff_deinitialize(caff_InterfaceHandle* interfaceHandle);

/* TODO: sort these into above, and document */
/* TODO: have the Interface own more of these to reduce API footprint */

CAFFEINE_API caff_AuthResult caff_signin(
    caff_InterfaceHandle interfaceHandle,
    char const * username,
    char const * password,
    char const * otp);

CAFFEINE_API caff_AuthResult caff_refreshAuth(caff_InterfaceHandle interfaceHandle, char const * refreshToken);

CAFFEINE_API void caff_signout(caff_InterfaceHandle interfaceHandle);

CAFFEINE_API bool caff_isSignedIn(caff_InterfaceHandle interfaceHandle);

CAFFEINE_API char const * caff_getRefreshToken(caff_InterfaceHandle interfaceHandle);
CAFFEINE_API char const * caff_getUsername(caff_InterfaceHandle interfaceHandle);
CAFFEINE_API char const * caff_getStageId(caff_InterfaceHandle interfaceHandle);
CAFFEINE_API bool caff_canBroadcast(caff_InterfaceHandle interfaceHandle);

CAFFEINE_API caff_GameList * caff_getGameList();
CAFFEINE_API void caff_freeGameInfo(caff_GameInfo ** info);
CAFFEINE_API void caff_freeGameList(caff_GameList ** games);

#endif /* LIBCAFFEINE_CAFFEINE_H */
