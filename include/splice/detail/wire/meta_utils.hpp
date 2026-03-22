#pragma once

#include <array>
#include <meta>
#include <tuple>

#include "splice/detail/hook/meta_utils.hpp"
#include "splice/detail/meta_core.hpp"
#include "splice/detail/wire/annotations.hpp"

namespace splice::detail::wire
{
  /// @brief Returns `true` if @p m is a non-special member function annotated
  /// with either `[[SPLICE_WIRE_SIGNAL]]` or `[[SPLICE_WIRE_SIGNAL_ONCE]]`.
  ///
  /// Excludes constructors, destructors, and operators.
  consteval bool is_signal_method(std::meta::info m)
  {
    return splice::detail::is_normal_function(m)
           && (splice::detail::has_annotation<splice::wire::signal>(m)
               || splice::detail::has_annotation<splice::wire::signal_once>(m));
  }

  /// @brief Returns `true` if @p m is a non-special member function annotated
  /// with `[[SPLICE_WIRE_SLOT(...)]]`.
  ///
  /// Excludes constructors, destructors, and operators.
  consteval bool is_slot_method(std::meta::info m)
  {
    return splice::detail::is_normal_function(m) && splice::detail::has_annotation<splice::wire::slot>(m);
  }

  /// @brief Returns a `std::array` of reflected signal methods on @p T,
  /// in declaration order. Includes both `signal` and `signal_once` methods.
  ///
  /// @tparam T The emitter class to inspect.
  template<typename T>
  consteval auto signal_methods()
  {
    return splice::detail::member_array<T, [](std::meta::info m) { return is_signal_method(m); }>();
  }

  /// @brief Returns a `std::array` of reflected slot methods on @p T,
  /// in declaration order.
  ///
  /// @tparam T The listener class to inspect.
  template<typename T>
  consteval auto slot_methods()
  {
    return splice::detail::member_array<T, [](std::meta::info m) { return is_slot_method(m); }>();
  }

  /// @brief Yields the `HookChain` type for a given signal method @p Method
  /// on emitter @p T.
  ///
  /// Signals reuse `splice::detail::HookChain` with no original function.
  /// The slot list IS the chain's hook list, and emit is just dispatch with
  /// original left null.
  ///
  /// @tparam T      The emitter class.
  /// @tparam Method A reflection of a signal method on @p T.
  template<typename T, std::meta::info Method>
  struct SignalChainFor
  {
    using type = typename splice::detail::ChainFor<T, Method>::type;
  };

  /// @brief Returns the number of signal methods on @p T as a plain
  /// `std::size_t`, safe to use in `if constexpr` without triggering
  /// consteval-only type restrictions.
  ///
  /// @tparam T The emitter class to inspect.
  template<typename T>
  consteval std::size_t signal_method_count()
  {
    return splice::detail::member_count<T, [](std::meta::info m) { return is_signal_method(m); }>();
  }

  /// @brief Returns the index of @p Signal in the signal methods array of @p T.
  ///
  /// Defined as a free consteval function in splice::detail::wire rather than
  /// a static member of SignalRegistry, allowing it to be called from constexpr
  /// variable initializers in template instantiation context.
  template<typename T, std::meta::info Signal>
  consteval std::size_t signal_index_of()
  {
    constexpr auto methods = signal_methods<T>();
    for (std::size_t i = 0; i < methods.size(); ++i)
      if (methods[i] == Signal)
        return i;
    return methods.size(); // unreachable for valid signal methods
  }
  /// @brief A `std::tuple` whose elements are the `HookChain` types for each
  /// signal method on @p T, in declaration order.
  ///
  /// @tparam T The emitter class whose signal methods define the tuple's element
  /// types.
  template<typename T, auto Methods, std::size_t... Is>
  auto signal_chain_tuple_impl(std::index_sequence<Is...>)
      -> std::tuple<typename SignalChainFor<T, Methods[Is]>::type...>;

  template<typename T>
  using SignalChainTuple = decltype(signal_chain_tuple_impl<T, signal_methods<T>()>(
      std::make_index_sequence<signal_method_count<T>()> { }));

} // namespace splice::detail::wire
