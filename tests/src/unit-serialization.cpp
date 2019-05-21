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

TEST_CASE("webrtc stats serialize") {
    using namespace webrtc;

    StatsReport report1{
        StatsReport::NewTypedId(StatsReport::kStatsReportTypeBwe, "bwe") };
    report1.set_timestamp(4.2);
    report1.AddInt64(StatsReport::kStatsValueNameBytesSent, 42ll);
    report1.AddFloat(StatsReport::kStatsValueNameAudioInputLevel, 0.125);

    StatsReport report2{
        StatsReport::NewTypedId(StatsReport::kStatsReportTypeSsrc, "ssrc") };
    report2.set_timestamp(8.4);
    report2.AddInt(StatsReport::kStatsValueNameFrameRateInput, 42);
    report2.AddInt(StatsReport::kStatsValueNameCpuLimitedResolution, 480);

    StatsReports reports{ &report1, &report2 };

    Json expected = {
        { { "caffeineReportType", "VideoBwe" },
          { "caffeineUnixTimestamp", 4.2 },
          { "bytesSent", 42ll },
          { "audioInputLevel", 0.125 } },
        { { "caffeineReportType", "ssrc" },
          { "caffeineUnixTimestamp", 8.4 },
          { "googFrameRateInput", 42 },
          { "googCpuLimitedResolution", 480 } },
    };

    CHECK(serializeWebrtcStats(reports) == expected);
}
