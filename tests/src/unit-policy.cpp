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

TEST_CASE("Annotate title") {
    SUBCASE("uses the supplied title when it is not empty") {
        CHECK(annotateTitle("Title") == "Title");
    }

    SUBCASE("uses the default title when an empty title is supplied") {
        CHECK(annotateTitle("") == defaultTitle);
    }

    SUBCASE("trims leading and trailing whitespace") {
        CHECK(annotateTitle("  Title  ") == "Title");
        CHECK(annotateTitle("     ") == defaultTitle);
    }

    SUBCASE("adds the 17+ tag when rated 17+") {
        std::string ratingTag = seventeenPlusTag;
        CHECK(annotateTitle("Title") == "Title");
        CHECK(annotateTitle("") == defaultTitle);
    }

    SUBCASE("truncates titles longer than the max character limit") {
        CHECK(annotateTitle(std::string(500, 'a')) == std::string(maxTitleLength, 'a'));
    }
}
