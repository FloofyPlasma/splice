#include <catch2/catch_test_macros.hpp>
#include <splice/splice.hpp>
#include <type_traits>

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
  std::string result = splice::flux::encode<splice::flux::JSON>(123.0f);
  std::string result2 = splice::flux::encode<splice::flux::JSON>(object);

  REQUIRE(result == "123.0");
  REQUIRE(result2 == R"({"armor":50.0,"health":100.0})");
}