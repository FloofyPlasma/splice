// Demonstrates using hooks to mock method behavior in tests: overriding
// return values and intercepting calls without subclassing or modifying
// the original class.
//
// This is useful when you want to test code that depends on a class whose
// behavior you want to control. Because hooks live in an isolated registry,
// each test can construct its own and tear it down cleanly.

#include <cassert>
#include <cmath>
#include <print>
#include <splice/splice.hpp>
#include <string>

class WeatherService
{
public:
  [[= splice::hook::hookable { }]] float getTemperature(std::string_view location) { return 20.7f; }

  [[= splice::hook::hookable { }]] bool isServiceAvailable() { return true; }
};

// A helper that consumes WeatherService through the registry.
// In a real codebase this might be a UI component, a controller, etc.
std::string formatWeatherReport(WeatherService *svc, std::shared_ptr<splice::hook::ClassRegistry<WeatherService>> reg)
{
  if (!reg->dispatch<^^WeatherService::isServiceAvailable>(svc))
    return "Service unavailable.";

  float temp = reg->dispatch<^^WeatherService::getTemperature>(svc, "London");
  return std::format("London: {:.1f}°C", temp);
}

void test_normal_conditions()
{
  std::println("--- normal conditions ---");

  auto reg = splice::hook::ClassRegistry<WeatherService>::make_isolated();

  reg->inject<^^WeatherService::getTemperature, splice::hook::InjectPoint::Head>(
      [](splice::detail::CallbackInfoReturnable<float> &ci, WeatherService *, std::string_view location)
      {
        std::println("  [mock] getTemperature called for '{}'", location);
        ci.return_value = 23.5f;
        ci.cancelled = true;
      });

  WeatherService svc;
  auto report = formatWeatherReport(&svc, reg);
  std::println("  report: {}", report);
  assert(report == "London: 23.5°C");
}

void test_service_unavailable()
{
  std::println("\n--- service unavailable ---");

  auto reg = splice::hook::ClassRegistry<WeatherService>::make_isolated();

  reg->inject<^^WeatherService::isServiceAvailable, splice::hook::InjectPoint::Head>(
      [](splice::detail::CallbackInfoReturnable<bool> &ci, WeatherService *)
      {
        std::println("  [mock] isServiceAvailable returning false");
        ci.return_value = false;
        ci.cancelled = true;
      });

  WeatherService svc;
  auto report = formatWeatherReport(&svc, reg);
  std::println("  report: {}", report);
  assert(report == "Service unavailable.");
}

void test_modify_return()
{
  std::println("\n--- modify_return rounding ---");

  auto reg = splice::hook::ClassRegistry<WeatherService>::make_isolated();

  reg->modify_return<^^WeatherService::getTemperature>(
      [](float temp) -> float
      {
        float rounded = std::round(temp);
        std::println("  [mock] rounding {:.1f} -> {:.1f}", temp, rounded);
        return rounded;
      });

  WeatherService svc;
  auto report = formatWeatherReport(&svc, reg);
  std::println("  report: {}", report);
}

int main()
{
  test_normal_conditions();
  test_service_unavailable();
  test_modify_return();
}
