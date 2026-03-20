module;

#include "splice/splice.hpp"

export module splice;

/// @brief The splice namespace.
export namespace splice {
    namespace hook {
        using splice::hook::InjectPoint;
        using splice::hook::HookError;
        using splice::hook::ClassRegistry;

        using splice::hook::hookable;
        using splice::hook::injection;

        namespace Priority {
            using splice::hook::Priority::Highest;
            using splice::hook::Priority::High;
            using splice::hook::Priority::Normal;
            using splice::hook::Priority::Low;
            using splice::hook::Priority::Lowest;
        } // namespace Priority
    } // namespace hook

    namespace wire {
        using splice::wire::SignalRegistry;
        using splice::wire::ConnectionGuard;
        using splice::wire::Connectable;

        using splice::wire::signal;
        using splice::wire::signal_once;
        using splice::wire::slot;
        using splice::wire::connections;

        using splice::wire::emit;
        using splice::wire::connect_all;
    } // namespace wire
} // namespace splice
