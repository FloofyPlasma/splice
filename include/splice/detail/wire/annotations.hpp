#pragma once

#include <meta>

#include "splice/detail/priority.hpp"

namespace splice::wire
{

  /// @brief Marks a method as a signal on an emitter type.
  ///
  /// The method must be declared but should not be defined. The wire layer
  /// generates the emit body via `SignalRegistry::emit`.
  ///
  /// @par Example
  /// @code
  /// class Button {
  /// public:
  ///     [[SPLICE_WIRE_SIGNAL]] void on_click(int x, int y);
  /// };
  /// @endcode
  struct signal
  {
  };

  /// @brief Marks a method as a one-shot signal on an emitter type.
  ///
  /// Fires once then automatically disconnects all listeners. Subsequent
  /// emits are no-ops. Useful for lifecycle events like `on_ready` or
  /// `on_loaded`.
  ///
  /// @par Example
  /// @code
  /// class Scene {
  /// public:
  ///     [[SPLICE_WIRE_SIGNAL_ONCE]] void on_ready();
  /// };
  /// @endcode
  struct signal_once
  {
  };

  /// @brief Marks a method as a slot on a listener type, connecting it to
  /// a specific signal via reflection.
  ///
  /// All fields are optional except `signal`.
  ///
  /// @par Fields
  ///
  /// - `signal`    - reflection of the signal method to connect to (required)
  /// - `priority`  - execution order relative to other slots on the same signal
  ///                 (default: `splice::hook::Priority::Normal`)
  /// - `condition` - consteval predicate; slot is skipped if it returns false
  ///                 (default: always fire)
  ///
  /// @par Example
  /// @code
  /// class Logger : public splice::wire::Connectable {
  /// public:
  ///     [[SPLICE_WIRE_SLOT(.signal = ^^Button::on_click)]]
  ///     void handle_click(int x, int y) { ... }
  ///
  ///     [[SPLICE_WIRE_SLOT(.signal = ^^Button::on_click,
  ///                        .priority = splice::hook::Priority::High)]]
  ///     void handle_click_early(int x, int y) { ... }
  /// };
  /// @endcode
  struct slot
  {
    /// @brief Reflection of the signal method this slot connects to.
    std::meta::info signal;

    /// @brief Execution order relative to other slots on the same signal.
    ///
    /// Lower values run first. Defaults to `splice::hook::Priority::Normal`.
    int priority = splice::hook::Priority::Normal;
  };

  /// @brief Marker type used to detect a `ConnectionGuard` member on a
  /// `Connectable` subclass via reflection.
  ///
  /// Not intended for direct use. Apply `Connectable` as a base class instead.
  struct connections
  {
  };

} // namespace splice::wire

// ---- macros ----------------------------------------------------------------

/// @brief Marks a method as a signal. The wire layer generates the emit body.
///
/// @par Example
/// @code
/// [[SPLICE_WIRE_SIGNAL]] void on_click(int x, int y);
/// @endcode
#define SPLICE_WIRE_SIGNAL                                                                                             \
  = splice::wire::signal { }

/// @brief Marks a method as a one-shot signal. Fires once then disconnects
/// all listeners automatically.
///
/// @par Example
/// @code
/// [[SPLICE_WIRE_SIGNAL_ONCE]] void on_ready();
/// @endcode
#define SPLICE_WIRE_SIGNAL_ONCE                                                                                        \
  = splice::wire::signal_once { }

/// @brief Marks a method as a slot connecting to a specific signal.
///
/// Accepts named fields from `splice::wire::slot`:
/// - `.signal`    (required) - reflection of the target signal
/// - `.priority`  (optional) - execution order
///
/// @par Example
/// @code
/// [[SPLICE_WIRE_SLOT(.signal = ^^Button::on_click)]]
/// void handle_click(int x, int y) { ... }
///
/// [[SPLICE_WIRE_SLOT(.signal = ^^Button::on_click,
///                    .priority = splice::hook::Priority::High)]]
/// void handle_click_early(int x, int y) { ... }
/// @endcode
#define SPLICE_WIRE_SLOT(...)                                                                                          \
  = splice::wire::slot { __VA_ARGS__ }
