#include "doctest.h"

TEST_CASE("doctest CHECK works") {
    CHECK(1 == 1);
}

TEST_CASE("doctest CHECK_FALSE works") {
    CHECK_FALSE(1 == 0);
}
