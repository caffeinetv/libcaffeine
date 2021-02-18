#include "Serialization.hpp"

#include <curl/curl.h>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <sstream>
#include <thread>

#if _WIN32
#include <Windows.h>
#include <functional>
#include <future>
#include <string>
#endif

#include "Configuration.hpp"
#include "Urls.hpp"

#define CONTENT_TYPE_JSON "Content-Type: application/json"
#define CONTENT_TYPE_FORM "Content-Type: multipart/form-data"

/* Notes for refactoring
 *
 * Request type: GET, PUT, POST, PATCH
 *
 * Request body format: Form data, JSON
 *
 * Request data itself
 *
 * URL, URL_F
 *
 * Basic, Authenticated
 *
 * Result type: new object/nullptr, success/failure
 *
 * Error type: (various Json formats, occasionally HTML e.g. for 502)
 */

using namespace std::chrono_literals;

namespace caff {
    std::string clientType;
    std::string clientVersion;

    class ScopedCurl final {
        static auto constexpr timeoutSeconds = 10l;
        static auto constexpr lowSpeedBps = 100'000l;

    public:
        explicit ScopedCurl(curl_slist * headers) : headers(headers) {
            CHECK_PTR(headers);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeoutSeconds);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, lowSpeedBps);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, timeoutSeconds);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&responseStr);
        }
        explicit ScopedCurl(char const * contentType) : ScopedCurl(basicHeaders(contentType)) {}
        ScopedCurl(char const * contentType, SharedCredentials & creds)
            : ScopedCurl(authenticatedHeaders(contentType, creds)) {}

        ~ScopedCurl() {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }

        operator CURL *() { return curl; }

        std::string const & getResponse() const { return responseStr; }

    private:
        CURL * curl = curl_easy_init();
        curl_slist * headers;
        std::string responseStr;

        static curl_slist * basicHeaders(char const * contentType) {
            curl_slist * headers = nullptr;
            headers = curl_slist_append(headers, contentType);
            headers = curl_slist_append(headers, ("X-Client-Type: " + clientType).c_str());
            headers = curl_slist_append(headers, ("X-Client-Version: " + clientVersion).c_str());
            headers = curl_slist_append(headers, "X-Libcaffeine-Version: " LIBCAFFEINE_VERSION);
            return headers;
        }

        static curl_slist * authenticatedHeaders(char const * contentType, SharedCredentials & sharedCreds) {
            std::string authorization("Authorization: Bearer ");
            std::string credential("X-Credential: ");

            {
                auto lockedCreds = sharedCreds.lock();
                authorization += lockedCreds.credentials.accessToken;
                credential += lockedCreds.credentials.credential;
            }

            curl_slist * headers = basicHeaders(contentType);
            headers = curl_slist_append(headers, authorization.c_str());
            headers = curl_slist_append(headers, credential.c_str());
            return headers;
        }

        static size_t curlWriteCallback(char * ptr, size_t, size_t nmemb, void * userData) {
            if (nmemb > 0) {
                std::string & resultStr = *(reinterpret_cast<std::string *>(userData));
                resultStr.append(ptr, nmemb);
            }
            return nmemb;
        }
    };

    struct ScopedPost {
        ~ScopedPost() { curl_formfree(head); }
        curl_httppost * head = nullptr;
        curl_httppost * tail = nullptr;
    };

    template <typename T> struct Retryable {
        enum Desire { Retry, Complete } desire = Retry;
        T result;

        Retryable(Desire desire, T result) : desire(desire), result(std::move(result)) {}
        Retryable(T result) : Retryable(Complete, std::move(result)) {}
    };

    template <typename T> Retryable<T> retry(T result) {
        return Retryable<T>{ Retryable<T>::Retry, std::move(result) };
    }

    template <typename T> bool isRetry(Retryable<T> const & retryable) {
        return retryable.desire == Retryable<T>::Retry;
    }

    template <typename T> bool isComplete(Retryable<T> const & retryable) {
        return retryable.desire == Retryable<T>::Complete;
    }

    std::chrono::duration<long long> backoffDuration(size_t tryNum) {
        return std::min(1 + 1 * tryNum, size_t{ 20 }) * 1s;
    }

    // TODO: put in configuration header or something
    int constexpr numRetries = 3;
    template <typename T> T retryRequest(std::function<Retryable<T>()> requestFunction) {
        Retryable<T> retryable{ Retryable<T>::Retry, {} };
        for (size_t tryNum = 0; tryNum < numRetries; ++tryNum) {
            if (tryNum > 0) {
                auto sleepFor = backoffDuration(tryNum);
                LOG_DEBUG("Retrying in %lld seconds", sleepFor.count());
                std::this_thread::sleep_for(sleepFor);
            }
            retryable = requestFunction();
            if (isComplete(retryable)) {
                break;
            }
        }
        if (isRetry(retryable)) {
            LOG_ERROR("Request failed after %d retries", numRetries);
        } else {
            LOG_DEBUG("Request complete");
        }
        return std::move(retryable.result);
    }

    SharedCredentials::SharedCredentials(Credentials credentials) : credentials(std::move(credentials)) {}

    LockedCredentials SharedCredentials::lock() {
        mutex.lock();
        return LockedCredentials(*this);
    }

    LockedCredentials::LockedCredentials(SharedCredentials & sharedCredentials)
        : credentials(sharedCredentials.credentials), lock(sharedCredentials.mutex, std::adopt_lock) {}

    static Retryable<caff_Result> doCheckVersion() {
        ScopedCurl curl(CONTENT_TYPE_JSON);

        curl_easy_setopt(curl, CURLOPT_URL, versionCheckUrl.c_str());


        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure checking version: [%d] %s", curlResult, curlError);
            return retry(caff_ResultFailure);
        }

        Json responseJson;
        try {
            responseJson = Json::parse(curl.getResponse());
        } catch (...) {
            LOG_ERROR("Failed to parse version check response");
            return retry(caff_ResultFailure);
        }

        auto errors = responseJson.find("errors");
        if (errors != responseJson.end()) {
            auto errorText = errors->at("_expired").at(0).get<std::string>();
            LOG_ERROR("%s", errorText.c_str());
            return caff_ResultOldVersion;
        }

        return caff_ResultSuccess;
    }

#if _WIN32
    class w32_ping_async {
    public:
        w32_ping_async(const char * hostname, int timeout_milliseconds)
            : result_p()
            , result_f(result_p.get_future())
            , async_thread([hostname = hostname, timeout_milliseconds = timeout_milliseconds, &result_p = result_p]() {
                result_p.set_value(do_ping_async(hostname, timeout_milliseconds));
            }) {}

        virtual ~w32_ping_async() { async_thread.join(); }
        bool result() { return result_f.get(); }

    private:
        static bool do_ping_async(const char * hostname, unsigned int timeout_milliseconds) {
            if ((strlen(hostname) > 100) || (timeout_milliseconds > 10000)) {
                return false;
            }

            char ping_command_line[256];
            sprintf_s(ping_command_line, "ping %s -n 1 -w %u", hostname, timeout_milliseconds);

            STARTUPINFOA startup_info;
            ZeroMemory(&startup_info, sizeof(startup_info));
            startup_info.cb = sizeof(startup_info);
            PROCESS_INFORMATION process_info;
            ZeroMemory(&process_info, sizeof(process_info));

            bool ping_result = false;
            if (CreateProcessA(
                        NULL,
                        ping_command_line,
                        NULL,
                        NULL,
                        FALSE,
                        CREATE_NO_WINDOW,
                        NULL,
                        NULL,
                        &startup_info,
                        &process_info)) {
                if (WAIT_OBJECT_0 == WaitForSingleObject(process_info.hProcess, timeout_milliseconds + 1000)) {
                    DWORD exit_code;
                    GetExitCodeProcess(process_info.hProcess, &exit_code);
                    ping_result = (0 == exit_code);
                }
                CloseHandle(process_info.hProcess);
                CloseHandle(process_info.hThread);
            }
            return ping_result;
        }

        std::promise<bool> result_p;
        std::future<bool> result_f;
        std::thread async_thread;
    };
#endif

    caff_Result checkInternetConnection() {
#if _WIN32
        // w32_ping_async::result() blocks until result is ready. 'true' is success 'false' is failure. 
        if (w32_ping_async("www.apple.com", 3000).result() || w32_ping_async("www.google.com", 3000).result() || w32_ping_async("www.amazon.com", 3000).result()) {
            return caff_ResultSuccess;
        }
#else
        // System result 0 is success 1 is failure
        /**
         *  In macos it is necessary to specify no of times to ping(-c)
         * otherwise it pings continuously until manually stopped. 
         */
        if (!system("ping www.apple.com -c 1 -t 3000") || !system("ping www.google.com -c 1 -t 3000") || !system("ping www.amazon.com -c 1 -t 3000")){
            return caff_ResultSuccess;
        }
#endif        
        return caff_ResultInternetDisconnected;
    }

    caff_Result checkCaffeineConnection() {
        ScopedCurl curl(CONTENT_TYPE_JSON);
        curl_easy_setopt(curl, CURLOPT_URL, healthCheckUrl.c_str());

        CURLcode curlResult =  curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            if (curlResult == CURLE_COULDNT_RESOLVE_HOST || curlResult == CURLE_COULDNT_CONNECT) {
                return caff_ResultCaffeineUnreachable;
            } else {
                return caff_ResultFailure;
            }
        }
        return caff_ResultSuccess;
    }

    caff_Result checkVersion() {
        if (clientType.empty() || clientVersion.empty()) {
            LOG_ERROR("Libcaffeine has not been initialized with client version info");
            return caff_ResultFailure;
        }
        return retryRequest<caff_Result>(doCheckVersion);
    }

    /* TODO: refactor this - lots of dupe code between request types
     * TODO: reuse curl handle across requests
     */
    static Retryable<AuthResponse> doSignIn(char const * username, char const * password, char const * otp) {
        Json requestJson;

        try {
            requestJson = { { "account",
                              {
                                      { "username", username },
                                      { "password", password },
                              } } };

            if (otp && otp[0]) {
                requestJson["mfa"] = { { "otp", otp } };
            }
        } catch (...) {
            LOG_ERROR("Failed to create request JSON");
            return { {} };
        }

        auto requestBody = requestJson.dump();

        ScopedCurl curl(CONTENT_TYPE_JSON);

        curl_easy_setopt(curl, CURLOPT_URL, signInUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure signing in: [%d] %s", curlResult, curlError);
            return { {} };
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        LOG_DEBUG("Http response [%ld]", responseCode);

        if (responseCode == 401) {
            return { { caff_ResultInfoIncorrect, {} } };
        }

        Json responseJson;
        try {
            responseJson = Json::parse(curl.getResponse());
        } catch (...) {
            LOG_ERROR("Failed to parse signin response");
            return { {} };
        }

        auto errors = responseJson.find("errors");
        if (errors != responseJson.end()) {
            auto otpError = errors->find("otp");
            if (otpError != errors->end()) {
                auto errorText = otpError->at(0).get<std::string>();
                LOG_ERROR("One time password error: %s", errorText.c_str());
                if (otp && *otp) {
                    return { { caff_ResultMfaOtpIncorrect } };
                } else {
                    return { { caff_ResultMfaOtpRequired } };
                }
            }
            auto errorText = errors->at("_error").at(0).get<std::string>();
            LOG_ERROR("Error logging in: %s", errorText.c_str());
            return { {} };
        }

        auto credsIt = responseJson.find("credentials");
        if (credsIt != responseJson.end()) {
            LOG_DEBUG("Sign-in complete");
            return { { caff_ResultSuccess, *credsIt } };
        }

        std::string mfaOtpMethod;

        auto nextIt = responseJson.find("next");
        if (nextIt != responseJson.end()) {
            auto & next = nextIt->get_ref<std::string const &>();
            if (next == "mfa_otp_required") {
                return { { caff_ResultMfaOtpRequired } };
            } else if (next == "legal_acceptance_required") {
                return { { caff_ResultLegalAcceptanceRequired } };
            } else if (next == "email_verification") {
                return { { caff_ResultEmailVerificationRequired } };
            } else {
                LOG_ERROR("Unrecognized auth next step %s", next.c_str());
                return { {} };
            }
        }

        LOG_ERROR("Sign-in response missing");
        return { {} };
    }

    AuthResponse signIn(char const * username, char const * password, char const * otp) {
        return retryRequest<AuthResponse>(std::bind(doSignIn, username, password, otp));
    }

    static Retryable<AuthResponse> doRefreshAuth(char const * refreshToken) {
        Json requestJson = { { "refresh_token", refreshToken } };

        auto requestBody = requestJson.dump();

        ScopedCurl curl(CONTENT_TYPE_JSON);

        curl_easy_setopt(curl, CURLOPT_URL, refreshTokenUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure refreshing credentials: [%d] %s", curlResult, curlError);
            return { {} };
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        LOG_DEBUG("Http response [%ld]", responseCode);

        if (responseCode == 401) {
            LOG_ERROR("Invalid refresh token");
            return { { caff_ResultInfoIncorrect } };
        }

        Json responseJson;
        try {
            responseJson = Json::parse(curl.getResponse());
        } catch (...) {
            LOG_ERROR("Failed to parse refresh response");
            return { {} };
        }

        auto errorsIt = responseJson.find("errors");
        if (errorsIt != responseJson.end()) {
            auto errorText = errorsIt->at("_error").at(0).get<std::string>();
            LOG_ERROR("Error refreshing credentials: %s", errorText.c_str());
            return { {} };
        }

        auto credsIt = responseJson.find("credentials");
        if (credsIt != responseJson.end()) {
            LOG_DEBUG("Credentials refresh complete");
            return { { caff_ResultSuccess, *credsIt } };
        }

        LOG_ERROR("Failed to extract response info");
        return { {} };
    }

    AuthResponse refreshAuth(char const * refreshToken) {
        return retryRequest<AuthResponse>(std::bind(doRefreshAuth, refreshToken));
    }

    bool refreshCredentials(SharedCredentials & creds) {
        auto refreshToken = creds.lock().credentials.refreshToken;
        auto response = refreshAuth(refreshToken.c_str());
        if (response.credentials) {
            creds.lock().credentials = std::move(*response.credentials);
            return true;
        }
        return false;
    }

    static Retryable<optional<UserInfo>> doGetUserInfo(SharedCredentials & creds) {
        ScopedCurl curl(CONTENT_TYPE_JSON, creds);

        auto urlStr = getUserUrl(creds.lock().credentials.caid);
        curl_easy_setopt(curl, CURLOPT_URL, urlStr.c_str());

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure fetching user: [%d] %s", curlResult, curlError);
            return { {} };
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        // TODO: duplicate logic; maybe move to ScopedCurl?
        if (responseCode == 401) {
            if (refreshCredentials(creds)) {
                return doGetUserInfo(creds);
            } else {
                return { {} };
            }
        }

        Json responseJson;
        try {
            responseJson = Json::parse(curl.getResponse());
        } catch (...) {
            LOG_ERROR("Failed to parse user response");
            return { {} };
        }

        auto errorsIt = responseJson.find("errors");
        if (errorsIt != responseJson.end()) {
            auto errorText = errorsIt->at("_error").at(0).get<std::string>();
            LOG_ERROR("Error fetching user: %s", errorText.c_str());
            return { {} };
        }

        auto userIt = responseJson.find("user");
        if (userIt != responseJson.end()) {
            LOG_DEBUG("Got user details");
            return { *userIt };
        }

        LOG_ERROR("Failed to get user info");
        return { {} };
    }

    optional<UserInfo> getUserInfo(SharedCredentials & creds) {
        return retryRequest<optional<UserInfo>>(std::bind(doGetUserInfo, std::ref(creds)));
    }

    static Retryable<optional<GameList>> doGetSupportedGames() {
        ScopedCurl curl(CONTENT_TYPE_JSON);

        curl_easy_setopt(curl, CURLOPT_URL, getGamesUrl.c_str());

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure fetching supported games: [%d] %s", curlResult, curlError);
            return { {} };
        }

        Json responseJson;
        try {
            responseJson = Json::parse(curl.getResponse());
        } catch (...) {
            LOG_ERROR("Failed to parse game list response");
            return { {} };
        }

        if (!responseJson.is_array()) {
            LOG_ERROR("Unable to retrieve games list");
            return { {} };
        }

        return { responseJson };
    }

    optional<GameList> getSupportedGames() { return retryRequest<optional<GameList>>(doGetSupportedGames); }

    static Retryable<bool> doTrickleCandidates(
            std::vector<IceInfo> const & candidates, std::string const & streamUrl, SharedCredentials & creds) {
        Json requestJson = { { "ice_candidates", candidates } };

        std::string requestBody = requestJson.dump();

        ScopedCurl curl(CONTENT_TYPE_JSON, creds);

        curl_easy_setopt(curl, CURLOPT_URL, streamUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure negotiating ICE: [%d] %s", curlResult, curlError);
            return retry(false);
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        switch (responseCode) {
        case 200:
            LOG_DEBUG("ICE candidates trickled");
            return true;
        case 401:
            LOG_DEBUG("Unauthorized - refreshing credentials");
            if (refreshCredentials(creds)) {
                return doTrickleCandidates(candidates, streamUrl, creds);
            } else {
                return false;
            }
        default:
            LOG_ERROR("Error negotiating ICE candidates");
            return retry(false);
        }
    }

    bool trickleCandidates(
            std::vector<IceInfo> const & candidates, std::string const & streamUrl, SharedCredentials & creds) {
        return retryRequest<bool>([&] { return doTrickleCandidates(candidates, streamUrl, creds); });
    }

    static Retryable<optional<HeartbeatResponse>> doHeartbeatStream(
            std::string const & streamUrl, SharedCredentials & sharedCreds) {
        ScopedCurl curl(CONTENT_TYPE_JSON, sharedCreds);

        auto url = streamHeartbeatUrl(streamUrl);

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}"); // TODO: is this necessary?
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure hearbeating stream: [%d] %s", curlResult, curlError);
            return { {} };
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        if (responseCode == 401) {
            LOG_DEBUG("Unauthorized - refreshing credentials");
            if (refreshCredentials(sharedCreds)) {
                return doHeartbeatStream(streamUrl, sharedCreds);
            }
            return { {} };
        }

        if (responseCode != 200) {
            LOG_ERROR("Error heartbeating stream: %ld", responseCode);
            return { {} };
        }

        Json responseJson;
        try {
            responseJson = Json::parse(curl.getResponse());
        } catch (...) {
            LOG_ERROR("Failed to parse refresh response");
            return { {} };
        }

        LOG_DEBUG("Broadcast heartbeat succeeded");
        return { responseJson };
    }

    optional<HeartbeatResponse> heartbeatStream(std::string const & streamUrl, SharedCredentials & sharedCreds) {
        return retryRequest<optional<HeartbeatResponse>>([&] { return doHeartbeatStream(streamUrl, sharedCreds); });
    }

    static Retryable<bool> doUpdateScreenshot(
            std::string broadcastId, ScreenshotData const & screenshotData, SharedCredentials & sharedCreds) {
        ScopedCurl curl(CONTENT_TYPE_FORM, sharedCreds);

        ScopedPost post;

        if (!screenshotData.empty()) {
            curl_formadd(
                    &post.head,
                    &post.tail,
                    CURLFORM_PTRNAME,
                    "broadcast[game_image]",
                    CURLFORM_BUFFER,
                    "game_image.jpg",
                    CURLFORM_BUFFERPTR,
                    &screenshotData[0],
                    CURLFORM_BUFFERLENGTH,
                    screenshotData.size(),
                    CURLFORM_CONTENTTYPE,
                    "image/jpeg",
                    CURLFORM_END);
        }

        curl_easy_setopt(curl, CURLOPT_HTTPPOST, post.head);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

        auto url = broadcastUrl(broadcastId);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure updating broadcast screenshot: [%d] %s", curlResult, curlError);
            return retry(false);
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        LOG_DEBUG("Http response code [%ld]", responseCode);

        if (responseCode == 401) {
            LOG_DEBUG("Unauthorized - refreshing credentials");
            if (refreshCredentials(sharedCreds)) {
                return doUpdateScreenshot(std::move(broadcastId), screenshotData, sharedCreds);
            }
            return false;
        }

        bool success = responseCode / 100 == 2;
        if (success) {
            return true;
        } else {
            LOG_ERROR("Failed to update broadcast screenshot");
            return retry(false);
        }
    }

    bool updateScreenshot(
            std::string broadcastId, ScreenshotData const & screenshotData, SharedCredentials & sharedCreds) {
        return retryRequest<bool>([&] { return doUpdateScreenshot(broadcastId, screenshotData, sharedCreds); });
    }

    void sendWebrtcStats(SharedCredentials & sharedCreds, Json const & stats) {
        ScopedCurl curl(CONTENT_TYPE_FORM, sharedCreds);

        ScopedPost post;

        auto statsString = stats.dump();

        curl_formadd(
                &post.head,
                &post.tail,
                CURLFORM_PTRNAME,
                "primary",
                CURLFORM_PTRCONTENTS,
                statsString.c_str(),
                CURLFORM_CONTENTTYPE,
                "application/json",
                CURLFORM_END);

        curl_easy_setopt(curl, CURLOPT_HTTPPOST, post.head);

        curl_easy_setopt(curl, CURLOPT_URL, broadcastMetricsUrl.c_str());

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure sending webrtc metrics: [%d] %s", curlResult, curlError);
            return;
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        LOG_DEBUG("Http response code [%ld]", responseCode);

        bool result = responseCode / 100 == 2;

        if (!result) {
            LOG_ERROR("Failed to send webrtc metrics");
        }
    }

    static Retryable<optional<Json>> doGraphqlRawRequest(SharedCredentials & creds, Json const & requestJson) {
        auto requestBody = requestJson.dump();

        ScopedCurl curl(CONTENT_TYPE_JSON, creds);

        curl_easy_setopt(curl, CURLOPT_URL, realtimeGraphqlUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure performing graphql request: [%d] %s", curlResult, curlError);
            return retry(optional<Json>{});
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        LOG_DEBUG("Http response [%ld]", responseCode);

        if (responseCode == 401) {
            LOG_DEBUG("Unauthorized - refreshing credentials");
            if (refreshCredentials(creds)) {
                return doGraphqlRawRequest(creds, requestJson);
            }
            return optional<Json>{};
        } else if (responseCode / 100 != 2) {
            return retry(optional<Json>{});
        }

        Json responseJson;
        try {
            return { { Json::parse(curl.getResponse()) } };
        } catch (...) {
            LOG_ERROR("Failed to deserialize graphql response to JSON");
            return optional<Json>{};
        }
    }

    optional<Json> graphqlRawRequest(SharedCredentials & creds, Json const & requestJson) {
        return retryRequest<optional<Json>>([&] { return doGraphqlRawRequest(creds, requestJson); });
    }

    static Retryable<optional<EncoderInfoResponse>> doGetEncoderInfo(SharedCredentials & sharedCreds) {
        ScopedCurl curl(CONTENT_TYPE_JSON, sharedCreds);

        curl_easy_setopt(curl, CURLOPT_URL, encoderInfoUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}"); 
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (CURLE_OK != curlResult) {
            LOG_ERROR("HTTP failure fetching encoder info: [%d] %s", curlResult, curlError);
            return { {} };
        }

        Json responseJson;
        try {
            responseJson = Json::parse(curl.getResponse());
        } catch (...) {
            LOG_ERROR("Failed to prase encoder info response");
            return { {} };
        }

        return { responseJson };
    }

    optional<EncoderInfoResponse> getEncoderInfo(SharedCredentials & creds) {
        return retryRequest<optional<EncoderInfoResponse>>(std::bind(doGetEncoderInfo, std::ref(creds)));
    }

} // namespace caff
