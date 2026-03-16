#pragma once

#include <array>
#include <meta>
#include <tuple>

namespace splice
{
  /// @brief Marker type used to annotate methods that should be hookable.
  ///
  /// Apply with `[[= hookable{}]]` on any non-special member function.
  struct hookable
  {
  };

  /// @brief Returns the number of parameters of the reflected @p Method.
  template<std::meta::info Method>
  consteval std::size_t param_count()
  {
    return std::meta::parameters_of(Method).size();
  }

  /// @brief Yields the type of the @p I th parameter of the reflected @p Method.
  ///
  /// @tparam Method A reflection of the method to inspect.
  /// @tparam I      Zero-based parameter index.
  template<std::meta::info Method, std::size_t I>
  struct NthParam
  {
    using type = [:std::meta::type_of(std::meta::parameters_of(Method)[I]):];
  };

  /// @brief Deduces a tuple of parameter types from an index sequence.
  ///
  /// @note Implementation helper, not intended for direct use. See `ParamTuple`.
  template<std::meta::info Method, std::size_t... Is>
  auto param_tuple_impl(std::index_sequence<Is...>) -> std::tuple<typename NthParam<Method, Is>::type...>;

  /// @brief A `std::tuple` whose elements match the parameter types of @p Method, in declaration order.
  ///
  /// @tparam Method A reflection of the method to inspect.
  ///
  /// @par Example
  /// For `void foo(int, float)`, `ParamTuple<^^foo>` is `std::tuple<int, float>`.
  template<std::meta::info Method>
  using ParamTuple = decltype(param_tuple_impl<Method>(std::make_index_sequence<param_count<Method>()> { }));

  /// @brief Forward declaration of `HookChain`. Defined in `hook_chain.hpp`.
  template<typename Ret, typename... Args>
  struct HookChain;

  /// @brief Constructs the `HookChain` type for a given @p Class, @p Method, and unpacked parameter tuple.
  ///
  /// @note Implementation helper, not intended for direct use. See `ChainFor`.
  template<typename Class, std::meta::info Method, typename ParamT>
  struct ChainBuilder;

  template<typename Class, std::meta::info Method, typename... Params>
  struct ChainBuilder<Class, Method, std::tuple<Params...>>
  {
    using Ret = [:std::meta::return_type_of(Method):];
    using type = HookChain<Ret, Class *, Params...>;
  };

  /// @brief Yields the `HookChain` type for a given @p Class and reflected @p Method.
  ///
  /// The chain's signature is `HookChain<Ret, Class*, Params...>`, where `Ret` and
  /// `Params...` are derived from the method's reflection info.
  ///
  /// @tparam Class  The class that owns the method.
  /// @tparam Method A reflection of the method to build a chain for.
  template<typename Class, std::meta::info Method>
  struct ChainFor
  {
    using type = typename ChainBuilder<Class, Method, ParamTuple<Method>>::type;
  };

  /// @brief Returns `true` if the reflected member @p m has a `[[= hookable{}]]` annotation.
  consteval bool has_hookable(std::meta::info m) { return !std::meta::annotations_of_with_type(m, ^^hookable).empty(); }

  /// @brief Returns `true` if @p m is a non-special member function annotated with `[[= hookable{}]]`.
  ///
  /// Excludes constructors, destructors, and operators.
  consteval bool is_hookable_method(std::meta::info m)
  {
    return std::meta::is_function(m) && std::meta::has_identifier(m) && !std::meta::is_constructor(m)
           && !std::meta::is_destructor(m) && !std::meta::is_operator_function(m) && has_hookable(m);
  }

  /// @brief Returns a `std::array` of reflected methods on @p T annotated with
  /// `[[= hookable{}]]`, in declaration order.
  ///
  /// @tparam T The class to inspect.
  template<typename T>
  consteval auto hookable_methods()
  {
    constexpr std::size_t count = []
    {
      std::size_t n = 0;
      for (auto m: std::meta::members_of(^^T, std::meta::access_context::unchecked()))
        if (is_hookable_method(m))
          n++;
      return n;
    }();

    std::array<std::meta::info, count> result { };
    std::size_t i = 0;
    for (auto m: std::meta::members_of(^^T, std::meta::access_context::unchecked()))
      if (is_hookable_method(m))
        result[i++] = m;
    return result;
  }

  /// @brief Deduces a tuple of `HookChain` types from an index sequence.
  ///
  /// @note Implementation helper, not intended for direct use. See `ChainTuple`.
  template<typename T, auto Methods, std::size_t... Is>
  auto chain_tuple_impl(std::index_sequence<Is...>) -> std::tuple<typename ChainFor<T, Methods[Is]>::type...>;

  /// @brief A `std::tuple` whose elements are the `HookChain` types for each
  /// hookable method on @p T, in declaration order.
  ///
  /// @tparam T The class whose hookable methods define the tuple's element types.
  template<typename T>
  using ChainTuple = decltype(chain_tuple_impl<T, hookable_methods<T>()>(
      std::make_index_sequence<hookable_methods<T>().size()> { }));
} // namespace splice
