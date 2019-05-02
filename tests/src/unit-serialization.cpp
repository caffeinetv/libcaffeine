#include "doctest.h"

#include "Serialization.hpp"

using Json = nlohmann::json;
using namespace caff;

TEST_CASE("Complete credentials successfully deserializes") {
    Json credsJson = { { "access_token", "fakeaccesstoken" },
                       { "refresh_token", "fakerefreshtoken" },
                       { "caid", "CAIDTHISISFAKE" },
                       { "credential", "fakecredential" }
    };

    Credentials creds = credsJson;
    CHECK(creds.accessToken == "fakeaccesstoken");
    CHECK(creds.refreshToken == "fakerefreshtoken");
    CHECK(creds.caid == "CAIDTHISISFAKE");
    CHECK(creds.credential == "fakecredential");
}

TEST_CASE("Empty credentials fails to deserialize") {
    Json credsJson = {};

    Credentials creds;
    CHECK_THROWS(creds = credsJson);
}
