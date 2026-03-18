// Demonstrates using Head and Tail hooks to log method calls and timing
// without modifying the original class.
//
// A common use case is observability: you want to know when methods are
// called, what arguments they received, and how long they took, without
// touching the original implementation.

#include <chrono>
#include <print>
#include <splice/splice.hpp>
#include <string>

class PaymentProcessor
{
public:
  [[= splice::hook::hookable {}]] bool processPayment(std::string_view account, float amount) { return amount > 0.0f; }

  [[= splice::hook::hookable {}]] void refund(std::string_view account, float amount) { }
};

SPLICE_HOOK_REGISTRY(PaymentProcessor, g_payments);

// Tracks when a method started so the Tail hook can compute elapsed time.
thread_local std::chrono::steady_clock::time_point g_call_start;

void setupLogging()
{
  g_payments->inject<^^PaymentProcessor::processPayment, splice::hook::InjectPoint::Head>(
      [](splice::detail::CallbackInfoReturnable<bool> &, PaymentProcessor *, std::string_view account, float amount)
      {
        g_call_start = std::chrono::steady_clock::now();
        std::println("[log] processPayment called: account: {}, amount: {:.2f}", account, amount);
      });

  g_payments->inject<^^PaymentProcessor::processPayment, splice::hook::InjectPoint::Tail>(
      [](splice::detail::CallbackInfoReturnable<bool> &ci, PaymentProcessor *, std::string_view, float)
      {
        auto elapsed = std::chrono::steady_clock::now() - g_call_start;
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        std::println("[log] processPayment returned {} in {}µs", ci.return_value.value_or(false), ms);
      });

  g_payments->inject<^^PaymentProcessor::refund, splice::hook::InjectPoint::Head>(
      [](splice::detail::CallbackInfo &, PaymentProcessor *, std::string_view account, float amount)
      { std::println("[log] refund called: account: {}, amount: {:.2f}", account, amount); });
}

int main()
{
  setupLogging();

  PaymentProcessor processor;

  g_payments->dispatch<^^PaymentProcessor::processPayment>(&processor, "acc-001", 49.99f);
  g_payments->dispatch<^^PaymentProcessor::processPayment>(&processor, "acc-002", -5.00f);
  g_payments->dispatch<^^PaymentProcessor::refund>(&processor, "acc-001", 49.99f);
}
