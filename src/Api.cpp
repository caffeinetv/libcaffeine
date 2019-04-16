#include "Serialization.hpp"

#include "ErrorLogging.hpp"

#include <curl/curl.h>
#include <mutex>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>


/* TODO: C++ify (namespace, raii, not libcurl, etc) */

// TODO: should backend differentiate client from libcaffeine version?
#define API_VERSION "0.1"

#define UNUSED_PARAMETER(p) ((void) p)

/* TODO: load these from config? environment? */
#if CAFFEINE_STAGING
#define CAFFEINE_DOMAIN "staging.caffeine.tv/"
#else
#define CAFFEINE_DOMAIN "caffeine.tv/"
#endif

#define API_ENDPOINT       "https://api." CAFFEINE_DOMAIN
#define REALTIME_ENDPOINT  "https://realtime." CAFFEINE_DOMAIN

/* TODO: some of these are deprecated */
#define VERSION_CHECK_URL  API_ENDPOINT "v1/version-check"
#define SIGNIN_URL         API_ENDPOINT "v1/account/signin"
#define REFRESH_TOKEN_URL  API_ENDPOINT "v1/account/token"
#define GETGAMES_URL       API_ENDPOINT "v1/games"
#define GETUSER_URL(id)    (std::string(API_ENDPOINT "v1/users/") + (id))
#define BROADCAST_URL(id)  (std::string(API_ENDPOINT "v1/broadcasts/") + (id))

#define STAGE_UPDATE_URL(username)        (std::string(REALTIME_ENDPOINT "v4/stage/") + (username))
#define STREAM_HEARTBEAT_URL(streamUrl)   (std::string((streamUrl)) + "/heartbeat")

#define CONTENT_TYPE_JSON  "Content-Type: application/json"
#define CONTENT_TYPE_FORM  "Content-Type: multipart/form-data"


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

namespace caff {
    // RAII helper to get rid of gotos
    // TODO use a C++ HTTP library
    class ScopedCurl final {
    public:
        explicit ScopedCurl(char const * contentType) : curl(curl_easy_init()), headers(basicHeaders(contentType))
        {
            applyHeaders();
        }

        ScopedCurl(char const * contentType, SharedCredentials & creds)
            : curl(curl_easy_init()), headers(authenticatedHeaders(contentType, creds))
        {
            applyHeaders();
        }

        ~ScopedCurl()
        {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }

        operator CURL *() { return curl; }

    private:
        CURL * curl;
        curl_slist * headers;

        void applyHeaders()
        {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }

        static curl_slist * basicHeaders(char const * contentType)
        {
            curl_slist * headers = nullptr;
            headers = curl_slist_append(headers, contentType);
            headers = curl_slist_append(headers, "X-Client-Type: libcaffeine");
            headers = curl_slist_append(headers, "X-Client-Version: " API_VERSION);
            return headers;
        }

        static curl_slist * authenticatedHeaders(char const * contentType, SharedCredentials & sharedCreds)
        {
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
    };

    struct ScopedPost {
        ~ScopedPost() { curl_formfree(head); }
        curl_httppost * head = nullptr;
        curl_httppost * tail = nullptr;
    };

    static size_t curlWriteCallback(char * ptr, size_t size, size_t nmemb, void * userData)
    {
        UNUSED_PARAMETER(size);
        if (nmemb > 0) {
            std::string & resultStr = *(reinterpret_cast<std::string *>(userData));
            resultStr.append(ptr, nmemb);
        }
        return nmemb;
    }

#define RETRY_MAX 3

    /* TODO: this is not very robust or intelligent. Some kinds of failure willarray
     * never be recoverable and should not be retried
     */
#define RETRY_REQUEST(ResultT, request) \
        for (int tryNum = 0; tryNum < RETRY_MAX; ++tryNum) { \
            ResultT result = request; \
            if (result) { \
                return result; \
            } \
            std::this_thread::sleep_for(std::chrono::seconds(1 + 1 * tryNum)); \
        } \
        return {}

    SharedCredentials::SharedCredentials(Credentials credentials) : credentials(std::move(credentials)) {}

    LockedCredentials SharedCredentials::lock()
    {
        mutex.lock();
        return LockedCredentials(*this);
    }

    LockedCredentials::LockedCredentials(SharedCredentials & sharedCredentials)
        : credentials(sharedCredentials.credentials)
        , lock(sharedCredentials.mutex, std::adopt_lock)
    {}

    static bool doIsSupportedVersion()
    {
        ScopedCurl curl(CONTENT_TYPE_JSON);

        curl_easy_setopt(curl, CURLOPT_URL, VERSION_CHECK_URL);

        std::string responseStr;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&responseStr);

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure checking version: [%d] %s", curlResult, curlError);
            return false;
        }

        Json responseJson;
        try {
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to parse version check response");
            return false;
        }

        auto errors = responseJson.find("errors");
        if (errors != responseJson.end()) {
            auto errorText = errors->at("_expired").at(0).get<std::string>();
            LOG_ERROR("%s", errorText.c_str());
            return false;
        }

        return true;
    }

    bool isSupportedVersion() {
        RETRY_REQUEST(bool, doIsSupportedVersion());
    }

    /* TODO: refactor this - lots of dupe code between request types
     * TODO: reuse curl handle across requests
     */
    static AuthResponse doSignin(char const * username, char const * password, char const * otp)
    {
        Json requestJson;

        try {
            requestJson = {
                {"account", {
                    {"username", username},
                    {"password", password},
                }}
            };

            if (otp && otp[0]) {
                requestJson["mfa"] = {{"otp", otp}};
            }
        }
        catch (...) {
            LOG_ERROR("Failed to create request JSON");
            return {};
        }

        auto requestBody = requestJson.dump();

        ScopedCurl curl(CONTENT_TYPE_JSON);

        curl_easy_setopt(curl, CURLOPT_URL, SIGNIN_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());

        std::string responseStr;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&responseStr);

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure signing in: [%d] %s", curlResult, curlError);
            return {};
        }

        Json responseJson;
        try {
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to parse signin response");
            return {};
        }

        auto errors = responseJson.find("errors");
        if (errors != responseJson.end()) {
            auto otpError = errors->find("otp");
            if (otpError != errors->end()) {
                auto errorText = otpError->at(0).get<std::string>();
                LOG_ERROR("One time password error: %s", errorText.c_str());
                if (otp && *otp) {
                    return { caff_ResultMfaOtpIncorrect };
                }
                else {
                    return { caff_ResultMfaOtpRequired };
                }
            }
            auto errorText = errors->at("_error").at(0).get<std::string>();
            LOG_ERROR("Error logging in: %s", errorText.c_str());
            return {};
        }

        auto credsIt = responseJson.find("credentials");
        if (credsIt != responseJson.end()) {
            LOG_DEBUG("Sign-in complete");
            return { caff_ResultSuccess, *credsIt };
        }

        std::string mfaOtpMethod;

        auto nextIt = responseJson.find("next");
        if (nextIt != responseJson.end()) {
            auto & next = nextIt->get_ref<std::string const &>();
            if (next == "mfa_otp_required") {
                return { caff_ResultMfaOtpRequired };
            }
            else if (next == "legal_acceptance_required") {
                return { caff_ResultLegalAcceptanceRequired };
            }
            else if (next == "email_verification") {
                return { caff_ResultEmailVerificationRequired };
            }
            else {
                LOG_ERROR("Unrecognized auth next step %s", next.c_str());
                return {};
            }
        }

        LOG_ERROR("Sign-in response missing");
        return {};
    }

    AuthResponse signIn(char const * username, char const * password, char const * otp)
    {
        RETRY_REQUEST(AuthResponse, doSignin(username, password, otp));
    }

    static AuthResponse doRefreshAuth(char const * refreshToken)
    {
        Json requestJson = { {"refresh_token", refreshToken} };

        auto requestBody = requestJson.dump();

        ScopedCurl curl(CONTENT_TYPE_JSON);

        curl_easy_setopt(curl, CURLOPT_URL, REFRESH_TOKEN_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());

        std::string responseStr;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&responseStr);

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure refreshing tokens: [%d] %s", curlResult, curlError);
            return {};
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        LOG_DEBUG("Http response [%ld]", responseCode);

        Json responseJson;
        try {
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to parse refresh response");
            return {};
        }

        auto errorsIt = responseJson.find("errors");
        if (errorsIt != responseJson.end()) {
            auto errorText = errorsIt->at("_error").at(0).get<std::string>();
            LOG_ERROR("Error refreshing tokens: %s", errorText.c_str());
            return {};
        }

        auto credsIt = responseJson.find("credentials");
        if (credsIt != responseJson.end()) {
            LOG_DEBUG("Sign-in complete");
            return { caff_ResultSuccess, *credsIt };
        }

        LOG_ERROR("Failed to extract response info");
        return {};
    }

    AuthResponse refreshAuth(char const * refreshToken)
    {
        RETRY_REQUEST(AuthResponse, doRefreshAuth(refreshToken));
    }

    static bool doRefreshCredentials(SharedCredentials & creds)
    {
        auto refreshToken = creds.lock().credentials.refreshToken;
        auto response = doRefreshAuth(refreshToken.c_str());
        if (response.credentials) {
            creds.lock().credentials = std::move(*response.credentials);
            return true;
        }
        else {
            return false;
        }
    }

    static bool refreshCredentials(SharedCredentials & creds)
    {
        RETRY_REQUEST(bool, doRefreshCredentials(creds));
    }

    static optional<UserInfo> doGetUserInfo(SharedCredentials & creds)
    {
        ScopedCurl curl(CONTENT_TYPE_JSON, creds);

        auto urlStr = GETUSER_URL(creds.lock().credentials.caid);
        curl_easy_setopt(curl, CURLOPT_URL, urlStr.c_str());

        std::string responseStr;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&responseStr);

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure fetching user: [%d] %s", curlResult, curlError);
            return {};
        }

        Json responseJson;
        try {
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to parse user response");
            return {};
        }

        auto errorsIt = responseJson.find("errors");
        if (errorsIt != responseJson.end()) {
            auto errorText = errorsIt->at("_error").at(0).get<std::string>();
            LOG_ERROR("Error fetching user: %s", errorText.c_str());
            return {};
        }

        auto userIt = responseJson.find("user");
        if (userIt != responseJson.end()) {
            LOG_DEBUG("Got user details");
            return UserInfo(*userIt);
        }

        LOG_ERROR("Failed to get user info");
        return {};
    }

    optional<UserInfo> getUserInfo(SharedCredentials & creds)
    {
        RETRY_REQUEST(optional<UserInfo>, doGetUserInfo(creds));
    }

    static optional<GameList> doGetSupportedGames()
    {
        ScopedCurl curl(CONTENT_TYPE_JSON);

        curl_easy_setopt(curl, CURLOPT_URL, GETGAMES_URL);

        std::string responseStr;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&responseStr);

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure fetching supported games: [%d] %s", curlResult, curlError);
            return {};
        }

        Json responseJson;
        try {
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to parse game list response");
            return {};
        }

        if (!responseJson.is_array()) {
            LOG_ERROR("Unable to retrieve games list");
            return {};
        }

        return GameList(responseJson);
    }

    optional<GameList> getSupportedGames()
    {
        RETRY_REQUEST(optional<GameList>, doGetSupportedGames());
    }

    static bool doTrickleCandidates(
        std::vector<IceInfo> const & candidates,
        std::string const & streamUrl,
        SharedCredentials & creds)
    {
        Json requestJson = { {"ice_candidates", candidates} };

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
            return false;
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        bool response = false;

        switch (responseCode) {
        case 200:
            response = true;
            break;
        case 401:
            LOG_DEBUG("Unauthorized - refreshing credentials");
            if (refreshCredentials(creds)) {
                response = doTrickleCandidates(candidates, streamUrl, creds);
            }
            break;
        default:
            break;
        }

        if (response) {
            LOG_DEBUG("ICE candidates trickled");
        }
        else {
            LOG_ERROR("Error negotiating ICE candidates");
        }

        return response;
    }

    bool trickleCandidates(
        std::vector<IceInfo> const & candidates,
        std::string const & streamUrl,
        SharedCredentials & creds)
    {
        RETRY_REQUEST(bool, doTrickleCandidates(candidates, streamUrl, creds));
    }

    static optional<HeartbeatResponse> doHeartbeatStream(std::string const & streamUrl, SharedCredentials & sharedCreds)
    {
        ScopedCurl curl(CONTENT_TYPE_JSON, sharedCreds);

        auto url = STREAM_HEARTBEAT_URL(streamUrl);

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}"); // TODO: is this necessary?
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

        std::string responseStr;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&responseStr);

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure hearbeating stream: [%d] %s", curlResult, curlError);
            return {};
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        if (responseCode == 401) {
            LOG_DEBUG("Unauthorized - refreshing credentials");
            if (refreshCredentials(sharedCreds)) {
                return doHeartbeatStream(streamUrl, sharedCreds);
            }
            return {};
        }

        if (responseCode != 200) {
            LOG_ERROR("Error heartbeating stream: %ld", responseCode);
            return {};
        }

        Json responseJson;
        try {
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to parse refresh response");
            return {};
        }

        LOG_DEBUG("Broadcast heartbeat succeeded");
        return HeartbeatResponse(responseJson);
    }

    optional<HeartbeatResponse> heartbeatStream(std::string const & streamUrl, SharedCredentials & sharedCreds)
    {
        RETRY_REQUEST(optional<HeartbeatResponse>, doHeartbeatStream(streamUrl, sharedCreds));
    }

    static bool doUpdateScreenshot(
        std::string broadcastId,
        ScreenshotData const & screenshotData,
        SharedCredentials & sharedCreds)
    {
        ScopedCurl curl(CONTENT_TYPE_FORM, sharedCreds);

        ScopedPost post;

        if (!screenshotData.empty()) {
            curl_formadd(&post.head, &post.tail,
                CURLFORM_COPYNAME, "broadcast[game_image]",
                CURLFORM_BUFFER, "game_image.jpg",
                CURLFORM_BUFFERPTR, &screenshotData[0],
                CURLFORM_BUFFERLENGTH, screenshotData.size(),
                CURLFORM_CONTENTTYPE, "image/jpeg",
                CURLFORM_END);
        }

        curl_easy_setopt(curl, CURLOPT_HTTPPOST, post.head);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

        auto url = BROADCAST_URL(broadcastId);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure updating broadcast screenshot: [%d] %s", curlResult, curlError);
            return false;
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        LOG_DEBUG("Http response code [%ld]", responseCode);

        bool result = responseCode / 100 == 2;

        if (!result) {
            LOG_ERROR("Failed to update broadcast screenshot");
        }

        return result;
    }

    bool updateScreenshot(
        std::string broadcastId,
        ScreenshotData const & screenshotData,
        SharedCredentials & sharedCreds)
    {
        RETRY_REQUEST(bool, doUpdateScreenshot(broadcastId, screenshotData, sharedCreds));
    }

    static bool isOutOfCapacityFailure(std::string const & type)
    {
        return type == "OutOfCapacity";
    }

    static optional<StageResponseResult> doStageUpdate(
        StageRequest const & request,
        SharedCredentials & creds)
    {
        if (!request.stage ||
            !request.stage->username ||
            request.stage->username->empty()) {
            LOG_ERROR("Did not set request username");
            return {};
        }

        Json requestJson = request;
        auto requestBody = requestJson.dump();

        ScopedCurl curl(CONTENT_TYPE_JSON, creds);

        auto urlStr = STAGE_UPDATE_URL(*request.stage->username);

        curl_easy_setopt(curl, CURLOPT_URL, urlStr.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

        std::string responseStr;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&responseStr);

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure performing stage update: [%d] %s", curlResult, curlError);
            return {};
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        LOG_DEBUG("Http response [%ld]", responseCode);

        if (responseCode == 401) {
            LOG_DEBUG("Unauthorized - refreshing credentials");
            if (refreshCredentials(creds)) {
                return doStageUpdate(request, creds);
            }
            return {};
        }

        Json responseJson;
        try {
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to deserialize stage update response to JSON");
            return {};
        }

        if (responseCode == 200) {
            try {
                return StageResponse(responseJson);
            }
            catch (...) {
                LOG_ERROR("Failed to unpack stage response");
                return {};
            }
        }
        else {
            try {
                auto type = responseJson.at("type").get<std::string>();

                // As of now, the only failure response we want to return and not retry is `OutOfCapacity`
                if (isOutOfCapacityFailure(type)) {
                    return FailureResponse(responseJson);
                }
            }
            catch (...) {
                LOG_ERROR("Failed to unpack failure response");
            }
            return {};
        }
    }

    static optional<StageResponseResult> stageUpdate(StageRequest const & request, SharedCredentials & creds)
    {
        RETRY_REQUEST(optional<StageResponseResult>, doStageUpdate(request, creds));
    }

    bool requestStageUpdate(
        StageRequest & request,
        SharedCredentials & creds,
        std::chrono::milliseconds * retryIn,
        bool * isOutOfCapacity)
    {
        auto result = stageUpdate(request, creds);
        if (!result.has_value()) {
            return false;
        }

        auto response = get_if<StageResponse>(&*result);
        if (response) {
            if (retryIn) {
                *retryIn = response->retryIn;
            }
            request.cursor = std::move(response->cursor);
            request.stage = std::move(response->stage);
            return true;
        }
        else
        {
            auto & failure = get<FailureResponse>(*result);
            if (isOutOfCapacity && isOutOfCapacityFailure(failure.type)) {
                *isOutOfCapacity = true;
            }
            return false;
        }
    }

}
