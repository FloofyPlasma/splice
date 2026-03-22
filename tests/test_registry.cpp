#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <splice/splice.hpp>
#include <vector>

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

/// Returns a fresh isolated registry for each test, avoiding cross-test hook
/// accumulation.
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

class Test1
{
public:
  static int val;

  [[= splice::hook::injection {
      .what = ^^DummyWorld::calcDamage, .where = splice::hook::InjectPoint::Return }]] static void
      inject1(splice::detail::CallbackInfoReturnable<float> &ci, DummyWorld *, DummyPlayer *, float)
  {
    ci.return_value = 99.0f;
  }

  [[= splice::hook::injection { .what = ^^DummyWorld::tryAction,
      .where = splice::hook::InjectPoint::Head }]] static void inject2(splice::detail::CallbackInfoReturnable<bool> &,
      DummyWorld *, DummyPlayer *)
  {
    val = 2;
  }

  [[= splice::hook::injection { .what = ^^DummyWorld::onStep, .where = splice::hook::InjectPoint::Tail }]] static void
      inject3(splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int)
  {
    val = 4;
  }
};

int Test1::val = 0;

TEST_CASE("Ensure functions actually get injected", "[registry][class_inject][static]")
{
  auto reg = make_registry();

  auto result = reg->inject_all_static<Test1>();

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  float ret = reg->dispatch<^^DummyWorld::calcDamage>(&world, &player, 20.0f);
  REQUIRE(ret == 99.0f);

  reg->dispatch<^^DummyWorld::tryAction>(&world, &player);
  REQUIRE(Test1::val == 2);

  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);
  REQUIRE(Test1::val == 4);
}

class Test2
{
public:
  static std::vector<int> v;

  [[= splice::hook::injection { .what = ^^DummyWorld::onStep,
      .where = splice::hook::InjectPoint::Tail,
      .priority = 0 }]] static void early(splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int)
  {
    v.push_back(1);
  }

  [[= splice::hook::injection { .what = ^^DummyWorld::onStep,
      .where = splice::hook::InjectPoint::Tail,
      .priority = 1000 }]] static void late(splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int)
  {
    v.push_back(2);
  }
};

std::vector<int> Test2::v = std::vector<int> { };

TEST_CASE("Ensure hooks respect priority", "[registry][class_inject][static]")
{
  auto reg = make_registry();
  auto result = reg->inject_all_static<Test2>();

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE(Test2::v == std::vector<int> { 1, 2 });
}

class Test3
{
public:
  static std::vector<int> v;

  [[= splice::hook::injection { .what = ^^DummyWorld::onStep, .where = splice::hook::InjectPoint::Tail }]] static void
      early(splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int)
  {
    v.push_back(1);
  }

  [[= splice::hook::injection { .what = ^^DummyWorld::onStep, .where = splice::hook::InjectPoint::Tail }]] static void
      late(splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int)
  {
    v.push_back(2);
  }
};

std::vector<int> Test3::v = std::vector<int> { };

TEST_CASE("Hooks without priority are registered in reverse declaration order", "[registry][class_inject][static]")
{
  auto reg = make_registry();
  auto result = reg->inject_all_static<Test3>();

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE(Test3::v == std::vector<int> { 2, 1 });
}

class DummyObject
{
public:
  [[= splice::hook::hookable { }]] void f() { }
};

SPLICE_HOOK_REGISTRY(DummyObject, g_obj);

class Test4
{
  [[= splice::hook::injection { .what = ^^DummyObject::f, .where = splice::hook::InjectPoint::Head }]] static void
      inject(splice::detail::CallbackInfo &, DummyObject *)
  {
  }
};

TEST_CASE("Only try to register hooks for the specified class", "[registry][class_inject][static]")
{
  auto reg = make_registry();
  auto result = reg->inject_all_static<Test4>();

  REQUIRE(result.has_value());

  result = g_obj->inject_all_static<Test4>();

  REQUIRE(result.has_value());
}

class Test5
{
public:
  int val = 0;
  [[= splice::hook::injection { .what = ^^DummyWorld::onStep, .where = splice::hook::InjectPoint::Head }]] void inject(
      splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int)
  {
    val = 1;
  }
};

TEST_CASE("Instanced injections function", "[registry][class_inject][instanced]")
{
  auto reg = make_registry();
  auto i = std::make_shared<Test5>();
  auto result = reg->inject_all_instanced(i);

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE(i->val == 1);
}

class Test6
{
public:
  static int v;
  int val = 0;
  [[= splice::hook::injection { .what = ^^DummyWorld::onStep, .where = splice::hook::InjectPoint::Head }]] void inject(
      splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int)
  {
    val++;
    v++;
  }
};

int Test6::v = 0;

TEST_CASE("Instanced injection only affects its instance", "[registry][class_inject][instanced]")
{
  auto reg = make_registry();
  auto i1 = std::make_shared<Test6>();
  auto i2 = std::make_shared<Test6>();

  auto result = reg->inject_all_instanced(i1);
  REQUIRE(result.has_value());
  result = reg->inject_all_instanced(i2);
  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE(i1->val == 1);
  REQUIRE(i2->val == 1);
  REQUIRE(Test6::v == 2);
}

class Test7
{
public:
  static int v;
  [[= splice::hook::injection { .what = ^^DummyWorld::onStep, .where = splice::hook::InjectPoint::Head }]] void inject(
      splice::detail::CallbackInfo &, DummyWorld *, DummyPlayer *, int, int)
  {
    v++;
  }
};

int Test7::v = 0;

TEST_CASE("Instanced injections don't run after pointer is discarded", "[registry][class_inject][instanced]")
{
  auto reg = make_registry();
  auto i = std::make_shared<Test7>();
  auto result = reg->inject_all_instanced(i);

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE(Test7::v == 1);

  i.reset();

  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE(Test7::v == 1);
}

class Test8
{
public:
  static int v;
  [[= splice::hook::injection { .what = ^^DummyWorld::onStep,
      .where = splice::hook::InjectPoint::Head }]][[= splice::hook::injection { .what = ^^DummyWorld::onStep,
      .where = splice::hook::InjectPoint::Head }]] static void inject(splice::detail::CallbackInfo &, DummyWorld *,
      DummyPlayer *, int, int)
  {
    v++;
  }
};

int Test8::v = 0;

TEST_CASE("Repeat annotations function", "[registry][class_inject]")
{
  auto reg = make_registry();
  auto result = reg->inject_all_static<Test8>();

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  reg->dispatch<^^DummyWorld::onStep>(&world, &player, 0, 0);

  REQUIRE(Test8::v == 2);
}

class Test9
{
public:
  [[= splice::hook::modify_arg { .what = ^^DummyWorld::calcDamage, .arg = 1 }]] static float modify(float f)
  {
    return f * 2.0f;
  }
};

TEST_CASE("static modify_arg class injections function", "[registry][class_inject][static]")
{
  auto reg = make_registry();
  auto result = reg->inject_all_static<Test9>();

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  float res = reg->dispatch<^^DummyWorld::calcDamage>(&world, &player, 2.0);

  REQUIRE(res == 4.0f);
}

class Test10
{
public:
  float v = 0;
  [[= splice::hook::modify_arg { .what = ^^DummyWorld::calcDamage, .arg = 1 }]] float modify(float f)
  {
    v = f;
    return f * 3.0f;
  }
};

TEST_CASE("instanced modify_arg class injections function", "[registry][class_inject][instanced]")
{
  auto reg = make_registry();
  std::shared_ptr<Test10> ptr = std::make_shared<Test10>(0);
  auto result = reg->inject_all_instanced(ptr);

  REQUIRE(result.has_value());

  DummyWorld world;
  DummyPlayer player;
  float res = reg->dispatch<^^DummyWorld::calcDamage>(&world, &player, 2.0);

  REQUIRE(res == 6.0f);
  REQUIRE(ptr->v == 2.0f);
}
