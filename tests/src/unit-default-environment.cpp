#include "doctest.h"

#include "Urls.hpp"

using namespace caff;

TEST_CASE("Default domain is caffeine.tv") {
    CHECK(caffeineDomain == "caffeine.tv");
}
