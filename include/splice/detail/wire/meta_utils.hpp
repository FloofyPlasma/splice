#pragma once

#include <array>
#include <meta>
#include <tuple>

#include "splice/detail/hook/meta_utils.hpp"
#include "splice/detail/wire/annotations.hpp"

namespace splice::detail::wire
{

  /// @brief Returns `true` if the reflected member @p m has a
  /// `[[SPLICE_WIRE_SIGNAL]]` annotation.
  consteval bool has_signal(std::meta::info m)
  {
    return !std::meta::annotations_of_with_type(m, ^^splice::wire::signal).empty();
  }

  /// @brief Returns `true` if the reflected member @p m has a
  /// `[[SPLICE_WIRE_SIGNAL_ONCE]]` annotation.
  consteval bool has_signal_once(std::meta::info m)
  {
    return !std::meta::annotations_of_with_type(m, ^^splice::wire::signal_once).empty();
  }

  /// @brief Returns `true` if the reflected member @p m has a
  /// `[[SPLICE_WIRE_SLOT(...)]]` annotation.
  consteval bool has_slot(std::meta::info m)
  {
    return !std::meta::annotations_of_with_type(m, ^^splice::wire::slot).empty();
  }

  /// @brief Returns `true` if @p m is a non-special member function annotated
  /// with either `[[SPLICE_WIRE_SIGNAL]]` or `[[SPLICE_WIRE_SIGNAL_ONCE]]`.
  ///
  /// Excludes constructors, destructors, and operators.
  consteval bool is_signal_method(std::meta::info m)
  {
    return std::meta::is_function(m) && std::meta::has_identifier(m) && !std::meta::is_constructor(m)
           && !std::meta::is_destructor(m) && !std::meta::is_operator_function(m)
           && (has_signal(m) || has_signal_once(m));
  }

  /// @brief Returns `true` if @p m is a non-special member function annotated
  /// with `[[SPLICE_WIRE_SLOT(...)]]`.
  ///
  /// Excludes constructors, destructors, and operators.
  consteval bool is_slot_method(std::meta::info m)
  {
    return std::meta::is_function(m) && std::meta::has_identifier(m) && !std::meta::is_constructor(m)
           && !std::meta::is_destructor(m) && !std::meta::is_operator_function(m) && has_slot(m);
  }

  /// @brief Returns a `std::array` of reflected signal methods on @p T,
  /// in declaration order. Includes both `signal` and `signal_once` methods.
  ///
  /// @tparam T The emitter class to inspect.
  template<typename T>
  consteval auto signal_methods()
  {
    constexpr std::size_t count = []
    {
      std::size_t n = 0;
      for (auto m: std::meta::members_of(^^T, std::meta::access_context::unchecked()))
        if (is_signal_method(m))
          n++;
      return n;
    }();

    std::array<std::meta::info, count> result { };
    std::size_t i = 0;
    for (auto m: std::meta::members_of(^^T, std::meta::access_context::unchecked()))
      if (is_signal_method(m))
        result[i++] = m;
    return result;
  }

  /// @brief Returns a `std::array` of reflected slot methods on @p T,
  /// in declaration order.
  ///
  /// @tparam T The listener class to inspect.
  template<typename T>
  consteval auto slot_methods()
  {
    constexpr std::size_t count = []
    {
      std::size_t n = 0;
      for (auto m: std::meta::members_of(^^T, std::meta::access_context::unchecked()))
        if (is_slot_method(m))
          n++;
      return n;
    }();

    std::array<std::meta::info, count> result { };
    std::size_t i = 0;
    for (auto m: std::meta::members_of(^^T, std::meta::access_context::unchecked()))
      if (is_slot_method(m))
        result[i++] = m;
    return result;
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
    std::size_t n = 0;
    for (auto m: std::meta::members_of(^^T, std::meta::access_context::unchecked()))
      if (is_signal_method(m))
        n++;
    return n;
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
  /// @tparam T The emitter class whose signal methods define the tuple's element types.
  template<typename T, auto Methods, std::size_t... Is>
  auto signal_chain_tuple_impl(std::index_sequence<Is...>)
      -> std::tuple<typename SignalChainFor<T, Methods[Is]>::type...>;

  template<typename T>
  using SignalChainTuple = decltype(signal_chain_tuple_impl<T, signal_methods<T>()>(
      std::make_index_sequence<signal_method_count<T>()> { }));

} // namespace splice::detail::wire
