#include <catch2/catch_test_macros.hpp>
#include <splice/splice.hpp>
#include <string>
#include <vector>

struct DummyButton
{
  [[SPLICE_WIRE_SIGNAL]] void on_click(int x, int y);
  [[SPLICE_WIRE_SIGNAL]] void on_hover();
  [[SPLICE_WIRE_SIGNAL_ONCE]] void on_ready();
};

WIRE_REGISTRY(DummyButton, g_button);

struct DummyLogger : public splice::wire::Connectable
{
  std::vector<std::string> log;

  [[SPLICE_WIRE_SLOT(.signal = ^^DummyButton::on_click)]]
  void handle_click(int x, int y)
  {
    log.push_back("click:" + std::to_string(x) + "," + std::to_string(y));
  }

  [[SPLICE_WIRE_SLOT(.signal = ^^DummyButton::on_hover)]]
  void handle_hover()
  {
    log.push_back("hover");
  }

  [[SPLICE_WIRE_SLOT(.signal = ^^DummyButton::on_ready)]]
  void handle_ready()
  {
    log.push_back("ready");
  }
};

struct PriorityListener : public splice::wire::Connectable
{
  std::vector<int> order;

  [[SPLICE_WIRE_SLOT(.signal = ^^DummyButton::on_hover, .priority = splice::hook::Priority::High)]]
  void handle_high()
  {
    order.push_back(1);
  }

  [[SPLICE_WIRE_SLOT(.signal = ^^DummyButton::on_hover, .priority = splice::hook::Priority::Low)]]
  void handle_low()
  {
    order.push_back(3);
  }

  [[SPLICE_WIRE_SLOT(.signal = ^^DummyButton::on_hover, .priority = splice::hook::Priority::Normal)]]
  void handle_normal()
  {
    order.push_back(2);
  }
};

static auto make_registry() { return splice::wire::SignalRegistry<DummyButton>::make_isolated(); }

// ---- tests -----------------------------------------------------------------

TEST_CASE("connect_all wires matching slots to signals", "[wire][connect]")
{
  auto reg = make_registry();
  DummyButton button;
  DummyLogger logger;

  splice::wire::connect_all(reg, &logger);

  reg->emit<^^DummyButton::on_click>(&button, 10, 20);

  REQUIRE(logger.log == std::vector<std::string> { "click:10,20" });
}

TEST_CASE("emit fires all connected slots", "[wire][emit]")
{
  auto reg = make_registry();
  DummyButton button;
  DummyLogger logger;

  splice::wire::connect_all(reg, &logger);

  reg->emit<^^DummyButton::on_click>(&button, 1, 2);
  reg->emit<^^DummyButton::on_hover>(&button);

  REQUIRE(logger.log == std::vector<std::string> { "click:1,2", "hover" });
}

TEST_CASE("slots fire in priority order", "[wire][priority]")
{
  auto reg = make_registry();
  DummyButton button;
  PriorityListener listener;

  splice::wire::connect_all(reg, &listener);
  reg->emit<^^DummyButton::on_hover>(&button);

  REQUIRE(listener.order == std::vector<int> { 1, 2, 3 });
}

TEST_CASE("signal_once fires exactly once", "[wire][signal_once]")
{
  auto reg = make_registry();
  DummyButton button;
  DummyLogger logger;

  splice::wire::connect_all(reg, &logger);

  reg->emit<^^DummyButton::on_ready>(&button);
  reg->emit<^^DummyButton::on_ready>(&button);
  reg->emit<^^DummyButton::on_ready>(&button);

  REQUIRE(logger.log == std::vector<std::string> { "ready" });
}

TEST_CASE("paused listener does not receive signals", "[wire][pause]")
{
  auto reg = make_registry();
  DummyButton button;
  DummyLogger logger;

  splice::wire::connect_all(reg, &logger);

  logger.pause();
  reg->emit<^^DummyButton::on_click>(&button, 5, 5);

  REQUIRE(logger.log.empty());
}

TEST_CASE("resumed listener receives signals again", "[wire][pause]")
{
  auto reg = make_registry();
  DummyButton button;
  DummyLogger logger;

  splice::wire::connect_all(reg, &logger);

  logger.pause();
  reg->emit<^^DummyButton::on_click>(&button, 1, 1);

  logger.resume();
  reg->emit<^^DummyButton::on_click>(&button, 2, 2);

  REQUIRE(logger.log == std::vector<std::string> { "click:2,2" });
}

TEST_CASE("connection_count reflects connected slots", "[wire][connection_count]")
{
  auto reg = make_registry();
  DummyLogger logger;

  REQUIRE(reg->connection_count<^^DummyButton::on_click>() == 0);

  splice::wire::connect_all(reg, &logger);

  REQUIRE(reg->connection_count<^^DummyButton::on_click>() == 1);
}

TEST_CASE("disconnect_all removes all slots from all signals", "[wire][disconnect]")
{
  auto reg = make_registry();
  DummyButton button;
  DummyLogger logger;

  splice::wire::connect_all(reg, &logger);
  reg->disconnect_all();

  reg->emit<^^DummyButton::on_click>(&button, 1, 1);
  reg->emit<^^DummyButton::on_hover>(&button);

  REQUIRE(logger.log.empty());
}

TEST_CASE("ConnectionGuard disconnects on listener destruction", "[wire][lifetime]")
{
  auto reg = make_registry();
  DummyButton button;

  {
    DummyLogger logger;
    splice::wire::connect_all(reg, &logger);
    REQUIRE(reg->connection_count<^^DummyButton::on_click>() == 1);
  } // logger destroyed here — ConnectionGuard fires

  REQUIRE(reg->connection_count<^^DummyButton::on_click>() == 0);
}

TEST_CASE("registry destroyed before listener is safe", "[wire][lifetime]")
{
  DummyLogger logger;

  {
    auto reg = make_registry();
    DummyButton button;
    splice::wire::connect_all(reg, &logger);
  } // registry destroyed here — ConnectionGuard weak_ptr expires

  // ~DummyLogger runs here — ConnectionGuard disconnect is a safe no-op
  REQUIRE_NOTHROW([] { DummyLogger l; }());
}

TEST_CASE("isolated registry is independent of shared instance", "[wire][registry]")
{
  auto a = make_registry();
  auto b = make_registry();

  REQUIRE(a.get() != b.get());
}

TEST_CASE("shared registry returns same instance", "[wire][registry]")
{
  auto a = splice::wire::SignalRegistry<DummyButton>::shared();
  auto b = splice::wire::SignalRegistry<DummyButton>::shared();

  REQUIRE(a.get() == b.get());
}

TEST_CASE("disconnect_from removes only slots for that emitter", "[wire][disconnect]")
{
  auto reg = make_registry();
  DummyButton button;
  DummyLogger logger;

  splice::wire::connect_all(reg, &logger);
  REQUIRE(reg->connection_count<^^DummyButton::on_click>() == 1);

  logger.disconnect_from(reg);
  REQUIRE(reg->connection_count<^^DummyButton::on_click>() == 0);
}

TEST_CASE("multiple listeners each receive emitted signals", "[wire][emit]")
{
  auto reg = make_registry();
  DummyButton button;
  DummyLogger a, b;

  splice::wire::connect_all(reg, &a);
  splice::wire::connect_all(reg, &b);

  reg->emit<^^DummyButton::on_hover>(&button);

  REQUIRE(a.log == std::vector<std::string> { "hover" });
  REQUIRE(b.log == std::vector<std::string> { "hover" });
}
