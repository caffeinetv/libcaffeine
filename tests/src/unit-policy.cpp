#include "doctest.h"

#include "Policy.hpp"

using namespace caff;

TEST_CASE("Aspect ratio check fails below 1:3") {
    CHECK(checkAspectRatio(1000, 3001) == caff_ResultAspectTooNarrow);
    CHECK(checkAspectRatio(999, 3000) == caff_ResultAspectTooNarrow);
}

TEST_CASE("Aspect ratio check fails above 3:1") {
    CHECK(checkAspectRatio(3001, 1000) == caff_ResultAspectTooWide);
    CHECK(checkAspectRatio(3000, 999) == caff_ResultAspectTooWide);
}

TEST_CASE("Aspect ratio check succeeds at exactly 1:3") {
    CHECK(checkAspectRatio(1000, 3000) == caff_ResultSuccess);
}

TEST_CASE("Aspect ratio check succeeds at exactly 3:1") {
    CHECK(checkAspectRatio(3000, 1000) == caff_ResultSuccess);
}

TEST_CASE("Aspect ratio check succeeds between 1:3 and 3:1") {
    CHECK(checkAspectRatio(1000, 1000) == caff_ResultSuccess);
    CHECK(checkAspectRatio(2000, 1000) == caff_ResultSuccess);
    CHECK(checkAspectRatio(1000, 2000) == caff_ResultSuccess);
    CHECK(checkAspectRatio(2999, 1000) == caff_ResultSuccess);
    CHECK(checkAspectRatio(1000, 2999) == caff_ResultSuccess);
}
