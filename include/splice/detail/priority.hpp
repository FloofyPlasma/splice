#pragma once

namespace splice::hook
{
  /// @brief Specifies where in the target method's execution a hook is injected.
  enum class InjectPoint
  {
    /// @brief Injected before the original method body executes.
    ///
    /// Setting `ci.cancelled = true` skips the original and all `Tail` hooks.
    Head,

    /// @brief Injected after the original method body executes.
    ///
    /// Always runs unless a `Head` hook cancelled the call.
    Tail,

    /// @brief Injected at every return path.
    ///
    /// `ci.return_value` contains the about-to-be-returned value and can be
    /// inspected or overridden. Only available on non-`void` methods.
    Return,
  };

  /// @brief Hook execution priority constants. Higher-named priorities run first.
  ///
  /// Internally, lower numeric values run first; the names reflect importance,
  /// not numeric order.
  ///
  /// Values can be used arithmetically to express relative ordering:
  /// @code
  /// reg->inject<^^GameWorld::mineBlock, splice::hook::InjectPoint::Head>(fn,
  ///     splice::hook::Priority::High + 1);
  /// @endcode
  namespace Priority
  {
    /// @brief Runs before all other hooks.
    inline constexpr int Highest = 0;

    /// @brief Runs before `Normal` and `Low` hooks.
    inline constexpr int High = 250;

    /// @brief Default priority, used when no priority is specified.
    inline constexpr int Normal = 500;

    /// @brief Runs after `Normal` and `High` hooks.
    inline constexpr int Low = 750;

    /// @brief Runs after all other hooks.
    inline constexpr int Lowest = 1000;
  } // namespace Priority
} // namespace splice::hook
