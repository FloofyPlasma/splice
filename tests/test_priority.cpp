#include <catch2/catch_test_macros.hpp>
#include <splice/splice.hpp>

TEST_CASE("Priority constants are strictly ordered", "[priority]")
{
  REQUIRE(splice::hook::Priority::Highest < splice::hook::Priority::High);
  REQUIRE(splice::hook::Priority::High < splice::hook::Priority::Normal);
  REQUIRE(splice::hook::Priority::Normal < splice::hook::Priority::Low);
  REQUIRE(splice::hook::Priority::Low < splice::hook::Priority::Lowest);
}

TEST_CASE("Priority arithmetic works as expected", "[priority]")
{
  REQUIRE(splice::hook::Priority::Normal + 1 > splice::hook::Priority::Normal);
  REQUIRE(splice::hook::Priority::Normal - 1 < splice::hook::Priority::Normal);
}

TEST_CASE("Priority::Normal is the midpoint of Highest and Lowest", "[priority]")
{
  REQUIRE(splice::hook::Priority::Normal == (splice::hook::Priority::Highest + splice::hook::Priority::Lowest) / 2);
}
