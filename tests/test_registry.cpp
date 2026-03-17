#include <catch2/catch_test_macros.hpp>
#include <splice/splice.hpp>

struct DummyPlayer
{
  std::string name;
  int health = 100;
};

class DummyWorld
{
public:
  [[= splice::hook::hookable { }]] void onStep(DummyPlayer *player, int x, int z) { }
  [[= splice::hook::hookable { }]] float calcDamage(DummyPlayer *player, float amount) { return amount; }
  [[= splice::hook::hookable { }]] bool tryAction(DummyPlayer *player) { return true; }

  void internalHelper() { }
};

SPLICE_HOOK_REGISTRY(DummyWorld, g_dummy);

/// Returns a fresh isolated registry for each test, avoiding cross-test hook accumulation.
static auto make_registry() { return splice::hook::ClassRegistry<DummyWorld>::make_isolated(); }

TEST_CASE("Head hook runs before original", "[registry][inject][head]")
{
  auto reg = make_registry();
  std::vector<std::string> order;

  reg->chain<^^DummyWorld::onStep>().original
      = [&](DummyWorld *, DummyPlayer *, int, int) { order.push_back("original"); };

  auto result = reg->inject<^^DummyWorld::onStep, splice::hook::InjectPoint::Head>(
      [&](splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int) { order.push_back("head"); });

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE(order == std::vector<std::string> { "head", "original" });
}

TEST_CASE("Head hook can cancel original", "[registry][inject][head]")
{
  auto reg = make_registry();
  bool original_ran = false;

  reg->chain<^^DummyWorld::onStep>().original = [&](DummyWorld *, DummyPlayer *, int, int) { original_ran = true; };

  auto result = reg->inject<^^DummyWorld::onStep, splice::hook::InjectPoint::Head>(
      [](splice::detail::CallbackInfo &ci, DummyWorld *, DummyPlayer *, int, int) { ci.cancelled = true; });

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE_FALSE(original_ran);
}

TEST_CASE("Tail hook runs after original", "[registry][inject][tail]")
{
  auto reg = make_registry();
  std::vector<std::string> order;

  reg->chain<^^DummyWorld::onStep>().original
      = [&](DummyWorld *, DummyPlayer *, int, int) { order.push_back("original"); };

  auto result = reg->inject<^^DummyWorld::onStep, splice::hook::InjectPoint::Tail>(
      [&](splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int) { order.push_back("tail"); });

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE(order == std::vector<std::string> { "original", "tail" });
}

TEST_CASE("Tail hook does not run when cancelled", "[registry][inject][tail]")
{
  auto reg = make_registry();
  bool tail_ran = false;

  auto head_result = reg->inject<^^DummyWorld::onStep, splice::hook::InjectPoint::Head>(
      [](splice::detail::CallbackInfo &ci, DummyWorld *, DummyPlayer *, int, int) { ci.cancelled = true; });

  REQUIRE(head_result.has_value());

  auto tail_result = reg->inject<^^DummyWorld::onStep, splice::hook::InjectPoint::Tail>(
      [&](splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int) { tail_ran = true; });

  REQUIRE(tail_result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE_FALSE(tail_ran);
}

TEST_CASE("Return hook can observe return value", "[registry][inject][return]")
{
  auto reg = make_registry();
  float observed = 0.0f;

  auto result = reg->inject<^^DummyWorld::calcDamage, splice::hook::InjectPoint::Return>(
      [&](splice::detail::CallbackInfoReturnable<float> &ci, DummyWorld *, DummyPlayer *, float)
      {
        if (ci.return_value)
          observed = *ci.return_value;
      });

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::calcDamage>(&world, &player, 10.0f);

  REQUIRE(observed == 10.0f);
}

TEST_CASE("Return hook can override return value", "[registry][inject][return]")
{
  auto reg = make_registry();

  auto result = reg->inject<^^DummyWorld::calcDamage, splice::hook::InjectPoint::Return>(
      [](splice::detail::CallbackInfoReturnable<float> &ci, DummyWorld *, DummyPlayer *, float)
      { ci.return_value = 99.0f; });

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  float ret = reg->dispatch<^^DummyWorld::calcDamage>(&world, &player, 10.0f);

  REQUIRE(ret == 99.0f);
}

TEST_CASE("Head hooks run in priority order", "[registry][priority]")
{
  auto reg = make_registry();
  std::vector<int> order;

  auto r1 = reg->inject<^^DummyWorld::onStep, splice::hook::InjectPoint::Head>(
      [&](splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int) { order.push_back(2); },
      splice::hook::Priority::Normal);

  auto r2 = reg->inject<^^DummyWorld::onStep, splice::hook::InjectPoint::Head>(
      [&](splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int) { order.push_back(1); },
      splice::hook::Priority::High);

  auto r3 = reg->inject<^^DummyWorld::onStep, splice::hook::InjectPoint::Head>(
      [&](splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int) { order.push_back(3); },
      splice::hook::Priority::Low);

  REQUIRE(r1.has_value());
  REQUIRE(r2.has_value());
  REQUIRE(r3.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE(order == std::vector<int> { 1, 2, 3 });
}

TEST_CASE("modify_arg rewrites the target argument", "[registry][modify_arg]")
{
  auto reg = make_registry();

  auto result = reg->modify_arg<^^DummyWorld::calcDamage, 1>([](float amount) -> float { return amount * 2.0f; });

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  float ret = reg->dispatch<^^DummyWorld::calcDamage>(&world, &player, 5.0f);

  REQUIRE(ret == 10.0f);
}

TEST_CASE("modify_return rewrites the return value", "[registry][modify_return]")
{
  auto reg = make_registry();

  auto result
      = reg->modify_return<^^DummyWorld::calcDamage>([](float ret) -> float { return std::clamp(ret, 0.0f, 20.0f); });

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  float ret = reg->dispatch<^^DummyWorld::calcDamage>(&world, &player, 50.0f);

  REQUIRE(ret == 20.0f);
}

TEST_CASE("SPLICE_HOOK_REGISTRY shared instance is stable", "[registry][macro]")
{
  auto a = splice::hook::ClassRegistry<DummyWorld>::shared();
  auto b = splice::hook::ClassRegistry<DummyWorld>::shared();

  REQUIRE(a.get() == b.get());
}

TEST_CASE("Isolated registry is independent of shared instance", "[registry][macro]")
{
  auto isolated = make_registry();
  auto shared = splice::hook::ClassRegistry<DummyWorld>::shared();

  REQUIRE(isolated.get() != shared.get());
}

class Test {
    public:
    [[= splice::hook::injection{ .what = ^^DummyWorld::calcDamage, .where = splice::hook::InjectPoint::Return }]]
    void inject(splice::detail::CallbackInfoReturnable<float> &ci, DummyWorld *, DummyPlayer *, float) {
        ci.return_value = 99.0f;
    }
};

TEST_CASE("inject_all")
{
  auto reg = make_registry();

  auto result = reg->inject_all<Test>();

  REQUIRE(result.has_value());

  reg->print_registry();

  DummyWorld world;
  DummyPlayer player;
  float ret = reg->dispatch<^^DummyWorld::calcDamage>(&world, &player, 20.0f);

  REQUIRE(ret == 99.0f);
}
