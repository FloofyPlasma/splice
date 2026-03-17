#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace splice::wire
{
  template<typename T>
  class SignalRegistry;

  /// @brief Manages the lifetime of all signal connections for a listener.
  ///
  /// Holds a list of type-erased disconnectors, one per signal connection.
  /// Each disconnector captures a `weak_ptr` to its `SignalRegistry` so that
  /// if the emitter is destroyed before the listener, the disconnector is a
  /// safe no-op.
  ///
  /// `ConnectionGuard` is not copyable, connections are tied to a specific
  /// listener instance. It is movable so that `Connectable` subclasses can
  /// be moved.
  ///
  /// Not intended for direct use. Inherit from `Connectable` instead.
  class ConnectionGuard
  {
  public:
    ConnectionGuard() = default;
    ConnectionGuard(const ConnectionGuard &) = delete;
    ConnectionGuard &operator=(const ConnectionGuard &) = delete;

    ConnectionGuard(ConnectionGuard &&other) noexcept
    {
      std::lock_guard lock(other.m_mutex);
      m_disconnectors = std::move(other.m_disconnectors);
    }

    ConnectionGuard &operator=(ConnectionGuard &&other) noexcept
    {
      if (this == &other)
        return *this;
      std::scoped_lock lock(m_mutex, other.m_mutex);
      m_disconnectors = std::move(other.m_disconnectors);
      return *this;
    }

    /// @brief Invokes all disconnectors, removing this listener from every
    /// connected signal. Safe to call if any emitters have already been destroyed.
    ~ConnectionGuard() { disconnect_all(); }

    /// @brief Appends a type-erased disconnector for a single connection.
    ///
    /// Called internally by `SignalRegistry::connect_all` and
    /// `SignalRegistry::connect`. Not intended for direct use.
    ///
    /// @param disconnector A callable that removes one slot from one registry.
    ///                     Must be safe to call even if the registry is gone
    ///                     (i.e. should capture a `weak_ptr` to the registry).
    void add_disconnector(std::function<void()> disconnector)
    {
      std::lock_guard lock(m_mutex);
      m_disconnectors.push_back(std::move(disconnector));
    }

    /// @brief Invokes all disconnectors and clears the list.
    ///
    /// After this call the guard holds no connections. Safe to call multiple times.
    void disconnect_all()
    {
      std::vector<std::function<void()>> local;
      {
        std::lock_guard lock(m_mutex);
        local = std::move(m_disconnectors);
      }
      // invoke outside the lock so disconnect callbacks don't deadlock
      // if they re-enter the guard
      for (auto &d: local)
        d();
    }

    /// @brief Returns the number of active connections held by this guard.
    std::size_t connection_count() const
    {
      std::lock_guard lock(m_mutex);
      return m_disconnectors.size();
    }

  private:
    mutable std::mutex m_mutex;
    std::vector<std::function<void()>> m_disconnectors;
  };

  /// @brief Base class for any type that wants to receive wire signals.
  ///
  /// Inherit from `Connectable` to get automatic connection lifetime management.
  /// `SignalRegistry::connect_all` detects the base class via reflection and
  /// populates the internal `ConnectionGuard` automatically. No member
  /// declaration or annotation required on the subclass.
  ///
  /// When a `Connectable` is destroyed its `ConnectionGuard` destructs first,
  /// disconnecting it from every signal it was connected to. If the emitter
  /// has already been destroyed the disconnectors are safe no-ops.
  ///
  /// @par Example
  /// @code
  /// class Logger : public splice::wire::Connectable {
  /// public:
  ///     [[SPLICE_WIRE_SLOT(.signal = ^^Button::on_click)]]
  ///     void handle_click(int x, int y) {
  ///         std::println("clicked at ({}, {})", x, y);
  ///     }
  /// };
  ///
  /// Logger logger;
  /// splice::wire::connect_all(g_button, &logger); // guard populated automatically
  /// // ~Logger disconnects from all signals
  /// @endcode
  class Connectable
  {
  public:
    Connectable() = default;
    virtual ~Connectable() = default;

    Connectable(const Connectable &) = delete;
    Connectable &operator=(const Connectable &) = delete;

    Connectable(Connectable &&) = default;
    Connectable &operator=(Connectable &&) = default;

    /// @brief Temporarily suppresses slot invocation without disconnecting.
    ///
    /// While paused, any signals emitted to this listener are silently skipped.
    /// Connections remain intact and resume normally after `resume()` is called.
    void pause() { m_paused.store(true); }

    /// @brief Re-enables slot invocation after a `pause()` call.
    void resume() { m_paused.store(false); }

    /// @brief Returns true if this listener is currently paused.
    bool is_paused() const { return m_paused.load(); }

    /// @brief Disconnects this listener from all signals on a specific emitter.
    ///
    /// Removes only the connections belonging to @p reg, leaving connections
    /// to other emitters intact.
    ///
    /// @tparam Emitter The emitter type whose registry to disconnect from.
    /// @param  reg     The `SignalRegistry` to disconnect from.
    template<typename Emitter>
    void disconnect_from(std::shared_ptr<SignalRegistry<Emitter>> reg)
    {
      reg->disconnect_all(this);
    }

    /// @brief The connection guard managing this listener's signal lifetime.
    ///
    /// Populated automatically by `SignalRegistry::connect_all`.
    /// Exposed as public so the registry can append disconnectors via reflection,
    /// but should not be manipulated directly by user code.
    ConnectionGuard m_connection_guard;

  private:
    std::atomic<bool> m_paused { false };
  };

} // namespace splice::wire
