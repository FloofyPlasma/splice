#pragma once

#include <algorithm>
#include <expected>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "splice/detail/priority.hpp"
#include "splice/detail/result.hpp"

namespace splice::detail
{

  /// @brief Passed to hooks on `void` methods. Supports cancellation.
  struct CallbackInfo
  {
    bool cancelled = false;
  };

  /// @brief Passed to hooks on non-`void` methods. Supports cancellation and
  /// return value inspection or override.
  ///
  /// If `return_value` is set when `cancelled` is `true`, that value is
  /// returned to the original caller.
  ///
  /// @tparam Ret The return type of the hooked method.
  template<typename Ret>
  struct CallbackInfoReturnable : CallbackInfo
  {
    std::optional<Ret> return_value;
  };

  /// @brief Selects `CallbackInfoReturnable<Ret>` for non-`void` methods and
  /// `CallbackInfo` for `void` methods.
  ///
  /// @tparam Ret The return type of the hooked method.
  template<typename Ret>
  using CallbackInfoFor = std::conditional_t<std::is_void_v<Ret>, CallbackInfo, CallbackInfoReturnable<Ret>>;

  /// @brief A single registered hook with an associated priority.
  ///
  /// Lower priority values run first.
  ///
  /// The `listener` field is used by `splice::wire::SignalRegistry` to track
  /// which `Connectable` instance owns this hook so it can be removed on
  /// disconnect. For `splice::hook::ClassRegistry` hooks it is always `nullptr`.
  ///
  /// @tparam Fn The hook's callable type.
  template<typename Fn>
  struct HookEntry
  {
    Fn fn;
    int priority = splice::hook::Priority::Normal;
    void *listener = nullptr;
  };

  /// @brief Holds the original function and all registered hooks for a single
  /// hookable method.
  ///
  /// @tparam Ret  The method's return type.
  /// @tparam Args The method's full parameter list, including the leading
  /// `Class*` instance pointer.
  ///
  /// The hook signature is `void(CI&, Args...)`, where `CI` is `CallbackInfo` for
  /// `void` methods or `CallbackInfoReturnable<Ret>` for non-`void` methods.
  ///
  /// @par Hook execution order
  /// 1. `Head` hooks, sorted ascending by priority (lowest value first).
  /// 2. Original function (skipped if any hook set `cancelled = true`).
  /// 3. `Tail` hooks, sorted ascending by priority.
  /// 4. `Return` hooks, with the return value already populated in `CI`.
  template<typename Ret, typename... Args>
  struct HookChain
  {
    /// @brief The `CallbackInfo` type for this chain.
    using CI = CallbackInfoFor<Ret>;

    /// @brief The original function type.
    using Fn = std::function<Ret(Args...)>;

    /// @brief The return type of the original function.
    using RetT = Ret;

    /// @brief Hook function type, receives `CI` by reference followed by all
    /// original method arguments.
    using Hook = std::function<void(CI &, Args &...)>;

    /// @brief The original method implementation.
    ///
    /// Can be replaced to wrap or override the original behaviour entirely.
    Fn original;

    /// @brief Hooks registered at `InjectPoint::Head`, sorted ascending by
    /// priority on insertion.
    std::vector<HookEntry<Hook>> head_hooks;

    /// @brief Hooks registered at `InjectPoint::Tail`, sorted ascending by
    /// priority on insertion.
    std::vector<HookEntry<Hook>> tail_hooks;

    /// @brief Hooks registered at `InjectPoint::Return`, sorted ascending by
    /// priority on insertion.
    ///
    /// Receives the about-to-be-returned value in `ci.return_value`.
    std::vector<HookEntry<Hook>> return_hooks;

    /// @brief Returns the number of hooks registered at `Head`.
    size_t head_count() const { return head_hooks.size(); }

    /// @brief Returns the number of hooks registered at `Tail`.
    size_t tail_count() const { return tail_hooks.size(); }

    /// @brief Returns the number of hooks registered at `Return`.
    size_t return_count() const { return return_hooks.size(); }

    /// @brief Registers a hook at the given inject @p point with the given @p
    /// priority.
    ///
    /// Hooks are inserted in sorted order, lowest priority value runs first.
    ///
    /// @param point    The injection point to register the hook at.
    /// @param fn       The hook callable.
    /// @param priority Execution order relative to other hooks at the same point.
    /// @returns `std::expected<void, splice::hook::HookError>`.
    std::expected<void, splice::hook::HookError> add(splice::hook::InjectPoint point, Hook fn,
        int priority = splice::hook::Priority::Normal, void *listener = nullptr)
    {
      auto &vec = hooks_for(point);
      auto it = std::lower_bound(
          vec.begin(), vec.end(), priority, [](const HookEntry<Hook> &e, int p) { return e.priority < p; });
      vec.insert(it, HookEntry<Hook> { std::move(fn), priority, listener });
      return { };
    }

    /// @brief Dispatches a call through the full hook chain.
    ///
    /// For `void` methods the return type is `void`. For non-`void` methods
    /// the return value is taken from `ci.return_value` if set by a hook,
    /// otherwise from the original function's return value.
    ///
    /// @param args The instance pointer followed by the method's parameters.
    auto dispatch(Args... args)
    {
      CI ci { };
      auto arg_tuple = std::tuple<Args...>(std::move(args)...);

      for (auto &e: head_hooks)
      {
        std::apply([&](auto &...a) { e.fn(ci, a...); }, arg_tuple);
        if (ci.cancelled)
          return early_return(ci);
      }

      if constexpr (!std::is_void_v<Ret>)
      {
        if (original)
          ci.return_value = std::apply(original, arg_tuple);
      } else
      {
        if (original)
          std::apply(original, arg_tuple);
      }

      for (auto &e: tail_hooks)
        std::apply([&](auto &...a) { e.fn(ci, a...); }, arg_tuple);

      for (auto &e: return_hooks)
        std::apply([&](auto &...a) { e.fn(ci, a...); }, arg_tuple);

      return final_return(ci);
    }

  private:
    /// @brief Returns the hook vector for the given inject @p point.
    std::vector<HookEntry<Hook>> &hooks_for(splice::hook::InjectPoint point)
    {
      switch (point)
      {
        case splice::hook::InjectPoint::Head:
          return head_hooks;
        case splice::hook::InjectPoint::Tail:
          return tail_hooks;
        case splice::hook::InjectPoint::Return:
          return return_hooks;
      }
      std::unreachable();
    }

    /// @brief Returns early on cancellation.
    ///
    /// For non-`void` methods, returns `ci.return_value` if set by a hook,
    /// otherwise a default-constructed `Ret{}`.
    auto early_return(CI &ci)
    {
      if constexpr (!std::is_void_v<Ret>)
        return ci.return_value.value_or(Ret { });
    }

    /// @brief Returns the final value after all hooks have run.
    ///
    /// For non-`void` methods, prefers `ci.return_value` if set by any hook,
    /// otherwise a default-constructed `Ret{}`.
    auto final_return(CI &ci)
    {
      if constexpr (!std::is_void_v<Ret>)
        return ci.return_value.value_or(Ret { });
    }
  };

} // namespace splice::detail
