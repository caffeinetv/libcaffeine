#include "Serialization.hpp"

#include <curl/curl.h>
#include <mutex>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>


/* TODO: C++ify (namespace, raii, not libcurl, etc) */
// TODO: get this from client code
// TODO: should backend differentiate client from libcaffeine version?
#define API_VERSION "22.2"

#define UNUSED_PARAMETER(p) ((void) p)

/* TODO: load these from config? */
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


// TODO make these into something else?
#define LOG_ERROR(...) ((void)0)
#define LOG_WARN(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define TRACE() ((void)0)

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

        ScopedCurl(char const * contentType, Credentials * creds)
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
            headers = curl_slist_append(headers, "X-Client-Type: obs"); // TODO client type parameter
            headers = curl_slist_append(headers, "X-Client-Version: " API_VERSION);
            return headers;
        }

        static curl_slist * authenticatedHeaders(char const * contentType, Credentials * creds)
        {
            std::string authorization("Authorization: Bearer ");
            std::string credential("X-Credential: ");

            {
                std::lock_guard<std::mutex> lock(creds->mutex);
                authorization += creds->accessToken;
                credential += creds->credential;
            }

            curl_slist * headers = basicHeaders(contentType);
            headers = curl_slist_append(headers, authorization.c_str());
            headers = curl_slist_append(headers, credential.c_str());
            return headers;
        }
    };

    class ScopedPost {
    public:
        ScopedPost(curl_httppost * post) : post(post) {}
        virtual ~ScopedPost() { curl_formfree(post); }
    private:
        curl_httppost * post;
    };

    /*
    void caff_set_string(char ** dest, char const * new_value)
    {
        if (!dest) return;

        if (*dest) {
            delete[] * dest;
            *dest = nullptr;
        }

        *dest = cstrdup(new_value);
    }

    void caff_free_string(char ** str)
    {
        if (!str) return;
        delete[] * str;
        *str = nullptr;
    }
    */

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

    static bool doIsSupportedVersion()
    {
        TRACE();

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
    static caff_AuthResponse * doSignin(char const * username, char const * password, char const * otp)
    {
        TRACE();
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
            return nullptr;
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
            return nullptr;
        }

        Json responseJson;
        try {
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to parse signin response");
            return nullptr;
        }

        auto errors = responseJson.find("errors");
        if (errors != responseJson.end()) {
            auto otpError = errors->find("otp");
            if (otpError != errors->end()) {
                auto errorText = otpError->at(0).get<std::string>();
                LOG_ERROR("One time password error: %s", errorText.c_str());
                return new caff_AuthResponse{ nullptr, cstrdup("mfa_otp_required"), nullptr };
            }
            auto errorText = errors->at("_error").at(0).get<std::string>();
            LOG_ERROR("Error logging in: %s", errorText.c_str());
            return nullptr;
        }

        std::string next;
        std::string mfaOtpMethod;
        Credentials * creds = nullptr;

        auto credsIt = responseJson.find("credentials");
        if (credsIt != responseJson.end()) {
            creds = new Credentials{ *credsIt };
            LOG_DEBUG("Sign-in complete");
        }

        auto nextIt = responseJson.find("next");
        if (nextIt != responseJson.end()) {
            nextIt->get_to(next);
        }

        auto methodIt = responseJson.find("mfa_otp_method");
        if (methodIt != responseJson.end()) {
            methodIt->get_to(mfaOtpMethod);
            LOG_DEBUG("MFA required");
        }

        if (credsIt == responseJson.end() && nextIt == responseJson.end() && methodIt == responseJson.end()) {
            LOG_ERROR("Sign-in response missing");
        }

        return new caff_AuthResponse{
            reinterpret_cast<caff_CredentialsHandle>(creds),
            cstrdup(next),
            cstrdup(mfaOtpMethod)
        };
    }

    caff_AuthResponse * signin(char const * username, char const * password, char const * otp)
    {
        RETRY_REQUEST(caff_AuthResponse*, doSignin(username, password, otp));
    }

    static Credentials * doRefreshAuth(char const * refreshToken)
    {
        TRACE();

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
            return nullptr;
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
            return nullptr;
        }

        auto errorsIt = responseJson.find("errors");
        if (errorsIt != responseJson.end()) {
            auto errorText = errorsIt->at("_error").at(0).get<std::string>();
            LOG_ERROR("Error refreshing tokens: %s", errorText.c_str());
            return nullptr;
        }

        auto credsIt = responseJson.find("credentials");
        if (credsIt != responseJson.end()) {
            LOG_DEBUG("Sign-in complete");
            return new Credentials{ *credsIt };
        }

        LOG_ERROR("Failed to extract response info");
        return nullptr;
    }

    Credentials * refreshAuth(char const * refreshToken)
    {
        RETRY_REQUEST(Credentials *, doRefreshAuth(refreshToken));
    }

    static bool doRefreshCredentials(Credentials * creds)
    {
        TRACE();
        std::string refreshToken;
        {
            std::lock_guard<std::mutex> lock(creds->mutex);
            refreshToken = creds->refreshToken;
        }

        Credentials * newCreds = doRefreshAuth(refreshToken.c_str());
        if (!newCreds) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(creds->mutex);
            std::swap(creds->accessToken, newCreds->accessToken);
            std::swap(creds->caid, newCreds->caid);
            std::swap(creds->refreshToken, newCreds->refreshToken);
            std::swap(creds->credential, newCreds->credential);
        }

        // TODO see if this is needed (need a body for post)
        delete newCreds;
        return true;
    }

    bool refreshCredentials(Credentials * creds)
    {
        RETRY_REQUEST(bool, doRefreshCredentials(creds));
    }

    static caff_UserInfo * doGetUserInfo(Credentials * creds)
    {
        TRACE();
        if (creds == nullptr) {
            LOG_ERROR("Missing credentials");
            return nullptr;
        }

        ScopedCurl curl(CONTENT_TYPE_JSON, creds);

        auto urlStr = GETUSER_URL(creds->caid);
        curl_easy_setopt(curl, CURLOPT_URL, urlStr.c_str());

        std::string responseStr;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&responseStr);

        char curlError[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

        CURLcode curlResult = curl_easy_perform(curl);
        if (curlResult != CURLE_OK) {
            LOG_ERROR("HTTP failure fetching user: [%d] %s", curlResult, curlError);
            return nullptr;
        }

        Json responseJson;
        try {
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to parse user response");
            return nullptr;
        }

        auto errorsIt = responseJson.find("errors");
        if (errorsIt != responseJson.end()) {
            auto errorText = errorsIt->at("_error").at(0).get<std::string>();
            LOG_ERROR("Error fetching user: %s", errorText.c_str());
            return nullptr;
        }

        auto userIt = responseJson.find("user");
        if (userIt != responseJson.end()) {
            LOG_DEBUG("Got user details");
            return new caff_UserInfo(*userIt);
        }

        LOG_ERROR("Failed to get user info");
        return nullptr;
    }

    caff_UserInfo * getUserInfo(Credentials * creds)
    {
        RETRY_REQUEST(caff_UserInfo*, doGetUserInfo(creds));
    }

    static caff_GameList * doGetSupportedGames()
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
            return nullptr;
        }

        Json responseJson;
        try {
            // TODO see if this is needed (need a body for post)
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to parse game list response");
            return nullptr;
        }

        if (!responseJson.is_array()) {
            LOG_ERROR("Unable to retrieve games list");
            return nullptr;
        }

        return new caff_GameList(responseJson);
    }

    caff_GameList * getSupportedGames()
    {
        RETRY_REQUEST(caff_GameList *, doGetSupportedGames());
    }

    static bool doTrickleCandidates(
        std::vector<IceInfo> const & candidates,
        std::string const & streamUrl,
        Credentials * creds)
    {
        TRACE();
        Json requestJson = { {"ice_candidates", candidates} }; // TODO see if this is needed (need a body for post)

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
            LOG_INFO("Unauthorized - refreshing credentials");
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

    bool trickleCandidates(std::vector<IceInfo> const & candidates, std::string const & streamUrl, Credentials * creds)
    {
        RETRY_REQUEST(bool, doTrickleCandidates(candidates, streamUrl, creds));
    }
    /*
    static HeartbeatResponse * do_caffeine_heartbeat_stream(char const * streamUrl, Credentials * creds)
    {
        TRACE();

        ScopedCurl curl(CONTENT_TYPE_JSON, creds);

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
            return nullptr;
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        if (responseCode == 401) {
            LOG_INFO("Unauthorized - refreshing credentials");
            if (refreshCredentials(creds)) {
                return do_caffeine_heartbeat_stream(streamUrl, creds);
            }
            return nullptr;
        }

        if (responseCode != 200) {
            LOG_ERROR("Error heartbeating stream: %ld", responseCode);
            return nullptr;
        }

        Json responseJson;
        try {
            responseJson = Json::parse(responseStr);
        }
        catch (...) {
            LOG_ERROR("Failed to parse refresh response");
            return nullptr;
        }

        LOG_DEBUG("Stream heartbeat succeeded");
        return new HeartbeatResponse{
            cstrdup(responseJson.at("connection_quality").get<std::string>())
        };
    }

    HeartbeatResponse * caff_heartbeat_stream(char const * streamUrl, Credentials * creds)
    {
        RETRY_REQUEST(HeartbeatResponse *, do_caffeine_heartbeat_stream(streamUrl, creds));
    }

    void caff_free_heartbeat_response(HeartbeatResponse ** response)
    {
        if (!response || !*response) {
            return;
        }

        delete[](*response)->connection_quality;
        delete *response;
        *response = nullptr;
    }

    static bool do_update_broadcast_screenshot(
        char const * broadcastId,
        uint8_t const * screenshot_data,
        size_t screenshot_size,
        Credentials * creds)
    {
        TRACE();
        ScopedCurl curl(CONTENT_TYPE_FORM, creds);

        curl_httppost * post = nullptr;
        curl_httppost * last = nullptr;
        ScopedPost cleanup(post);

        if (screenshot_data) {
            curl_formadd(&post, &last,
                CURLFORM_COPYNAME, "broadcast[game_image]",
                CURLFORM_BUFFER, "game_image.jpg",
                CURLFORM_BUFFERPTR, screenshot_data,
                CURLFORM_BUFFERLENGTH, screenshot_size,
                CURLFORM_CONTENTTYPE, "image/jpeg",
                CURLFORM_END);
        }

        curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
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

    bool caff_update_broadcast_screenshot(
        char const * broadcastId,
        uint8_t const * screenshot_data,
        size_t screenshot_size,
        Credentials * creds)
    {
        if (!broadcastId) {
            LOG_ERROR("Passed in nullptr broadcastId");
            return false;
        }

        RETRY_REQUEST(bool, do_update_broadcast_screenshot(broadcastId, screenshot_data, screenshot_size, creds));
    }
    */

    StageRequest * createStageRequest(std::string username, std::string clientId)
    {
        auto request = new StageRequest{};
        request->client.id = std::move(clientId);
        request->stage = Stage{};
        request->stage->username = username;
        return request;
    }

    static bool isOutOfCapacityFailure(std::string const & type)
    {
        return type == "OutOfCapacity";
    }

    static optional<StageResponseResult> doStageUpdate(
        StageRequest const & request,
        Credentials * creds)
    {
        TRACE();

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
            LOG_INFO("Unauthorized - refreshing credentials");
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
            LOG_ERROR("Failed to deserialized stage update response to JSON: %s", json_error.text);
            return {};
        }

        if (responseCode == 200) {
            try {
                return StageResponseResult{ responseJson.get<StageResponse>(), {} };
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
                if (!isOutOfCapacityFailure(type)) {
                    return {};
                }

                auto messageJson = responseJson.value("display_message", Json::object());
                return StageResponseResult{
                    {},
                    FailureResponse{
                        std::move(type),
                        responseJson.value("reason", ""),
                        DisplayMessage{
                            messageJson.value("title", ""),
                            messageJson.at("body").get<std::string>(),
                        }} };
            }
            catch (...) {
                LOG_ERROR("Failed to unpack failure response");
                return {};
            }
        }

        return {};
    }

    optional<StageResponseResult> stageUpdate(StageRequest const & request, Credentials * creds)
    {
        RETRY_REQUEST(optional<StageResponseResult>, doStageUpdate(request, creds));
    }

    bool requestStageUpdate(
        StageRequest * request,
        Credentials * creds,
        double * retryIn,
        bool * isOutOfCapacity)
    {
        auto result = stageUpdate(*request, creds);

        bool success = result && result->response;
        if (success) {
            if (retryIn) {
                *retryIn = result->response->retryIn;
            }
            request->cursor = std::move(result->response->cursor);
            request->stage = std::move(result->response->stage);
        }
        else if (isOutOfCapacity
            && result
            && result->failure
            && isOutOfCapacityFailure(result->failure->type))
        {
            *isOutOfCapacity = true;
        }

        return success;
    }

}
