// Copyright 2019 Caffeine Inc. All rights reserved.

/*! \file caffeine.h

This is the public API for Libcaffeine
*/

#ifndef LIBCAFFEINE_CAFFEINE_H
#define LIBCAFFEINE_CAFFEINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LIBCAFFEINE_VERSION "0.2"

#ifdef __cplusplus
#    define CAFFEINE_LINKAGE extern "C"
#else
#    define CAFFEINE_LINKAGE
#endif

#if defined(LIBCAFFEINE_LIBRARY)
#    if defined(_WIN32)
#        define CAFFEINE_API CAFFEINE_LINKAGE __declspec(dllexport)
#    else
#        define CAFFEINE_API CAFFEINE_LINKAGE __attribute__((visibility("default")))
#    endif
#elif defined(_WIN32)
#    define CAFFEINE_API CAFFEINE_LINKAGE __declspec(dllimport)
#else
#    define CAFFEINE_API CAFFEINE_LINKAGE
#endif

//! Log level severities
/*!
These are used to filter out log messages based on their severity

\see caff_initialize()
\see caff_LogCallback
*/
typedef enum caff_LogLevel {
    caff_LogLevelAll,     //!< Extremely verbose, potentially containing sensitive information
    caff_LogLevelDebug,   //!< Verbose logging for debug purposes
    caff_LogLevelWarning, //!< Messages that may indicate a problem but are tolerable failures
    caff_LogLevelError,   //!< Messages indicating a failure state
    caff_LogLevelNone,    //!< Do not log any messages

    //! Used for bounds checking
    caff_LogLevelLast = caff_LogLevelNone
} caff_LogLevel;


//! Raw pixel formats supported by WebRTC
/*!
\see caff_sendVideo()
*/
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

    //! Used for bounds checking
    caff_VideoFormatLast = caff_VideoFormatBgra
} caff_VideoFormat;


//! Status results and errors returned by libcaffeine API functions
typedef enum caff_Result {
    caff_ResultSuccess = 0, //!< General success result
    caff_ResultFailure,     //!< General failure result

    // Auth
    caff_ResultUsernameRequired,          //!< Username is missing
    caff_ResultPasswordRequired,          //!< Password is missing
    caff_ResultRefreshTokenRequired,      //!< Refresh token is missing
    caff_ResultInfoIncorrect,             //!< Username/password or refresh token are incorrect
    caff_ResultLegalAcceptanceRequired,   //!< User must accept Caffeine Terms of Use
    caff_ResultEmailVerificationRequired, //!< User must verify email address
    caff_ResultMfaOtpRequired,            //!< User must enter a 2-factor one-time-password sent to their email
    caff_ResultMfaOtpIncorrect,           //!< The one-time-password provided was incorrect

    // Broadcast
    caff_ResultOldVersion,          //!< libcaffeine or client version is too old to broadcast successfully
    caff_ResultNotSignedIn,         //!< The instance must be signed in
    caff_ResultOutOfCapacity,       //!< Could not find capacity on the server to start a broadcast
    caff_ResultTakeover,            //!< The user ended the broadcast from another device or instance
    caff_ResultAlreadyBroadcasting, //!< A broadcast is already online
    caff_ResultAspectTooNarrow,     //!< The video being sent is too narrow for Caffeine
    caff_ResultAspectTooWide,       //!< The video being sent is too wide for Caffeine
    caff_ResultDisconnected,        //!< Broadcast has been disconnected
    caff_ResultBroadcastFailed,     //!< Broadcast failed to start

    //! Used for bounds checking
    caff_ResultLast = caff_ResultBroadcastFailed
} caff_Result;


//! Content rating for broadcasts
/*!
This is used to prefix the broadcast title with "[17+] " when selected. In the future this will likely be replaced with
a more generic tagging system

\see caff_startBroadcast()
\see caff_setRating()
*/
typedef enum caff_Rating {
    caff_RatingNone,          //!< Do not apply a rating tag
    caff_RatingSeventeenPlus, //!< Add [17+] tag

    //! Used for bounds checking
    caff_RatingLast = caff_RatingSeventeenPlus
} caff_Rating;


//! Indicator of connection quality, reported by Caffeine's back-end
/*!
\see caff_getConnectionQuality()
*/
typedef enum caff_ConnectionQuality {
    caff_ConnectionQualityGood,   //!< The connection quality is good
    caff_ConnectionQualityPoor,   //!< There is a significant amount of packet loss on the way to the server
    caff_ConnectionQualityUnknown //!< Connection quality has not been established
} caff_ConnectionQuality;


enum {
    //! Tells caff_sendVideo() to generate a frame timestamp from the current system time
    caff_TimestampGenerate = -1ll
};


//! Opaque handle to libcaffeine instance.
/*!
This is passed into most libcaffeine API functions

\see caff_initialize()
*/
typedef struct caff_Instance * caff_InstanceHandle;


//! Log message callback
/*!
This callback is used to pass log messages from libcaffeine into the application's logging facility.

\param severity is the severity of the log message
\param message is the log message itself

\see caff_initialize()
*/
typedef void (*caff_LogCallback)(caff_LogLevel severity, char const * message);


//! Game enumeration callback
/*!
This callback is used to iterate over the list of games supported by Caffeine.

\param userData is the pointer provided to caff_enumerateGames()
\param processName is the name of the game's binary with its file extension removed. For example, "WoW64.exe" would be
    reported as "WoW64".
\param gameId is Caffeine's unique ID for the game
\param gameName is the English name of the game as listed on the Caffeine website

\see caff_enumerateGames()
\see caff_setGameId()
*/
typedef void (*caff_GameEnumerator)(
        void * userData, char const * processName, char const * gameId, char const * gameName);


//! Broadcast start callback
/*!
This is called when the stream has started and the broadcast is going online.

\param userData is the pointer provided to caff_startBroadcast()

\see caff_startBroadcast()
*/
typedef void (*caff_BroadcastStartedCallback)(void * userData);


//! Broadcast failed callback
/*!
This is called when the stream failed

\param userData is the pointer provided to caff_startBroadcast()
\param error is the error result indicating the type of failure

\see caff_startBroadcast()
*/
typedef void (*caff_BroadcastFailedCallback)(void * userData, caff_Result error);


//! Get a string representation of an error enum
/*!
\param result the result code

\return a string representing the given result or "Unknown" if out of range
*/
CAFFEINE_API char const * caff_resultString(caff_Result result);


//! Initialize the Caffeine library
/*!
This should be called exactly once during application startup, before calling any other libcaffeine functions.
Subsequent calls will be ignored and return the same ::caff_Result as the first call.

\param clientType a simple identifier for the application. E.g. `"obs"` for OBS Studio
\param clientVersion the version string for the application. E.g. `"1.2.3"`
\param minLogLevel sets the minimum level for log messages to be reported
\param logCallback if provided, this function will be called with all log messages _instead of_ outputing to stderr

\return ::caff_ResultSuccess or ::caff_ResultFailure

\see caff_LogLevel
\see caff_LogCallback
*/
CAFFEINE_API caff_Result caff_initialize(
        char const * clientType, char const * clientVersion, caff_LogLevel minLogLevel, caff_LogCallback logCallback);


//! Check if this version of the application and libcaffeine are still supported
/*!
This will send a request to Caffeine with the client type, client version, and libcaffeine version. An application can
use this to prompt the user to update.

TODO: differentiate clientVersion failure from libcaffeine version failure

\return
    - ::caff_ResultSuccess if the check succeeds
    - ::caff_ResultOldVersion if the check fails
    - ::caff_ResultFailure if the check could not complete
*/
CAFFEINE_API caff_Result caff_checkVersion();


//! Create a Caffeine instance
/*!
The instance manages authentication and the state of the broadcast, and is passed into most other API functions. An
application usually only needs a single instance, but more than one can be created and will not interfere.

When the application no longer needs the instance (e.g. on shutdown), call caff_freeInstance().

\return an opaque handle to the instance

\see caff_freeInstance()
*/
CAFFEINE_API caff_InstanceHandle caff_createInstance();


//! Enumerate the supported games list
/*!
The supplied enumerator will be called synchronously, once for each process name/game ID combination in Caffeine's
supported games list. 

This is used by an application to decide which game ID to set on the broadcast. The application is responsible for
determining what game is running, e.g. by monitoring the foreground window process.

\param instanceHandle the handle returned by caff_createInstance()
\param userData an arbitrary pointer passed unmodified to the enumerator for context
\param enumerator the enumerator callback

\return ::caff_ResultFailure if the game list failed to download, otherwise ::caff_ResultSuccess

\see caff_startBroadcast()
\see caff_setGameId()
*/
CAFFEINE_API caff_Result
        caff_enumerateGames(caff_InstanceHandle instanceHandle, void * userData, caff_GameEnumerator enumerator);


//! First-time sign-in with username and password
/*!
This will attempt to authenticate the user. The result indicates further actions the user must take, if possible.

The first time this function is called, \p otp should be `NULL`. If the user has 2-factor authentication enabled, an
e-mail will be sent to the user with a one-time password. To finish signing in, call caff_signIn() again with the same
\p username, \p password, and \p otp.

\param instanceHandle the handle returned by caff_createInstance()
\param username the username attempting to sign in
\param password the user's password
\param otp (optional) one-time-password for 2-factor authentication enabled

\return
        - ::caff_ResultSuccess upon successful sign-in
        - ::caff_ResultFailure for unknown/unexpected errors
        - ::caff_ResultUsernameRequired
        - ::caff_ResultPasswordRequired
        - ::caff_ResultInfoIncorrect
        - ::caff_ResultLegalAcceptanceRequired
        - ::caff_ResultEmailVerificationRequired
        - ::caff_ResultMfaOtpRequired
        - ::caff_ResultMfaOtpIncorrect
        - ::caff_ResultAlreadyBroadcasting

\see caff_Result
\see caff_getRefreshToken()
\see caff_refreshAuth()
*/
CAFFEINE_API caff_Result
        caff_signIn(caff_InstanceHandle instanceHandle, char const * username, char const * password, char const * otp);


//! Get an authentication refresh token
/*!
Once the instance is signed in (caff_signIn()), this will provide a refresh token which can be used for subsequent
authentications. This token is intended to be stored by the application to provide "stay signed in" behavior.

\param instanceHandle the handle returned by caff_createInstance()

\return the refresh token if signed in, otherwise NULL

\see caff_refreshAuth()
*/
CAFFEINE_API char const * caff_getRefreshToken(caff_InstanceHandle instanceHandle);


//! Sign in with refresh token
/*!
This attempts to authenticate using the provided refresh token. If this fails, the user will need to sign in again with
username and password via caff_signIn().

\param instanceHandle the handle returned by caff_createInstance()
\param refreshToken the refresh token returned by caff_getRefreshToken() after a previous sign-in

\return
    - ::caff_ResultSuccess
    - ::caff_ResultInfoIncorrect
    - ::caff_ResultRefreshTokenRequired
    - ::caff_ResultFailure
    - ::caff_ResultAlreadyBroadcasting

\see caff_signIn()
\see caff_getRefreshToken()
*/
CAFFEINE_API caff_Result caff_refreshAuth(caff_InstanceHandle instanceHandle, char const * refreshToken);


//! Determine if the instance is signed in
/*!
\param instanceHandle the handle returned by caff_createInstance()

\return `true` if the instance has successfully been signed in with either caff_signIn() or caff_refreshAuth(),
        otherwise `false`
*/
CAFFEINE_API bool caff_isSignedIn(caff_InstanceHandle instanceHandle);


//! Get the signed-in username
/*!
\param instanceHandle the handle returned by caff_createInstance()

\return the username if signed in, otherwise `NULL`
*/
CAFFEINE_API char const * caff_getUsername(caff_InstanceHandle instanceHandle);


//! Determine if the user is allowed to broadcast
/*!
\param instanceHandle the handle returned by caff_createInstance()

\return `false` if the user's account is flagged as unable to broadcast. Otherwise `true`
*/
CAFFEINE_API bool caff_canBroadcast(caff_InstanceHandle instanceHandle);


//! Start a broadcast on Caffeine
/*!
This will initiate the broadcast startup process on a new thread as long as the instance is signed in and there isn't
already a broadcast in progress. When the broadcast has started, \p broadcastStartedCallback will be called from that
thread, signalling that the application should start sending audio & video frames.

If there is an error that either prevents the broadcast from starting or interrupts a broadcast in progress, then
\p broadcastFailedCallback will be called instead.

\param instanceHandle the handle returned by caff_createInstance()
\param userData an optional pointer passed unmodified to the callbacks
\param title the raw (untagged) broadcast title
\param rating the content rating
\param gameId (optional) currently active game, for showing icons and categorizing on the Caffeine website
\param broadcastStartedCallback called when broadcast successfully starts
\param broadcastFailedCallback called when broadcast fails

\return
        - ::caff_ResultAlreadyBroadcasting if the instance already has a broadcast in progress
        - ::caff_ResultNotSignedIn if the instance has not been authenticated
        - ::caff_ResultOldVersion if the version of libcaffeine or the application are no longer supported
        - ::caff_ResultSuccess if the broadcast thread has started

\see caff_setTitle()
\see caff_setRating()
\see caff_setGameId()
\see caff_sendAudio()
\see caff_sendVideo()
\see caff_endBroadcast()
*/
CAFFEINE_API caff_Result caff_startBroadcast(
        caff_InstanceHandle instanceHandle,
        void * userData,
        char const * title,
        caff_Rating rating,
        char const * gameId,
        caff_BroadcastStartedCallback broadcastStartedCallback,
        caff_BroadcastFailedCallback broadcastFailedCallback);


//! Broadcasts a frame of audio
/*!
This should be called by the application's audio output thread as long as the broadcast is online (after
`broadcastStartedCallback` has been called, and before either caff_endBroadcast() or `broadcastFailedCallback`).

Calling this while the broadcast is offline will have no effect, so the end-of-broadcast and end-of-capture do not have
to be strictly synchronized.

**TODO**: accept audio in different formats and transcode (with a warning log message)

\param instanceHandle the instance returned by caff_createInstance()
\param samples pointer to raw sample data. Samples must be 16-bit integer, 48000 Hz, 2-channel pending above TODO
\param samplesPerChannel number of samples per channel

\see caff_startBroadcast()
\see caff_sendVideo()
*/
CAFFEINE_API void caff_sendAudio(caff_InstanceHandle instanceHandle, uint8_t * samples, size_t samplesPerChannel);


//! Broadcasts a frame of video
/*!
This should be called by the application's video output thread as long as the broadcast is online (after
`broadcastStartedCallback` has been called, and before either caff_endBroadcast() or `broadcastFailedCallback`).

The first frame of video sent after starting a broadcast will be used as the screenshot in the Caffeine.tv lobby.

Calling this while the broadcast is offline will have no effect, so the end-of-broadcast and end-of-capture do not have
to be strictly synchronized.

For best results, \p height should be `720` and \p format should be ::caff_VideoFormatI420. This will avoid costs of
rescaling and reformatting by WebRTC.

\param instanceHandle the instance returned by caff_createInstance()
\param format the format of the raw pixel data
\param framePixels the raw pixel data
\param frameTotalBytes the number of bytes of pixel data
\param width the frame width
\param height the frame height
\param timestampMicros the timestamp for the video frame. You can pass ::caff_TimestampGenerate to use the current
    system time

\see caff_startBroadcast()
\see caff_sendAudio()
*/
CAFFEINE_API void caff_sendVideo(
        caff_InstanceHandle instanceHandle,
        caff_VideoFormat format,
        uint8_t const * framePixels,
        size_t frameTotalBytes,
        int32_t width,
        int32_t height,
        int64_t timestampMicros);


//! Set the game ID for the broadcast
/*!
This should be called during an active broadcast to update the user's stage with a new game ID (or none, if no longer
capturing a supported game).

Calling this while the broadcast is offline will have no effect, so the end-of-broadcast and end-of-game-monitoring do
not have to be strictly synchronized.

\param instanceHandle the instance returned by caff_createInstance()
\param gameId
              - a known game ID that the application acquires from caff_enumerateGames()
              - NULL to indicate that there is no supported game being captured
*/
CAFFEINE_API void caff_setGameId(caff_InstanceHandle instanceHandle, char const * gameId);


//! Set the title of the broadcast
/*!
This should be called during an active broadcast to update the user's stage with a broadcast title.

Calling this while the broadcast is offline will have no effect.

\param instanceHandle the instance returned by caff_createInstance()
\param title the user's desired broadcast title, max 60 characters. If `NULL` or empty, the default title
        "LIVE on Caffeine!" will be used
*/
CAFFEINE_API void caff_setTitle(caff_InstanceHandle instanceHandle, char const * title);


//! Set the content rating of the broadcast
/*!
This should be called during an active broadcast to update the broadcast title with a new content rating.

Calling this while the broadcast is offline will have no effect.

\param instanceHandle the instance returned by caff_createInstance()
\param rating the new content rating
*/
CAFFEINE_API void caff_setRating(caff_InstanceHandle instanceHandle, caff_Rating rating);


//! Get the connection quality for the broadcast
/*!
The application can call this during an active broadcast to determine if there are issues with the network between the
broadcaster and the Caffeine servers.

\param instanceHandle the instance returned by caff_createInstance()

\return - caff_ConnectionQualityGood if network conditions are acceptable
        - caff_ConnectionQualityPoor if there is a significant amount of packet loss
        - caff_ConnectionQualityUnknown if the broadcast is offline or the server has not responded with a reading yet
*/
CAFFEINE_API caff_ConnectionQuality caff_getConnectionQuality(caff_InstanceHandle instanceHandle);


//! End a Caffeine broadcast
/*!
This signals the server to end the broadcast and closes the RTC connection.

If the broadcast failed to start or ended in failure, the application does not need to call this.

Calling this while the broadcast is offline will have no effect.

\param instanceHandle the instance returned by caff_createInstance()
*/
CAFFEINE_API void caff_endBroadcast(caff_InstanceHandle instanceHandle);


//! Sign out of caffeine
/*!
This will end the broadcast, if active, and discard the instance's authentication data.

\param instanceHandle the instance returned by caff_createInstance()
*/
CAFFEINE_API void caff_signOut(caff_InstanceHandle instanceHandle);



//! Free the Caffeine instance
/*!
This destroys the internal factory objects, shuts down worker threads, etc.

\param instanceHandle pointer to the instance handle received from caff_createInstance. Since this handle will no longer
    be valid after the function returns, the pointed-to handle will be set to NULL.
*/
CAFFEINE_API void caff_freeInstance(caff_InstanceHandle * instanceHandle);

#endif /* LIBCAFFEINE_CAFFEINE_H */
