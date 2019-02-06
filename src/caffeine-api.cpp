#include "caffeine-api.hpp"

#include "nlohmann/json.hpp"

#include <curl/curl.h>
#include <mutex>
#include <sstream>
#include <thread>
#include <chrono>


using json = nlohmann::json;

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
#define STREAM_HEARTBEAT_URL(stream_url)  ((stream_url) + std::string("/heartbeat"))

#define CONTENT_TYPE_JSON  "Content-Type: application/json"
#define CONTENT_TYPE_FORM  "Content-Type: multipart/form-data"


// TODO make these into something else?
#define log_error(...) ((void)0)
#define log_warn(...) ((void)0)
#define log_debug(...) ((void)0)
#define log_info(...) ((void)0)
#define trace() ((void)0)

/*
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

struct caff_credentials {
    std::string access_token;
    std::string refresh_token;
    std::string caid;
    std::string credential;

    std::mutex mutex;
};

struct caffeine_stage_response {
    std::string cursor;
    double retry_in;
    std::unique_ptr<caff_stage> stage;

    caffeine_stage_response(std::string const & cursor, double retry_in, std::unique_ptr<caff_stage> && stage)
        : cursor(cursor), retry_in(retry_in), stage(std::move(stage))
    {}
};

struct caffeine_display_message {
    std::string title;
    std::string body;
};

struct caffeine_failure_response {
    std::string type;
    std::string reason;
    struct caffeine_display_message display_message;

    caffeine_failure_response(
        std::string const & type,
        std::string const & reason,
        struct caffeine_display_message const &display_message)
        : type(type), reason(reason), display_message(display_message)
    {}
};

struct caffeine_stage_response_result {
    std::unique_ptr<caffeine_stage_response> response;
    std::unique_ptr<caffeine_failure_response> failure;

    caffeine_stage_response_result(
        std::unique_ptr<caffeine_stage_response> && response,
        std::unique_ptr<caffeine_failure_response> && failure)
        : response(std::move(response))
        , failure(std::move(failure))
    {}
};

std::string caff_generate_unique_id()
{
    const int id_length = 12;
    const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::string id;
    id.reserve(id_length + 1);

    for (int i = 0; i < id_length; ++i) {
        int random_index = rand() % (sizeof(charset) - 1);
        char character = charset[random_index];
        id += character;
    }

    return id;
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

static struct caff_credentials * make_credentials(
    std::string && access_token,
    std::string && caid,
    std::string && refresh_token,
    std::string && credential)
{
    return new caff_credentials{
        std::move(access_token),
        std::move(caid),
        std::move(refresh_token),
        std::move(credential)
    };
}

// RAII helper to get rid of gotos
class ScopedCURL {
public:
    ScopedCURL(char const * contentType) : curl(curl_easy_init()), headers(basicHeaders(contentType)) {
        applyHeaders();
    }

    ScopedCURL(char const * contentType, caff_credentials * creds) : curl(curl_easy_init()) {
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

    static struct curl_slist * basicHeaders(char const * content_type) {
        struct curl_slist * headers = nullptr;
        headers = curl_slist_append(headers, content_type);
        headers = curl_slist_append(headers, "X-Client-Type: obs"); // TODO client type parameter
        headers = curl_slist_append(headers, "X-Client-Version: " API_VERSION);
        return headers;
    }

    static struct curl_slist * authenticatedHeaders(
        char const * content_type,
        struct caff_credentials * creds)
    {
        std::string authorization("Authorization: Bearer ");
        std::string credential("X-Credential: ");

        {
            std::lock_guard<std::mutex> lock(creds->mutex);
            authorization += creds->access_token;
            credential += creds->credential;
        }

        struct curl_slist * headers = basicHeaders(content_type);
        headers = curl_slist_append(headers, authorization.c_str());
        headers = curl_slist_append(headers, credential.c_str());
        return headers;
    }
};

char const * caff_refresh_token(struct caff_credentials * creds)
{
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

bool do_caffeine_is_supported_version()
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

bool caff_is_supported_version() {
    retry_request(bool, do_caffeine_is_supported_version());
}

/* TODO holdovers from C */
char * cstrdup(char const * str) {
    if (!str)
        return nullptr;

    auto copylen = strlen(str) + 1;
    auto result = new char[copylen];
    strncpy(result, str, copylen);
    return result;
}

char * cstrdup(std::string const & str) {
    if (str.empty())
        return nullptr;

    auto result = new char[str.length() + 1];
    str.copy(result, str.length());
    result[str.length()] = 0;
    return result;
}

/* TODO: refactor this - lots of dupe code between request types
 * TODO: reuse curl handle across requests
 */
static struct caff_auth_response * do_caffeine_signin(
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

        if (otp) {
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
    struct caff_credentials * creds = nullptr;

    auto credsIt = response_json.find("credentials");
    if (credsIt != response_json.end()) {
        creds = make_credentials(
            credsIt->at("access_token").get<std::string>(),
            credsIt->at("caid").get<std::string>(),
            credsIt->at("refresh_token").get<std::string>(),
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
        creds,
        cstrdup(next),
        cstrdup(mfa_otp_method)
    };
}

struct caff_auth_response * caff_signin(
    char const * username,
    char const * password,
    char const * otp)
{
    retry_request(caff_auth_response*, do_caffeine_signin(username, password, otp));
}

static struct caff_credentials * do_caffeine_refresh_auth(
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
        return make_credentials(
            credsIt->at("access_token").get<std::string>(),
            credsIt->at("caid").get<std::string>(),
            credsIt->at("refresh_token").get<std::string>(),
            credsIt->at("credential").get<std::string>());
    }

    log_error("Failed to extract response info");
    return nullptr;
}

struct caff_credentials * caff_refresh_auth(
    char const * refresh_token)
{
    retry_request(caff_credentials*, do_caffeine_refresh_auth(refresh_token));
}

void caff_free_credentials(struct caff_credentials ** credentials)
{
    trace();
    if (!credentials || !*credentials)
        return;

    {
        std::lock_guard<std::mutex> lock((*credentials)->mutex);
        (*credentials)->access_token.erase();
        (*credentials)->caid.erase();
        (*credentials)->refresh_token.erase();
        (*credentials)->credential.erase();
    }
    delete *credentials;
    *credentials = nullptr;
}

void caff_free_auth_response(struct caff_auth_response ** auth_response)
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

static bool do_refresh_credentials(struct caff_credentials * creds)
{
    trace();
    std::string refresh_token;
    {
        std::lock_guard<std::mutex> lock(creds->mutex);
        refresh_token = creds->refresh_token;
    }

    struct caff_credentials * new_creds = caff_refresh_auth(refresh_token.c_str());

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
    caff_free_credentials(&new_creds);
    return true;
}

bool refresh_credentials(struct caff_credentials * creds)
{
    retry_request(bool, do_refresh_credentials(creds));
}

static struct caff_user_info * do_caffeine_getuser(struct caff_credentials * creds)
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

struct caff_user_info * caff_getuser(struct caff_credentials * creds)
{
    retry_request(caff_user_info*, do_caffeine_getuser(creds));
}

void caff_free_user_info(struct caff_user_info ** user_info)
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

static struct caff_games * do_caffeine_get_supported_games()
{
    struct caff_games * response = nullptr;

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

struct caff_games * caff_get_supported_games()
{
    retry_request(struct caff_games *,
        do_caffeine_get_supported_games());
}

void caff_free_game_info(struct caff_game_info ** info)
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

void caff_free_game_list(struct caff_games ** games)
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
    caff_ice_candidates candidates,
    size_t num_candidates,
    char const * stream_url,
    struct caff_credentials * creds)
{
    trace();
    auto ice_candidates = json::array({});
    for (size_t i = 0; i < num_candidates; ++i)
    {
        ice_candidates.push_back({
            {"candidate", candidates[i].sdp},
            {"sdpMid", candidates[i].sdp_mid},
            {"sdpMLineIndex", candidates[i].sdp_mline_index},
            });
    }

    json request_json = {
        {"ice_candidates", ice_candidates}
        // TODO see if this is needed (need a body for post)
    };

    std::string request_body = request_json.dump();

    ScopedCURL curl(CONTENT_TYPE_JSON, creds);

    curl_easy_setopt(curl, CURLOPT_URL, stream_url);
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
            response = do_caffeine_trickle_candidates(candidates, num_candidates, stream_url, creds);
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
    caff_ice_candidates candidates,
    size_t num_candidates,
    char const * stream_url,
    struct caff_credentials * creds)
{
    retry_request(bool,
        do_caffeine_trickle_candidates(candidates, num_candidates,
            stream_url, creds));
}

static struct caff_heartbeat_response * do_caffeine_heartbeat_stream(
    char const * stream_url,
    struct caff_credentials * creds)
{
    trace();

    ScopedCURL curl(CONTENT_TYPE_JSON, creds);

    auto url = STREAM_HEARTBEAT_URL(stream_url);

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
            return do_caffeine_heartbeat_stream(stream_url, creds);
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
    return new caff_heartbeat_response{
        cstrdup(response_json.at("connection_quality").get<std::string>())
    };
}

struct caff_heartbeat_response * caff_heartbeat_stream(
    char const * stream_url,
    struct caff_credentials * creds)
{
    retry_request(
        struct caff_heartbeat_response *,
        do_caffeine_heartbeat_stream(stream_url, creds));
}

void caff_free_heartbeat_response(
    struct caff_heartbeat_response ** response)
{
    if (!response || !*response) {
        return;
    }

    delete[](*response)->connection_quality;
    delete *response;
    *response = nullptr;
}

char * caff_annotate_title(char const * title, enum caff_rating rating)
{
    static const size_t MAX_TITLE_LENGTH = 60; // TODO: policy- should be somewhere else
    static std::string const rating_strings[] = { "", "[17+] " }; // TODO maybe same here

    if (rating < CAFF_RATING_NONE || rating >= CAFF_RATING_MAX)
        rating = CAFF_RATING_NONE;

    auto final_title = rating_strings[rating] + title;
    if (final_title.length() > MAX_TITLE_LENGTH)
        final_title.resize(MAX_TITLE_LENGTH);

    return cstrdup(final_title);
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
    struct caff_credentials * creds)
{
    trace();
    ScopedCURL curl(CONTENT_TYPE_FORM, creds);

    struct curl_httppost * post = nullptr;
    struct curl_httppost * last = nullptr;
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
    struct caff_credentials * creds)
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

static void caffeine_free_feed_contents(struct caff_feed * feed)
{
    if (!feed) {
        return;
    }

    delete[]feed->id;
    delete[]feed->client_id;
    delete[]feed->role;
    delete[]feed->description;
    delete[]feed->source_connection_quality;
    delete[]feed->content.id;
    delete[]feed->content.type;
    delete[]feed->stream.id;
    delete[]feed->stream.source_id;
    delete[]feed->stream.url;
    delete[]feed->stream.sdp_offer;
    delete[]feed->stream.sdp_answer;
}

struct caff_feed * caff_get_stage_feed(struct caff_stage * stage, char const * id)
{
    if (!stage || !id) {
        return nullptr;
    }

    for (size_t i = 0; i < stage->num_feeds; ++i) {
        struct caff_feed * feed = &stage->feeds[i];
        if (strcmp(id, feed->id) == 0) {
            return feed;
        }
    }

    return nullptr;
}

static void caffeine_copy_feed_contents(struct caff_feed const * from, struct caff_feed * to)
{
    to->id = cstrdup(from->id);
    to->client_id = cstrdup(from->client_id);
    to->role = cstrdup(from->role);
    to->description = cstrdup(from->description);
    to->source_connection_quality = cstrdup(from->source_connection_quality);
    to->volume = from->volume;
    to->capabilities = from->capabilities;
    to->content.id = cstrdup(from->content.id);
    to->content.type = cstrdup(from->content.type);
    to->stream.id = cstrdup(from->stream.id);
    to->stream.source_id = cstrdup(from->stream.source_id);
    to->stream.url = cstrdup(from->stream.url);
    to->stream.sdp_offer = cstrdup(from->stream.sdp_offer);
    to->stream.sdp_answer = cstrdup(from->stream.sdp_answer);
}


void caff_set_stage_feed(struct caff_stage * stage, struct caff_feed const * feed)
{
    if (!stage || !feed) {
        return;
    }

    caff_clear_stage_feeds(stage);

    stage->feeds = new caff_feed;
    caffeine_copy_feed_contents(feed, stage->feeds);
    stage->num_feeds = 1;
}

void caff_clear_stage_feeds(struct caff_stage * stage)
{
    if (!stage || !stage->feeds) {
        return;
    }

    for (size_t i = 0; i < stage->num_feeds; ++i) {
        caffeine_free_feed_contents(&stage->feeds[i]);
    }

    delete[]stage->feeds;
    stage->feeds = nullptr;
    stage->num_feeds = 0;
}

void caff_free_stage(struct caff_stage ** stage)
{
    if (!stage || !*stage) {
        return;
    }

    delete[](*stage)->id;
    delete[](*stage)->username;
    delete[](*stage)->title;
    delete[](*stage)->broadcast_id;
    caff_clear_stage_feeds(*stage);
    delete *stage;
    *stage = nullptr;
}

struct caff_stage_request * caff_copy_stage_request(
    struct caff_stage_request const * request)
{
    if (!request) {
        return nullptr;
    }

    struct caff_stage * stage = request->stage;
    struct caff_stage * stage_copy = nullptr;
    if (stage) {
        caff_feed * feeds_copy = nullptr;
        if (stage->num_feeds > 0) {
            feeds_copy = new caff_feed[stage->num_feeds];
            for (size_t i = 0; i < stage_copy->num_feeds; ++i) {
                caffeine_copy_feed_contents(&stage->feeds[i], &feeds_copy[i]);
            }
        }
        stage_copy = new caff_stage{
            cstrdup(stage->id),
            cstrdup(stage->username),
            cstrdup(stage->title),
            cstrdup(stage->broadcast_id),
            stage->upsert_broadcast,
            stage->live,
            feeds_copy,
            stage->num_feeds,
        };
    }

    return new caff_stage_request{
        cstrdup(request->username),
        cstrdup(request->client_id),
        cstrdup(request->cursor),
        stage_copy
    };
}

void caff_free_stage_request(struct caff_stage_request ** request)
{
    if (!request || !*request) {
        return;
    }

    delete[](*request)->username;
    delete[](*request)->client_id;
    delete[](*request)->cursor;
    caff_free_stage(&(*request)->stage);
    delete *request;
    *request = nullptr;
}

void caffeine_free_stage_response(struct caffeine_stage_response ** response)
{
    if (!response || !*response) {
        return;
    }

    delete *response;
    *response = nullptr;
}

void caffeine_free_failure(struct caffeine_failure_response ** failure)
{
    if (!failure || !*failure) {
        return;
    }

    delete *failure;
    *failure = nullptr;
}

static json caffeine_serialize_stage_request(struct caff_stage_request request)
{
    json request_json = {
        {"client", {
            {"id", request.client_id},
            {"headless", true}
        }}
    };

    if (request.cursor) {
        request_json.push_back({ "cursor", request.cursor });
    }

    if (request.stage) {
        json stage = {
            {"id", request.stage->id},
            {"username", request.stage->username},
            {"title", request.stage->title},
            {"broadcast_id", request.stage->broadcast_id},
            {"upsert_broadcast", request.stage->upsert_broadcast},
            {"live", request.stage->live},
        };

        json feeds = json::object();

        for (size_t i = 0; i < request.stage->num_feeds; ++i) {
            struct caff_feed * feed = &request.stage->feeds[i];
            json json_feed = {
                {"id", feed->id},
                {"client_id", feed->client_id},
                {"role", feed->role},
                {"description", feed->description},
                {"source_connection_quality", feed->source_connection_quality},
                {"volume", feed->volume},
                {"capabilities", {
                    {"video", feed->capabilities.video},
                    {"audio", feed->capabilities.audio},
                }},
            };

            if (feed->content.id && feed->content.type) {
                json_feed.push_back({ "content", {
                    {"id", feed->content.id},
                    {"type", feed->content.type},
                } });
            }

            if (feed->stream.sdp_offer || feed->stream.id) {
                json_feed.push_back({ "stream", {
                    {"id", feed->stream.id},
                    {"source_id", feed->stream.source_id},
                    {"url", feed->stream.url},
                    {"sdp_offer", feed->stream.sdp_offer},
                    {"sdp_answer", feed->stream.sdp_answer},
                } });
            }

            feeds.push_back({ feed->id, json_feed });
        }

        stage.push_back({ "feeds", feeds });
        request_json.push_back({ "payload", stage });
    }

    return request_json;
}

std::unique_ptr<caffeine_stage_response> caffeine_deserialize_stage_response(json response_json)
{
    std::string cursor;
    double retry_in = 0.0;
    try {
        response_json.at("cursor").get_to(cursor);
        response_json.at("retry_in").get_to(retry_in);
    }
    catch (...) {
        log_error("Failed to parse stage response cursor and retry_in");
        return nullptr;
    }

    auto payloadIt = response_json.find("payload");
    if (payloadIt == response_json.end()) {
        log_error("Response did not contain a payload");
        return nullptr;
    }

    std::string id;
    std::string username;
    std::string title;
    std::string broadcast_id;
    bool upsert_broadcast = 0;
    bool live = 0;

    try {
        payloadIt->at("id").get_to(id);
        payloadIt->at("username").get_to(username);
        payloadIt->at("title").get_to(title);
        auto broadcastIdIt = payloadIt->find("broadcast_id");
        if (broadcastIdIt != payloadIt->end())
            broadcastIdIt->get_to(broadcast_id);
        auto upsertIt = payloadIt->find("upsert_broadcast");
        if (upsertIt != payloadIt->end())
            upsertIt->get_to(upsert_broadcast);
    }
    catch (...) {
        log_error("Failed to parse stage");
        return nullptr;
    }

    std::unique_ptr<caff_stage> stage{
        new caff_stage{
            cstrdup(id),
            cstrdup(username),
            cstrdup(title),
            cstrdup(broadcast_id),
            upsert_broadcast,
            live,
            nullptr,
            0
        },
    };

    auto feedsIt = payloadIt->find("feeds");
    if (feedsIt != payloadIt->end() && feedsIt->size() > 0) {
        stage->num_feeds = feedsIt->size();
        stage->feeds = new caff_feed[stage->num_feeds]{};

        struct caff_feed * feed_pointer = stage->feeds;
        for (auto & feedJson : *feedsIt) {

            auto capabilities = feedJson.value("capabilities", json::object());
            auto content = feedJson.value("content", json::object());
            auto stream = feedJson.value("stream", json::object());

            try {
                *feed_pointer = {
                    cstrdup(feedJson.at("id").get<std::string>()),
                    cstrdup(feedJson.value("client_id", "")),
                    cstrdup(feedJson.value("role", "")),
                    cstrdup(feedJson.value("description", "")),
                    cstrdup(feedJson.value("source_connection_quality", "")),
                    feedJson.value("volume", 0.0),
                    {
                        capabilities.value("audio", true),
                        capabilities.value("video", true),
                    },
                    {
                        cstrdup(content.value("id", "")),
                        cstrdup(content.value("type", "")),
                    },
                    {
                        cstrdup(stream.value("id", "")),
                        cstrdup(stream.value("source_id", "")),
                        cstrdup(stream.value("url", "")),
                        cstrdup(stream.value("sdp_offer", "")),
                        cstrdup(stream.value("sdp_answer", "")),
                    },
                };
            }
            catch (...) {
                log_error("Failed to parse feed");
                return nullptr;
            }

            ++feed_pointer;
        }
    }

    return std::make_unique<caffeine_stage_response>(
        cstrdup(cursor),
        retry_in,
        std::move(stage)
        );
}

static bool is_out_of_capacity_failure_type(std::string const & type)
{
    return type == "OutOfCapacity";
}

static std::unique_ptr<caffeine_stage_response_result> do_caffeine_stage_update(
    struct caff_stage_request const & request,
    struct caff_credentials * creds)
{
    trace();

    if (!request.username) {
        log_error("Did not set request username");
        return nullptr;
    }

    auto request_json = caffeine_serialize_stage_request(request);
    auto request_body = request_json.dump();

    ScopedCURL curl(CONTENT_TYPE_JSON, creds);

    auto url_str = STAGE_UPDATE_URL(request.username);

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
        return nullptr;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    log_debug("Http response [%ld]", response_code);

    if (response_code == 401) {
        log_info("Unauthorized - refreshing credentials");
        if (refresh_credentials(creds)) {
            return do_caffeine_stage_update(request, creds);
        }
        return nullptr;
    }

    json response_json;
    try {
        response_json = json::parse(response_str);
    }
    catch (...) {
        log_error("Failed to deserialized stage update response to JSON: %s",
            json_error.text);
        return nullptr;
    }

    if (response_code == 200) {
        return std::make_unique<caffeine_stage_response_result>(
            caffeine_deserialize_stage_response(response_json),
            nullptr);
    }
    else {
        try {
            auto type = response_json.at("type").get<std::string>();

            // As of now, the only failure response we want to return and not retry is `OutOfCapacity`
            if (!is_out_of_capacity_failure_type(type)) {
                return nullptr;
            }

            auto message_json = response_json.value("display_message", json::object());
            return std::make_unique<caffeine_stage_response_result>(
                nullptr,
                std::make_unique<caffeine_failure_response>(
                    type,
                    response_json.value("reason", ""),
                    caffeine_display_message{
                        message_json.value("title", ""),
                        message_json.at("body").get<std::string>(),
                    }));
        }
        catch (...) {
            log_error("Failed to unpack failure response");
            return nullptr;
        }
    }

    return nullptr;
}

std::unique_ptr<caffeine_stage_response_result> caffeine_stage_update(
    struct caff_stage_request const & request,
    struct caff_credentials * creds)
{
    retry_request(std::unique_ptr<caffeine_stage_response_result>,
        do_caffeine_stage_update(request, creds));
}

static void transfer_stage_data(
    std::unique_ptr<caffeine_stage_response> && from_response,
    struct caff_stage_request * to_request)
{
    if (to_request->cursor) {
        delete[] to_request->cursor;
    }
    caff_free_stage(&to_request->stage);
    to_request->cursor = cstrdup(from_response->cursor);
    to_request->stage = from_response->stage.release();
}

bool caff_request_stage_update(
    struct caff_stage_request * request,
    struct caff_credentials * creds,
    double * retry_in,
    bool * is_out_of_capacity)
{
    auto result = caffeine_stage_update(*request, creds);

    bool success = result && result->response;
    if (success) {
        if (retry_in) {
            *retry_in = result->response->retry_in;
        }
        transfer_stage_data(std::move(result->response), request);
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
