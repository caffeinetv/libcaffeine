#include "doctest.h"

#include "Urls.hpp"

using namespace caff;

TEST_CASE("Custom domain is used when environment variable is set") {
    CHECK(caffeineDomain == "custom.environment.net");
}
