#include <catch2/catch_test_macros.hpp>

unsigned int Square(unsigned int number) {
    return number * number;
}

TEST_CASE("Squares are computed", "[square]") {
    REQUIRE(Square(0) == 0);
    REQUIRE(Square(1) == 1);
    REQUIRE(Square(2) == 4);
    REQUIRE(Square(3) == 9);
    REQUIRE(Square(10) == 100);
}
