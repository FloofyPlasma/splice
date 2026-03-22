#pragma once

#include <array>
#include <cstddef>
#include <meta>
#include <tuple>

#include "splice/detail/hook/hook_chain.hpp"
#include "splice/detail/meta_core.hpp"

namespace splice::hook
{

  /// @brief Marker type used to annotate methods that should be hookable.
  ///
  /// Apply with `[[= splice::hook::hookable{}]]` or the `SPLICE_HOOKABLE` macro
  /// on any non-special member function.
  ///
  /// @par Example
  /// @code
  /// struct GameWorld {
  ///     [[= splice::hook::hookable{}]] void mineBlock(int x, int y, int z);
  /// };
  /// @endcode
  struct hookable
  {
  };

  /// @brief Annotation for marking a method as a hook to be registered by
  /// `inject_all_X()`
  ///
  /// Apply with `[[= splice::hook::injection{/* ... */}]]` or with the
  /// `SPLICE[_PRIO]_INJECT_*` macros on any non-special member function.
  ///
  /// Non-static injection methods are registered via `inject_all_instanced`
  /// whereas static injection methods are registered via `inject_all_static`
  ///
  /// @par Example
  /// @code
  /// class C {
  ///     [[= splice::hook::injection{
  ///         .what = ^^GameWorld::mineBlock,
  ///         .where = splice::hook::InjectPoint::Head
  ///     }]]
  ///     void injectMine(splice::detail::CallbackInfo &, GameWorld *, int, int,
  ///     int);
  /// }
  /// @endcode
  struct injection
  {
    /// @brief The method to hook
    std::meta::info what;
    /// @brief What kind of hook this is
    InjectPoint where;
    /// @brief The priority of the hook
    std::size_t priority = Priority::Normal;
  };

  /// @brief Annotation for marking a method as a `modify_arg` hook to be registered by `inject_all_X()`
  ///
  /// Apply with `[[= splice::hook::modify_arg{/*...*/}]]` on any non-special member function
  ///
  /// Non-static injection methods are registered via `inject_all_instanced`
  /// whereas static injection methods are registered via `inject_all_static`
  ///
  /// @par Example
  /// @code
  /// class C {
  ///     [[= splice::hook::modify_arg{
  ///         .what = ^^GameWorld::mineBlock,
  ///         .arg = 1
  ///     }]]
  ///     int modifySecondInt(int);
  /// }
  /// @endcode
  struct modify_arg
  {
    /// @brief The method to modify the argument of
    std::meta::info what;
    /// @brief the priority of the hook
    std::size_t priority = Priority::Normal;
    /// @brief The index of the argument to modify
    std::size_t arg;
  };

} // namespace splice::hook

namespace splice::detail
{

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

  /// @brief A `std::tuple` whose elements match the parameter types of @p Method,
  /// in declaration order.
  ///
  /// @tparam Method A reflection of the method to inspect.
  ///
  /// @par Example
  /// For `void foo(int, float)`, `ParamTuple<^^foo>` is `std::tuple<int, float>`.
  template<std::meta::info Method>
  using ParamTuple = decltype(param_tuple_impl<Method>(std::make_index_sequence<param_count<Method>()> { }));

  /// @brief Constructs the `HookChain` type for a given @p Class, @p Method, and
  /// unpacked parameter tuple.
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

  /// @brief Yields the `HookChain` type for a given @p Class and reflected @p
  /// Method.
  ///
  /// The chain's signature is `HookChain<Ret, Class*, Params...>`, where `Ret`
  /// and `Params...` are derived from the method's reflection info.
  ///
  /// @tparam Class  The class that owns the method.
  /// @tparam Method A reflection of the method to build a chain for.
  template<typename Class, std::meta::info Method>
  struct ChainFor
  {
    using type = typename ChainBuilder<Class, Method, ParamTuple<Method>>::type;
  };

  /// @brief Returns `true` if @p m is a non-special member function annotated
  /// with
  /// `[[= splice::hook::hookable{}]]`.
  ///
  /// Excludes constructors, destructors, and operators.
  consteval bool is_hookable_method(std::meta::info m)
  {
    return splice::detail::is_normal_function(m) && splice::detail::has_annotation<splice::hook::hookable>(m);
  }

  /// @brief Returns a `std::array` of reflected methods on @p T annotated with
  /// `[[= splice::hook::hookable{}]]`, in declaration order.
  ///
  /// @tparam T The class to inspect.
  template<typename T>
  consteval auto hookable_methods()
  {
    return splice::detail::member_array<T, [](std::meta::info m) { return is_hookable_method(m); }>();
  }

  /// @brief returns the number of hookable methods on @p t as a plain
  /// `std::size_t` constant.
  ///
  /// @tparam t the class to inspect.
  template<typename T>
  consteval std::size_t hookable_method_count()
  {
    return splice::detail::member_count<T, [](std::meta::info m) { return is_hookable_method(m); }>();
  }

  /// @brief Returns `true` if @p m is a non-special member function annotated with
  /// `[[= splice::hook::injection{/* ... */}]]` or `[[= splice::hook::modify_arg{/*...*/}]]`.
  ///
  /// Excludes constructors, destructors, and operators.
  consteval bool is_injection_method(std::meta::info m)
  {
    return splice::detail::is_normal_function(m)
           && (splice::detail::has_annotation<splice::hook::injection>(m)
               || splice::detail::has_annotation<splice::hook::modify_arg>(m));
  }

  /// @brief Returns a `std::array` of reflected methods on @p T annotated with
  /// `[[= splice::hook::injection{/* ... */}]]` or `[[= splice::hook::modify_arg{/*...*/}]]`, in declaration order.
  ///
  /// @tparam T The class to inspect.
  template<typename T>
  consteval auto injection_methods()
  {
    return splice::detail::member_array<T, [](std::meta::info m) { return is_injection_method(m); }>();
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

} // namespace splice::detail
