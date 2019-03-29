#define LIBCAFFEINE_LIBRARY

#include "caffeine-api.hpp"
#include "iceinfo.hpp"

#include "nlohmann/json.hpp"

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
#define log_error(...) ((void)0)
#define log_warn(...) ((void)0)
#define log_debug(...) ((void)0)
#define log_info(...) ((void)0)
#define trace() ((void)0)

using namespace caff;
using json = nlohmann::json;

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
 * Error type: (various json formats, occasionally HTML e.g. for 502)
 */

 // RAII helper to get rid of gotos
 // TODO use a C++ HTTP library
class ScopedCURL {
public:
    ScopedCURL(char const * contentType)
        : curl(curl_easy_init()), headers(basicHeaders(contentType))
    {
        applyHeaders();
    }

    ScopedCURL(char const * contentType, Credentials * creds)
        : curl(curl_easy_init()), headers(authenticatedHeaders(contentType, creds))
    {
        applyHeaders();
    }

    virtual ~ScopedCURL() {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    operator CURL *() { return curl; }

private:
    CURL * curl;
    curl_slist * headers;

    void applyHeaders() {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    static curl_slist * basicHeaders(char const * content_type) {
        curl_slist * headers = nullptr;
        headers = curl_slist_append(headers, content_type);
        headers = curl_slist_append(headers, "X-Client-Type: obs"); // TODO client type parameter
        headers = curl_slist_append(headers, "X-Client-Version: " API_VERSION);
        return headers;
    }

    static curl_slist * authenticatedHeaders(
        char const * content_type,
        Credentials * creds)
    {
        std::string authorization("Authorization: Bearer ");
        std::string credential("X-Credential: ");

        {
            std::lock_guard<std::mutex> lock(creds->mutex);
            authorization += creds->access_token;
            credential += creds->credential;
        }

        curl_slist * headers = basicHeaders(content_type);
        headers = curl_slist_append(headers, authorization.c_str());
        headers = curl_slist_append(headers, credential.c_str());
        return headers;
    }
};

/* TODO holdovers from C */
static char * cstrdup(char const * str) {
    if (!str)
        return nullptr;

    auto copylen = strlen(str) + 1;
    auto result = new char[copylen];
    strncpy(result, str, copylen);
    return result;
}

static char * cstrdup(std::string const & str) {
    if (str.empty())
        return nullptr;

    auto result = new char[str.length() + 1];
    str.copy(result, str.length());
    result[str.length()] = 0;
    return result;
}

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

static size_t caffeine_curl_write_callback(char * ptr, size_t size,
    size_t nmemb, void * user_data)
{
    UNUSED_PARAMETER(size);
    if (nmemb == 0)
        return 0;

    std::string & result_str = *(reinterpret_cast<std::string *>(user_data));
    result_str.append(ptr, nmemb);
    return nmemb;
}

char const * refresh_token(Credentials * handle)
{
    if (!handle) return nullptr;

    auto creds = reinterpret_cast<Credentials *>(handle);
    return creds->refresh_token.c_str();
}

#define RETRY_MAX 3

/* TODO: this is not very robust or intelligent. Some kinds of failure willarray
 * never be recoverable and should not be retried
 */
#define retry_request(result_t, request) \
        for (int try_num = 0; try_num < RETRY_MAX; ++try_num) { \
            result_t result = request; \
            if (result) \
                return result; \
            std::this_thread::sleep_for(std::chrono::seconds(1 + 1 * try_num)); \
        } \
        return {}

static bool do_caffeine_is_supported_version()
{
    trace();

    ScopedCURL curl(CONTENT_TYPE_JSON);

    curl_easy_setopt(curl, CURLOPT_URL, VERSION_CHECK_URL);

    std::string response_str;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        caffeine_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response_str);

    char curl_error[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log_error("HTTP failure checking version: [%d] %s", res, curl_error);
        return false;
    }

    json response_json;
    try {
        response_json = json::parse(response_str);
    }
    catch (...) {
        log_error("Failed to parse version check response");
        return false;
    }

    auto errors = response_json.find("errors");
    if (errors != response_json.end()) {
        auto error_text = errors->at("_expired").at(0).get<std::string>();
        log_error("%s", error_text.c_str());
        return false;
    }

    return true;
}

CAFFEINE_API bool caff_is_supported_version() {
    retry_request(bool, do_caffeine_is_supported_version());
}

/* TODO: refactor this - lots of dupe code between request types
 * TODO: reuse curl handle across requests
 */
static caff_auth_response * do_caffeine_signin(
    char const * username,
    char const * password,
    char const * otp)
{
    trace();
    json request_json;

    try {
        request_json = {
            {"account", {
                {"username", username},
                {"password", password},
            }}
        };

        if (otp && otp[0]) {
            request_json.push_back({ "mfa", {{"otp", otp}} });
        }
    }
    catch (...) {
        log_error("Failed to create request JSON");
        return nullptr;
    }

    auto request_body = request_json.dump();

    ScopedCURL curl(CONTENT_TYPE_JSON);

    curl_easy_setopt(curl, CURLOPT_URL, SIGNIN_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());

    std::string response_str;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, caffeine_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response_str);

    char curl_error[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log_error("HTTP failure signing in: [%d] %s", res, curl_error);
        return nullptr;
    }

    json response_json;
    try {
        response_json = json::parse(response_str);
    }
    catch (...) {
        log_error("Failed to parse signin response");
        return nullptr;
    }

    auto errors = response_json.find("errors");
    if (errors != response_json.end()) {
        auto otpError = errors->find("otp");
        if (otpError != errors->end()) {
            auto error_text = otpError->at(0).get<std::string>();
            log_error("One time password error: %s", error_text.c_str());
            return new caff_auth_response{
                nullptr,
                cstrdup("mfa_otp_required"),
                nullptr
            };
        }
        auto error_text = errors->at("_error").at(0).get<std::string>();
        log_error("Error logging in: %s", error_text.c_str());
        return nullptr;
    }

    std::string next;
    std::string mfa_otp_method;
    Credentials * creds = nullptr;

    auto credsIt = response_json.find("credentials");
    if (credsIt != response_json.end()) {
        creds = new Credentials(
            credsIt->at("access_token").get<std::string>(),
            credsIt->at("refresh_token").get<std::string>(),
            credsIt->at("caid").get<std::string>(),
            credsIt->at("credential").get<std::string>());
        log_debug("Sign-in complete");
    }

    auto nextIt = response_json.find("next");
    if (nextIt != response_json.end()) {
        nextIt->get_to(next);
    }

    auto methodIt = response_json.find("mfa_otp_method");
    if (methodIt != response_json.end()) {
        methodIt->get_to(mfa_otp_method);
        log_debug("MFA required");
    }

    if (credsIt == response_json.end()
        && nextIt == response_json.end()
        && methodIt == response_json.end()) {
        log_error("Sign-in response missing");
    }

    return new caff_auth_response{
        reinterpret_cast<caff_credentials_handle>(creds),
        cstrdup(next),
        cstrdup(mfa_otp_method)
    };
}

CAFFEINE_API caff_auth_response * caff_signin(
    char const * username,
    char const * password,
    char const * otp)
{
    retry_request(caff_auth_response*, do_caffeine_signin(username, password, otp));
}

static Credentials * do_caffeine_refresh_auth(
    char const * refresh_token)
{
    trace();

    json request_json = { {"refresh_token", refresh_token} };

    auto request_body = request_json.dump();

    ScopedCURL curl(CONTENT_TYPE_JSON);

    curl_easy_setopt(curl, CURLOPT_URL, REFRESH_TOKEN_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());

    std::string response_str;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, caffeine_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response_str);

    char curl_error[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log_error("HTTP failure refreshing tokens: [%d] %s", res, curl_error);
        return nullptr;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    log_debug("Http response [%ld]", response_code);

    json response_json;
    try {
        response_json = json::parse(response_str);
    }
    catch (...) {
        log_error("Failed to parse refresh response");
        return nullptr;
    }

    auto errorsIt = response_json.find("errors");
    if (errorsIt != response_json.end()) {
        auto errorText = errorsIt->at("_error").at(0).get<std::string>();
        log_error("Error refreshing tokens: %s", errorText.c_str());
        return nullptr;
    }

    auto credsIt = response_json.find("credentials");
    if (credsIt != response_json.end()) {
        log_debug("Sign-in complete");
        return new Credentials(
            credsIt->at("access_token").get<std::string>(),
            credsIt->at("refresh_token").get<std::string>(),
            credsIt->at("caid").get<std::string>(),
            credsIt->at("credential").get<std::string>());
    }

    log_error("Failed to extract response info");
    return nullptr;
}

Credentials * refresh_auth(
    char const * refresh_token)
{
    retry_request(Credentials *, do_caffeine_refresh_auth(refresh_token));
}

void free_credentials(Credentials ** handle)
{
    trace();
    if (!handle || !*handle)
        return;
    Credentials ** credentials = handle;

    {
        std::lock_guard<std::mutex> lock((*credentials)->mutex);
        (*credentials)->access_token.erase();
        (*credentials)->caid.erase();
        (*credentials)->refresh_token.erase();
        (*credentials)->credential.erase();
    }
    delete *credentials;
    *handle = nullptr;
}

CAFFEINE_API void caff_free_auth_response(caff_auth_response ** auth_response)
{
    trace();
    if (!auth_response || !*auth_response)
        return;

    caff_free_credentials(&(*auth_response)->credentials);

    delete[](*auth_response)->next;
    delete[](*auth_response)->mfa_otp_method;

    delete *auth_response;
    *auth_response = nullptr;
}

static bool do_refresh_credentials(Credentials * creds)
{
    trace();
    std::string refresh_token;
    {
        std::lock_guard<std::mutex> lock(creds->mutex);
        refresh_token = creds->refresh_token;
    }

    Credentials * new_creds = refresh_auth(refresh_token.c_str());

    if (!new_creds)
        return false;

    {
        std::lock_guard<std::mutex> lock(creds->mutex);
        std::swap(creds->access_token, new_creds->access_token);
        std::swap(creds->caid, new_creds->caid);
        std::swap(creds->refresh_token, new_creds->refresh_token);
        std::swap(creds->credential, new_creds->credential);
    }

    // TODO see if this is needed (need a body for post)
    free_credentials(&new_creds);
    return true;
}

bool refresh_credentials(Credentials * creds)
{
    retry_request(bool, do_refresh_credentials(creds));
}

static caff_user_info * do_caffeine_getuser(Credentials * creds)
{
    trace();
    if (creds == nullptr) {
        log_error("Missing credentials");
        return nullptr;
    }

    ScopedCURL curl(CONTENT_TYPE_JSON, creds);

    auto url_str = GETUSER_URL(creds->caid);
    curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());

    std::string response_str;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, caffeine_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response_str);

    char curl_error[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log_error("HTTP failure fetching user: [%d] %s", res, curl_error);
        return nullptr;
    }

    json response_json;
    try {
        response_json = json::parse(response_str);
    }
    catch (...) {
        log_error("Failed to parse user response");
        return nullptr;
    }

    auto errorsIt = response_json.find("errors");
    if (errorsIt != response_json.end()) {
        auto errorText = errorsIt->at("_error").at(0).get<std::string>();
        log_error("Error fetching user: %s", errorText.c_str());
        return nullptr;
    }

    auto userIt = response_json.find("user");
    if (userIt != response_json.end()) {
        log_debug("Got user details");
        return new caff_user_info{
            cstrdup(userIt->at("caid").get<std::string>()),
            cstrdup(userIt->at("username").get<std::string>()),
            cstrdup(userIt->at("stage_id").get<std::string>()),
            userIt->at("can_broadcast").get<bool>()
        };
    }

    log_error("Failed to get user info");
    return nullptr;
}

caff_user_info * getuser(Credentials * creds)
{
    retry_request(caff_user_info*, do_caffeine_getuser(creds));
}

CAFFEINE_API void caff_free_user_info(caff_user_info ** user_info)
{
    trace();
    if (!user_info || !*user_info)
        return;

    delete[](*user_info)->caid;
    delete[](*user_info)->username;
    delete[](*user_info)->stage_id;
    delete *user_info;
    *user_info = nullptr;
}

static caff_games * do_caffeine_get_supported_games()
{
    caff_games * response = nullptr;

    ScopedCURL curl(CONTENT_TYPE_JSON);

    curl_easy_setopt(curl, CURLOPT_URL, GETGAMES_URL);

    std::string response_str;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, caffeine_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response_str);

    char curl_error[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log_error("HTTP failure fetching supported games: [%d] %s", res, curl_error);
        return nullptr;
    }

    json response_json;
    try {
        // TODO see if this is needed (need a body for post)
        response_json = json::parse(response_str);
    }
    catch (...) {
        log_error("Failed to parse game list response");
        return nullptr;
    }

    if (!response_json.is_array())
    {
        log_error("Unable to retrieve games list");
        return nullptr;
    }

    auto num_games = response_json.size();
    response = new caff_games{
        new caff_game_info *[num_games] {0},
        num_games
    };

    for (size_t gameIndex = 0; gameIndex < num_games; ++gameIndex) {
        auto const & value = response_json[gameIndex];

        try {
            auto id_num = value.at("id").get<int>();
            auto name = value.at("name").get<std::string>();
            auto const & process_names = value["process_names"];
            auto num_processes = process_names.size();
            if (num_processes == 0) {
                log_warn("No process names found for %s; ignoring", name.c_str());
                continue;
            }

            std::ostringstream id_stream;
            id_stream << id_num;

            auto info = new caff_game_info{
                cstrdup(id_stream.str()),
                cstrdup(name),
                new char *[num_processes] {0},
                num_processes
            };

            for (size_t processIndex = 0; processIndex < num_processes; ++processIndex) {
                try {
                    info->process_names[processIndex] = cstrdup(process_names.at(processIndex).get<std::string>());
                }
                catch (...) {
                    log_warn("Unable to read process name; ignoring");
                }
            }

            response->game_infos[gameIndex] = info;
        }
        catch (...) {
            log_warn("Unable to parse game list entry; ignoring");
        }
    }

    return response;
}

CAFFEINE_API caff_games * caff_get_supported_games()
{
    retry_request(caff_games *,
        do_caffeine_get_supported_games());
}

CAFFEINE_API void caff_free_game_info(caff_game_info ** info)
{
    if (!info || !*info)
        return;

    for (size_t i = 0; i < (*info)->num_process_names; ++i) {
        delete[](*info)->process_names[i];
        (*info)->process_names[i] = nullptr;
    }
    delete[](*info)->id;
    delete[](*info)->name;
    delete[](*info)->process_names;
    delete *info;
    *info = nullptr;
}

CAFFEINE_API void caff_free_game_list(caff_games ** games)
{
    trace();
    if (!games || !*games)
        return;

    for (size_t i = 0; i < (*games)->num_games; ++i) {
        caff_free_game_info(&(*games)->game_infos[i]);
    }

    delete[](*games)->game_infos;
    delete *games;
    *games = nullptr;
}

static bool do_caffeine_trickle_candidates(
    std::vector<IceInfo> const & candidates,
    std::string const & streamUrl,
    Credentials * creds)
{
    trace();
    auto ice_candidates = json::array({});
    for (auto const & candidate : candidates)
    {
        ice_candidates.push_back({
            {"candidate", candidate.sdp},
            {"sdpMid", candidate.sdpMid},
            {"sdpMLineIndex", candidate.sdpMLineIndex},
            });
    }

    json request_json = {
        {"ice_candidates", ice_candidates}
        // TODO see if this is needed (need a body for post)
    };

    std::string request_body = request_json.dump();

    ScopedCURL curl(CONTENT_TYPE_JSON, creds);

    curl_easy_setopt(curl, CURLOPT_URL, streamUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

    char curl_error[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log_error("HTTP failure negotiating ICE: [%d] %s", res, curl_error);
        return false;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    bool response = false;

    switch (response_code) {
    case 200:
        response = true;
        break;
    case 401:
        log_info("Unauthorized - refreshing credentials");
        if (refresh_credentials(creds)) {
            response = do_caffeine_trickle_candidates(candidates, streamUrl, creds);
        }
        break;
    default:
        break;
    }

    if (response) {
        log_debug("ICE candidates trickled");
    }
    else {
        log_error("Error negotiating ICE candidates");
    }

    return response;
}

bool caff_trickle_candidates(
    std::vector<IceInfo> const & candidates,
    std::string const & streamUrl,
    Credentials * creds)
{
    retry_request(bool,
        do_caffeine_trickle_candidates(candidates, streamUrl, creds));
}

static HeartbeatResponse * do_caffeine_heartbeat_stream(
    char const * streamUrl,
    Credentials * creds)
{
    trace();

    ScopedCURL curl(CONTENT_TYPE_JSON, creds);

    auto url = STREAM_HEARTBEAT_URL(streamUrl);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}"); // TODO: is this necessary?
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

    std::string response_str;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, caffeine_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response_str);

    char curl_error[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log_error("HTTP failure hearbeating stream: [%d] %s", res, curl_error);
        return nullptr;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code == 401) {
        log_info("Unauthorized - refreshing credentials");
        if (refresh_credentials(creds)) {
            return do_caffeine_heartbeat_stream(streamUrl, creds);
        }
        return nullptr;
    }

    if (response_code != 200) {
        log_error("Error heartbeating stream: %ld", response_code);
        return nullptr;
    }

    json response_json;
    try {
        response_json = json::parse(response_str);
    }
    catch (...) {
        log_error("Failed to parse refresh response");
        return nullptr;
    }

    log_debug("Stream heartbeat succeeded");
    return new HeartbeatResponse{
        cstrdup(response_json.at("connection_quality").get<std::string>())
    };
}

HeartbeatResponse * caff_heartbeat_stream(
    char const * streamUrl,
    Credentials * creds)
{
    retry_request(
        HeartbeatResponse *,
        do_caffeine_heartbeat_stream(streamUrl, creds));
}

void caff_free_heartbeat_response(
    HeartbeatResponse ** response)
{
    if (!response || !*response) {
        return;
    }

    delete[](*response)->connection_quality;
    delete *response;
    *response = nullptr;
}

// RAII helper
class CleanupPost {
public:
    CleanupPost(curl_httppost * post) : post(post) {}
    virtual ~CleanupPost() { curl_formfree(post); }
private:
    curl_httppost * post;
};

static bool do_update_broadcast_screenshot(
    char const * broadcast_id,
    uint8_t const * screenshot_data,
    size_t screenshot_size,
    Credentials * creds)
{
    trace();
    ScopedCURL curl(CONTENT_TYPE_FORM, creds);

    curl_httppost * post = nullptr;
    curl_httppost * last = nullptr;
    CleanupPost cleanup(post);

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

    auto url = BROADCAST_URL(broadcast_id);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    char curl_error[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log_error("HTTP failure updating broadcast screenshot: [%d] %s", res, curl_error);
        return false;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    log_debug("Http response code [%ld]", response_code);

    bool result = response_code / 100 == 2;

    if (!result) {
        log_error("Failed to update broadcast screenshot");
    }

    return result;
}

bool caff_update_broadcast_screenshot(
    char const * broadcast_id,
    uint8_t const * screenshot_data,
    size_t screenshot_size,
    Credentials * creds)
{
    if (!broadcast_id) {
        log_error("Passed in nullptr broadcast_id");
        return false;
    }

    retry_request(bool,
        do_update_broadcast_screenshot(
            broadcast_id,
            screenshot_data,
            screenshot_size,
            creds));
}

StageRequest * caff_create_stage_request(std::string username, std::string client_id)
{
    auto request = new StageRequest{};
    request->client.id = std::move(client_id);
    request->stage = Stage{};
    request->stage->username = username;
    return request;
}

namespace nlohmann {
    template <typename T>
    struct adl_serializer<absl::optional<T>> {
        static void to_json(json& j, const absl::optional<T>& opt) {
            if (opt == absl::optional<T>{}) {
                j = nullptr;
            }
            else {
                j = *opt;
            }
        }

        static void from_json(const json& j, absl::optional<T>& opt) {
            if (j.is_null()) {
                opt = absl::optional<T>{};
            }
            else {
                opt = j.get<T>();
            }
        }
    };
}

template <typename T>
void get_value(json const & j, char const * key, T & target)
{
    j.at(key).get_to(target);
}

template <typename T>
void get_value(json const & j, char const * key, absl::optional<T> & target)
{
    auto it = j.find(key);
    if (it != j.end()) {
        it->get_to(target);
    }
    else {
        target.reset();
    }
}

template <typename T>
void put_value(json & j, char const * key, T const & source)
{
    j[key] = source;
}

template <typename T>
void put_value(json & j, char const * key, absl::optional<T> const & source)
{
    if (source) {
        put_value(j, key, *source);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(ContentType, {
    {ContentType::Game, "game"},
    {ContentType::User, "user"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(caff_connection_quality, {
    {CAFF_CONNECTION_QUALITY_GOOD, "GOOD"},
    {CAFF_CONNECTION_QUALITY_BAD, "BAD"},
    {CAFF_CONNECTION_QUALITY_UNKNOWN, nullptr},
})

void to_json(json & j, Client const & client)
{
    put_value(j, "id", client.id);
    put_value(j, "headless", client.headless);
    put_value(j, "constrained_baseline", client.constrainedBaseline);
}

void to_json(json & j, FeedCapabilities const & capabilities)
{
    put_value(j, "video", capabilities.video);
    put_value(j, "audio", capabilities.audio);
}

void to_json(json & j, FeedContent const & content)
{
    put_value(j, "id", content.id);
    put_value(j, "type", content.type);
}

void to_json(json & j, FeedStream const & stream)
{
    put_value(j, "id", stream.id);
    put_value(j, "source_id", stream.source_id);
    put_value(j, "url", stream.url);
    put_value(j, "sdp_offer", stream.sdp_offer);
    put_value(j, "sdp_answer", stream.sdp_answer);
}

void to_json(json & j, Feed const & feed)
{
    put_value(j, "id", feed.id);
    put_value(j, "client_id", feed.client_id);
    put_value(j, "role", feed.role);
    put_value(j, "description", feed.description);
    put_value(j, "source_connection_quality", feed.source_connection_quality);
    put_value(j, "volume", feed.volume);
    put_value(j, "capabilities", feed.capabilities);
    put_value(j, "content", feed.content);
    put_value(j, "stream", feed.stream);
}

void to_json(json & j, Stage const & stage)
{
    put_value(j, "id", stage.id);
    put_value(j, "username", stage.username);
    put_value(j, "title", stage.title);
    put_value(j, "broadcast_id", stage.broadcast_id);
    put_value(j, "upsert_broadcast", stage.upsert_broadcast);
    put_value(j, "live", stage.live);
    put_value(j, "feeds", stage.feeds);
}

void to_json(json & j, StageRequest const & request)
{
    put_value(j, "client", request.client);
    put_value(j, "cursor", request.cursor);
    put_value(j, "payload", request.stage);
}

void from_json(json const & j, FeedStream & stream)
{
    get_value(j, "id", stream.id);
    get_value(j, "source_id", stream.source_id);
    get_value(j, "url", stream.url);
    get_value(j, "sdp_offer", stream.sdp_offer);
    get_value(j, "sdp_answer", stream.sdp_answer);
}

void from_json(json const & j, FeedContent & content)
{
    get_value(j, "id", content.id);
    get_value(j, "type", content.type);
}

void from_json(json const & j, FeedCapabilities & capabilities)
{
    get_value(j, "audio", capabilities.audio);
    get_value(j, "video", capabilities.video);
}

void from_json(json const & j, Feed & feed)
{
    get_value(j, "id", feed.id);
    get_value(j, "client_id", feed.client_id);
    get_value(j, "role", feed.role);
    get_value(j, "description", feed.description);
    get_value(j, "source_connection_quality", feed.source_connection_quality);
    get_value(j, "volume", feed.volume);
    get_value(j, "capabilities", feed.capabilities);
    get_value(j, "content", feed.content);
    get_value(j, "stream", feed.stream);
}

void from_json(json const & j, Stage & stage)
{
    get_value(j, "id", stage.id);
    get_value(j, "username", stage.username);
    get_value(j, "title", stage.title);
    get_value(j, "broadcast_id", stage.broadcast_id);
    get_value(j, "upsert_broadcast", stage.upsert_broadcast);
    get_value(j, "live", stage.live);
    get_value(j, "feeds", stage.feeds);
}

void from_json(json const & j, StageResponse & response)
{
    get_value(j, "cursor", response.cursor);
    get_value(j, "retry_in", response.retry_in);
    get_value(j, "payload", response.stage);
}

void from_json(json const & j, DisplayMessage & message)
{
    get_value(j, "title", message.title);
    get_value(j, "body", message.body);
};

void from_json(json const & j, FailureResponse & response)
{
    get_value(j, "type", response.type);
    get_value(j, "reason", response.reason);
    get_value(j, "display_message", response.display_message);
};

static bool is_out_of_capacity_failure_type(std::string const & type)
{
    return type == "OutOfCapacity";
}

static absl::optional<StageResponseResult> do_caffeine_stage_update(
    StageRequest const & request,
    Credentials * creds)
{
    trace();

    if (!request.stage ||
        !request.stage->username ||
        request.stage->username->empty()) {
        log_error("Did not set request username");
        return {};
    }

    json request_json = request;
    auto request_body = request_json.dump();

    ScopedCURL curl(CONTENT_TYPE_JSON, creds);

    auto url_str = STAGE_UPDATE_URL(*request.stage->username);

    curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

    std::string response_str;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, caffeine_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response_str);

    char curl_error[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log_error("HTTP failure performing stage update: [%d] %s", res, curl_error);
        return {};
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    log_debug("Http response [%ld]", response_code);

    if (response_code == 401) {
        log_info("Unauthorized - refreshing credentials");
        if (refresh_credentials(creds)) {
            return do_caffeine_stage_update(request, creds);
        }
        return {};
    }

    json response_json;
    try {
        response_json = json::parse(response_str);
    }
    catch (...) {
        log_error("Failed to deserialized stage update response to JSON: %s",
            json_error.text);
        return {};
    }

    if (response_code == 200) {
        try {
            return StageResponseResult{ response_json.get<StageResponse>(), {} };
        }
        catch (...) {
            log_error("Failed to unpack stage response");
            return {};
        }
    }
    else {
        try {
            auto type = response_json.at("type").get<std::string>();

            // As of now, the only failure response we want to return and not retry is `OutOfCapacity`
            if (!is_out_of_capacity_failure_type(type)) {
                return {};
            }

            auto message_json = response_json.value("display_message", json::object());
            return StageResponseResult{
                {},
                FailureResponse{
                    std::move(type),
                    response_json.value("reason", ""),
                    DisplayMessage{
                        message_json.value("title", ""),
                        message_json.at("body").get<std::string>(),
                    }} };
        }
        catch (...) {
            log_error("Failed to unpack failure response");
            return {};
        }
    }

    return {};
}

absl::optional<StageResponseResult> caffeine_stage_update(
    StageRequest const & request,
    Credentials * creds)
{
    retry_request(absl::optional<StageResponseResult>,
        do_caffeine_stage_update(request, creds));
}

bool caff_request_stage_update(
    StageRequest * request,
    Credentials * creds,
    double * retry_in,
    bool * is_out_of_capacity)
{
    auto result = caffeine_stage_update(*request, creds);

    bool success = result && result->response;
    if (success) {
        if (retry_in) {
            *retry_in = result->response->retry_in;
        }
        request->cursor = std::move(result->response->cursor);
        request->stage = std::move(result->response->stage);
    }
    else if (is_out_of_capacity
        && result
        && result->failure
        && is_out_of_capacity_failure_type(result->failure->type))
    {
        *is_out_of_capacity = true;
    }

    return success;
}
