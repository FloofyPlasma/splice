#pragma once

namespace splice::hook
{

  /// @brief Error type returned by hook registration functions.
  ///
  /// All registration functions return `std::expected<void, HookError>` and
  /// are marked `[[nodiscard]]`; unhandled errors produce a compiler warning.
  ///
  /// @par Example
  /// @code
  /// auto result = reg->inject<^^GameWorld::mineBlock, splice::hook::InjectPoint::Head>(fn);
  /// if (!result)
  ///     // handle result.error()
  /// @endcode
  enum class HookError
  {
    /// @brief A conflicting exclusive hook is already registered on this method.
    Conflict,
  };

} // namespace splice::hook
