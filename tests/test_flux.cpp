#include <catch2/catch_test_macros.hpp>
#include <splice/splice.hpp>

// Explicit opt-in only -- only annotated fields
struct ExplicitOnly
{
  [[= splice::flux::field { .name = "hp" }]]
  float health = 100.f;

  [[= splice::flux::field{}]]
  float armor = 50.f;

  float ignored = 0.f; // should not appear in descriptor
};

// Full auto -- all public fields, private excluded
struct [[= splice::flux::serializable{}]] FullAuto
{
  float x {};
  float y {};
  float z {};
private:
  float secret {}; // must not appear
};

// Mixed -- public fields by default, overrides on specific ones
struct [[= splice::flux::serializable{}]] Mixed
{
  float position {};

  [[= splice::flux::field { .name = "hp" }]]
  float health {};

  [[= splice::flux::field { .replicate = true }]]
  float ammo {};
};

// ---- tests -----------------------------------------------------------------

TEST_CASE("explicit serialized fields", "[flux][explicit]")
{
  ExplicitOnly object;
  std::string result = splice::flux::encode<splice::flux::JSON>(object);

  // TODO: Add rename support
  REQUIRE(result == R"({"armor":50.0,"health":100.0})");
}

TEST_CASE("full automatic field serialization", "[flux][fullauto]")
{
  FullAuto object;
  std::string result = splice::flux::encode<splice::flux::JSON>(object);

  REQUIRE(result == R"({"x":0.0,"y":0.0,"z":0.0})");
}

TEST_CASE("mixed field serialization", "[flux][mixed]")
{
  Mixed object;
  std::string result = splice::flux::encode<splice::flux::JSON>(object);

  REQUIRE(result == R"({"ammo":0.0,"health":0.0,"position":0.0})");
}