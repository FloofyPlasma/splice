#pragma once

#include <memory>
#include <mutex>
#include <print>
#include <shared_mutex>
#include <string>
#include <utility>

#include "splice/detail/hook/registry.hpp"
#include "splice/detail/wire/annotations.hpp"
#include "splice/detail/wire/connectable.hpp"
#include "splice/detail/wire/meta_utils.hpp"

namespace splice::wire
{

  /// @brief Per-class signal registry. Holds a `HookChain` for every method on
  /// @p T annotated with `[[SPLICE_WIRE_SIGNAL]]` or `[[SPLICE_WIRE_SIGNAL_ONCE]]`.
  ///
  /// Obtain the process-wide shared instance via `SignalRegistry<T>::shared()`.
  /// The registry is lazily constructed on first use and destroyed when all
  /// `shared_ptr` handles are released.
  ///
  /// Signals reuse `splice::detail::HookChain` internally with no original
  /// function, slots are registered as `Head` hooks and `emit` is a dispatch
  /// call with `original` left null.
  ///
  /// Thread safety: slot registration and disconnection take an exclusive lock
  /// per signal chain. Emit takes a shared lock, allowing concurrent emits on
  /// the same signal.
  ///
  /// @tparam T The emitter class whose signal methods this registry manages.
  ///
  /// @par Example
  /// @code
  /// WIRE_REGISTRY(Button, g_button);
  ///
  /// Logger logger;
  /// splice::wire::connect_all(g_button, &logger);
  /// splice::wire::emit<^^Button::on_click>(g_button, &button, 42, 100);
  /// @endcode
  template<typename T>
  class SignalRegistry : public std::enable_shared_from_this<SignalRegistry<T>>
  {
    static constexpr auto Methods = splice::detail::wire::signal_methods<T>();

    splice::detail::wire::SignalChainTuple<T> m_chains;

    /// @brief Per-signal shared mutexes for thread-safe emit/connect.
    /// Mutable so const methods (connection_count, print_registry) can lock.
    mutable std::array<std::shared_mutex, splice::detail::wire::signal_method_count<T>()> m_mutexes;

    /// @brief Tracks which signal_once signals have already fired.
    std::array<std::atomic<bool>, splice::detail::wire::signal_method_count<T>()> m_fired;

    /// @brief Optional hook registry for routing emits through splice::hook.
    std::weak_ptr<splice::hook::ClassRegistry<T>> m_hook_registry;
    mutable std::mutex m_hook_registry_mutex;

    SignalRegistry()
    {
      for (auto &f: m_fired)
        f.store(false);
    }

  public:
    /// @brief Creates a fresh isolated registry not shared with any other caller.
    ///
    /// @note Intended for unit testing only; prefer `shared()` in production code.
    /// @returns A `shared_ptr` to a newly constructed `SignalRegistry<T>`.
    static std::shared_ptr<SignalRegistry<T>> make_isolated()
    {
      return std::shared_ptr<SignalRegistry<T>>(new SignalRegistry<T>());
    }

    /// @brief Returns the process-wide shared `SignalRegistry` for @p T,
    /// constructing it on first call if no live instance exists.
    ///
    /// Thread-safe. The registry is destroyed when all `shared_ptr` handles
    /// go out of scope.
    ///
    /// @returns A `shared_ptr` to the shared `SignalRegistry<T>`.
    static std::shared_ptr<SignalRegistry<T>> shared()
    {
      static std::weak_ptr<SignalRegistry<T>> s_instance;
      static std::mutex s_mutex;

      std::lock_guard lock(s_mutex);
      if (auto p = s_instance.lock())
        return p;

      auto p = std::shared_ptr<SignalRegistry<T>>(new SignalRegistry<T>());
      s_instance = p;
      return p;
    }

    /// @brief Returns a reference to the `HookChain` for @p Signal using
    /// index-based tuple access.
    ///
    /// @tparam Signal A reflection of a signal method on @p T.
    template<std::meta::info Signal>
    [[nodiscard]] __attribute__((always_inline)) auto &chain()
    {
      return std::get<splice::detail::wire::signal_index_of<T, Signal>()>(m_chains);
    }

    /// @brief Const overload of `chain()`.
    template<std::meta::info Signal>
    [[nodiscard]] __attribute__((always_inline)) const auto &chain() const
    {
      return std::get<splice::detail::wire::signal_index_of<T, Signal>()>(m_chains);
    }

    /// @brief Connects all `[[SPLICE_WIRE_SLOT]]` methods on @p Listener that
    /// target a signal on @p T, and populates the listener's `ConnectionGuard`
    /// via its `Connectable` base.
    ///
    /// Reflects over @p Listener at compile time, finds every slot whose
    /// `.signal` field matches a signal on @p T, and registers it. Slots
    /// targeting other emitter types are silently skipped.
    ///
    /// @tparam Listener A type inheriting from `splice::wire::Connectable`.
    /// @param  listener Pointer to the listener instance.
    template<typename Listener>
    void connect_all(Listener *listener)
    {
      static_assert(std::is_base_of_v<Connectable, Listener>,
          "connect_all: Listener must inherit from splice::wire::Connectable");

      template for (constexpr std::meta::info slot_m: splice::detail::wire::slot_methods<Listener>())
      {
        // Use std::meta::extract directly on the annotation. Avoids
        // annotations_of_with_type which returns a heap-allocated vector
        // and cannot be used in a constant expression context.
        constexpr splice::wire::slot ann = std::meta::extract<splice::wire::slot>(
            std::meta::annotations_of_with_type(slot_m, ^^splice::wire::slot)[0]);
        constexpr std::meta::info target = ann.signal;

        // Extract priority as a plain int constexpr so it can be passed
        // to connect_slot at runtime, ann itself is consteval-only because
        // splice::wire::slot contains std::meta::info.
        constexpr int slot_priority = ann.priority;

        template for (constexpr std::meta::info signal_m: Methods)
        {
          if constexpr (target == signal_m)
          {
            connect_slot<signal_m, slot_m>(listener, slot_priority);
          }
        }
      }
    }

    /// @brief Explicitly connects a single slot method to a single signal.
    ///
    /// @tparam Signal     A reflection of a signal method on @p T.
    /// @tparam SlotMethod A reflection of a slot method on @p Listener.
    /// @tparam Listener   A type inheriting from `splice::wire::Connectable`.
    /// @param  listener   Pointer to the listener instance.
    /// @param  priority   Execution order (default: `splice::hook::Priority::Normal`).
    template<std::meta::info Signal, std::meta::info SlotMethod, typename Listener>
    void connect(Listener *listener, int priority = splice::hook::Priority::Normal)
    {
      static_assert(
          std::is_base_of_v<Connectable, Listener>, "connect: Listener must inherit from splice::wire::Connectable");

      connect_slot<Signal, SlotMethod>(listener, priority);
    }

    /// @brief Disconnects all slots belonging to @p listener from every signal.
    ///
    /// Called automatically by `ConnectionGuard` on destruction. Can also be
    /// called manually to disconnect early.
    ///
    /// @param listener Pointer to the listener to disconnect.
    void __attribute__((always_inline)) disconnect_all(Connectable *listener)
    {
      template for (constexpr std::meta::info signal_m: Methods)
      {
        constexpr std::size_t idx = splice::detail::wire::signal_index_of<T, signal_m>();
        std::unique_lock lock(m_mutexes[idx]);
        auto &c = chain<signal_m>();

        auto remove = [&](auto &hooks)
        {
          hooks.erase(
              std::remove_if(hooks.begin(), hooks.end(), [listener](const auto &e) { return e.listener == listener; }),
              hooks.end());
        };

        remove(c.head_hooks);
        remove(c.tail_hooks);
        remove(c.return_hooks);
      }
    }

    /// @brief Disconnects all slots from a specific signal.
    ///
    /// @tparam Signal A reflection of a signal method on @p T.
    template<std::meta::info Signal>
    void __attribute__((always_inline)) disconnect_all()
    {
      constexpr std::size_t idx = splice::detail::wire::signal_index_of<T, Signal>();
      std::unique_lock lock(m_mutexes[idx]);
      auto &c = chain<Signal>();
      c.head_hooks.clear();
      c.tail_hooks.clear();
      c.return_hooks.clear();
    }

    /// @brief Disconnects all slots from all signals.
    void disconnect_all()
    {
      template for (constexpr std::meta::info signal_m: Methods) { disconnect_all<signal_m>(); }
    }

    /// @brief Emits a signal, invoking all connected non-paused slots in
    /// priority order.
    ///
    /// For `signal_once` signals: fires on the first call only, then
    /// atomically marks the signal as fired and clears all slots. Subsequent
    /// calls are no-ops.
    ///
    /// If an optional `ClassRegistry` has been set via `set_hook_registry`,
    /// the emit is routed through the hook dispatch layer instead of directly
    /// through the signal chain.
    ///
    /// Thread-safe: takes a shared lock allowing concurrent emits.
    ///
    /// @tparam Signal A reflection of a signal method on @p T.
    /// @param  emitter Pointer to the emitter instance.
    /// @param  args    Arguments forwarded to each slot.
    template<std::meta::info Signal, typename... Args>
    void emit(T *emitter, Args &&...args)
    {
      constexpr std::size_t idx = splice::detail::wire::signal_index_of<T, Signal>();
      constexpr bool is_once = splice::detail::wire::has_signal_once(Signal);

      // signal_once: check fired flag before taking any lock
      if constexpr (is_once)
      {
        if (m_fired[idx].load())
          return;
      }

      std::shared_lock lock(m_mutexes[idx]);

      if constexpr (is_once)
      {
        // double-checked under lock
        bool expected = false;
        if (!m_fired[idx].compare_exchange_strong(expected, true))
          return;
      }

      // route through hook registry if one is set. Guarded by a helper
      // template so ClassRegistry<T> is never instantiated when T has no
      // hookable methods (which would produce an empty ChainTuple)
      if (try_hook_dispatch<Signal>(emitter, std::forward<Args>(args)...))
      {
        if constexpr (is_once)
          disconnect_all<Signal>();
        return;
      }

      // direct dispatch through the signal chain
      chain<Signal>().dispatch(emitter, std::forward<Args>(args)...);

      if constexpr (is_once)
      {
        lock.unlock();
        disconnect_all<Signal>();
      }
    }

    /// @brief Returns the number of slots connected to @p Signal.
    ///
    /// @tparam Signal A reflection of a signal method on @p T.
    template<std::meta::info Signal>
    std::size_t connection_count() const
    {
      constexpr std::size_t idx = splice::detail::wire::signal_index_of<T, Signal>();
      std::shared_lock lock(m_mutexes[idx]);
      return chain<Signal>().head_hooks.size();
    }

    /// @brief Optionally routes emits through a `splice::hook::ClassRegistry`.
    ///
    /// When set, `emit` calls `hook_reg->dispatch` instead of invoking the
    /// signal chain directly. This allows `splice::hook` hooks to intercept,
    /// cancel, or modify signal emissions.
    ///
    /// @param hook_reg The hook registry to route through.
    void set_hook_registry(std::shared_ptr<splice::hook::ClassRegistry<T>> hook_reg)
    {
      std::lock_guard lock(m_hook_registry_mutex);
      m_hook_registry = hook_reg;
    }

    /// @brief Prints a summary of all signals and their connected slot counts.
    ///
    /// @par Example output
    /// @code
    /// [splice::wire] registry for Button:
    ///   [on_click    ]  slots: 2  once: false  fired: false
    ///   [on_ready    ]  slots: 1  once: true   fired: true
    /// @endcode
    void print_registry() const
    {
      std::println("[splice::wire] registry for {}:", std::string(std::meta::identifier_of(^^T)));
      template for (constexpr std::meta::info m: Methods)
      {
        constexpr std::size_t idx = splice::detail::wire::signal_index_of<T, m>();
        constexpr bool is_once = splice::detail::wire::has_signal_once(m);
        std::println("  [{:<20}]  slots: {}  once: {}  fired: {}", std::string(std::meta::identifier_of(m)),
            connection_count<m>(), is_once, is_once ? m_fired[idx].load() : false);
      }
    }

  private:
    /// @brief Attempts to route @p Signal through the hook registry if one is
    /// set. Returns `true` if the hook registry handled the call, `false` if
    /// the caller should proceed with direct dispatch.
    ///
    /// Two overloads selected by `if constexpr` on hookable method count:
    /// - When T has hookable methods: checks the registry and dispatches
    /// - When T has no hookable methods: always returns false (no-op)
    ///
    /// The split avoids instantiating `ClassRegistry<T>::dispatch` when T
    /// has no hookable methods (which would produce an empty `ChainTuple`
    /// and fail to compile).
    template<std::meta::info Signal, typename... Args>
    bool try_hook_dispatch(T *emitter, Args &&...args)
    {
      if constexpr (splice::detail::hookable_method_count<T>() > 0)
      {
        std::lock_guard hook_lock(m_hook_registry_mutex);
        if (auto hook_reg = m_hook_registry.lock())
        {
          hook_reg->template dispatch<Signal>(emitter, std::forward<Args>(args)...);
          return true;
        }
      }
      return false;
    }

    /// @brief Registers a single slot method as a Head hook on @p Signal,
    /// and appends a disconnector to the listener's `ConnectionGuard`.
    template<std::meta::info Signal, std::meta::info SlotMethod, typename Listener>
    void connect_slot(Listener *listener, int priority)
    {
      using Chain = typename splice::detail::wire::SignalChainFor<T, Signal>::type;
      using CI = typename Chain::CI;
      using Params = splice::detail::ParamTuple<Signal>;

      constexpr std::size_t idx = splice::detail::wire::signal_index_of<T, Signal>();

      auto hook = make_slot_hook<Signal, SlotMethod, Listener, CI>(listener, static_cast<Params *>(nullptr));

      {
        std::unique_lock lock(m_mutexes[idx]);
        [[maybe_unused]] auto result
            = chain<Signal>().add(splice::hook::InjectPoint::Head, std::move(hook), priority, listener);
      }

      // weak_ptr ensures the disconnector is a safe no-op if the registry
      // is destroyed before the listener
      std::weak_ptr<SignalRegistry<T>> weak_self = this->shared_from_this();

      static_cast<Connectable *>(listener)->m_connection_guard.add_disconnector(
          [weak_self, listener]()
          {
            if (auto reg = weak_self.lock())
              reg->disconnect_all(static_cast<Connectable *>(listener));
          });
    }

    /// @brief Builds the hook callable for a slot method, wrapping it with
    /// a pause check before invoking the slot.
    template<std::meta::info Signal, std::meta::info SlotMethod, typename Listener, typename CI, typename... Params>
    auto make_slot_hook(Listener *listener, std::tuple<Params...> *)
    {
      return
          typename splice::detail::wire::SignalChainFor<T, Signal>::type::Hook { [listener](CI &, T *, Params &...args)
            {
              if (static_cast<Connectable *>(listener)->is_paused())
                return;

              (listener->[:SlotMethod:])(args...);
            } };
    }
  };

} // namespace splice::wire

// ---- macros ----------------------------------------------------------------

/// @brief Declares a shared signal registry handle for the given emitter class.
///
/// @par Example
/// @code
/// WIRE_REGISTRY(Button, g_button);
/// @endcode
#define WIRE_REGISTRY(Emitter, name) inline auto name = splice::wire::SignalRegistry<Emitter>::shared()

/// @brief Declares both a shared `SignalRegistry` and a shared `ClassRegistry`
/// for the given emitter class, and wires them together so that emits are
/// routed through the hook dispatch layer.
///
/// Produces two variables: `name_signals` and `name_hooks`.
///
/// @par Example
/// @code
/// WIRE_HOOK_REGISTRY(Button, g_button);
/// // g_button_signals -> splice::wire::SignalRegistry<Button>
/// // g_button_hooks   -> splice::hook::ClassRegistry<Button>
/// @endcode
#define WIRE_HOOK_REGISTRY(Emitter, name)                                                                              \
  inline auto name##_signals = splice::wire::SignalRegistry<Emitter>::shared();                                        \
  inline auto name##_hooks = splice::hook::ClassRegistry<Emitter>::shared();                                           \
  namespace                                                                                                            \
  {                                                                                                                    \
    [[maybe_unused]] inline auto _##name##_wire_init = (name##_signals->set_hook_registry(name##_hooks), 0);           \
  }

// ---- free function wrappers ------------------------------------------------

namespace splice::wire
{

  /// @brief Emits a signal through the provided registry.
  template<std::meta::info Signal, typename Emitter, typename... Args>
  void emit(std::shared_ptr<SignalRegistry<Emitter>> reg, Emitter *emitter, Args &&...args)
  {
    reg->template emit<Signal>(emitter, std::forward<Args>(args)...);
  }

  /// @brief Connects all matching slots on @p listener to signals on the
  /// emitter type managed by @p reg.
  template<typename Emitter, typename Listener>
  void connect_all(std::shared_ptr<SignalRegistry<Emitter>> reg, Listener *listener)
  {
    reg->connect_all(listener);
  }

} // namespace splice::wire
