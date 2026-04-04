#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "splice/detail/assert.hpp"
#include "splice/detail/hook/hook_chain.hpp"
#include "splice/detail/hook/meta_utils.hpp"
#include "splice/detail/priority.hpp"
#include "splice/detail/result.hpp"

namespace splice::hook
{
  /// @brief Per-class hook registry. Holds a `HookChain` for every method on @p T
  /// annotated with `[[= splice::hook::hookable{}]]`.
  ///
  /// Obtain the process-wide shared instance via `ClassRegistry<T>::shared()`.
  /// The registry is lazily constructed on first use and destroyed when all
  /// `shared_ptr` handles are released.
  ///
  /// @tparam T The class whose hookable methods this registry manages.
  ///
  /// @par Example
  /// @code
  /// SPLICE_HOOK_REGISTRY(GameWorld, g_world);
  ///
  /// g_world->inject<^^GameWorld::mineBlock,
  /// splice::hook::InjectPoint::Head>(fn);
  /// g_world->dispatch<^^GameWorld::mineBlock>(&world, &steve, 10, 64, 5);
  /// @endcode
  template<typename T>
  class ClassRegistry
  {
    static constexpr auto Methods = splice::detail::hookable_methods<T>();

    splice::detail::ChainTuple<T> m_chains;

    /// @brief Initialises every chain with its original function wrapper.
    /// Private, use `shared()` or `make_isolated()` to obtain an instance.
    ClassRegistry()
    {
      template for (constexpr std::meta::info m: Methods)
      {
        chain<m>().original = make_original<m>(static_cast<splice::detail::ParamTuple<m> *>(nullptr));
      }
    }

  public:
    /// @brief Creates a fresh, isolated registry instance not shared with any
    /// other caller.
    ///
    /// @note Intended for unit testing only; prefer `shared()` in production
    /// code.
    /// @returns A `shared_ptr` to a newly constructed `ClassRegistry<T>`.
    static std::shared_ptr<ClassRegistry<T>> make_isolated()
    {
      return std::shared_ptr<ClassRegistry<T>>(new ClassRegistry<T>());
    }

    /// @brief Returns the process-wide shared `ClassRegistry` for @p T,
    /// constructing it on first call if no live instance exists.
    ///
    /// The registry is destroyed automatically when all `shared_ptr` handles go
    /// out of scope. Thread-safe.
    ///
    /// @note Intended to be stored via the `SPLICE_HOOK_REGISTRY` macro:
    /// @code
    /// // gameworld_hooks.hpp
    /// SPLICE_HOOK_REGISTRY(GameWorld, g_world);
    /// @endcode
    /// @returns A `shared_ptr` to the shared `ClassRegistry<T>`.
    static std::shared_ptr<ClassRegistry<T>> shared()
    {
      static std::weak_ptr<ClassRegistry<T>> s_instance;
      static std::mutex s_mutex;

      std::lock_guard lock(s_mutex);
      if (auto p = s_instance.lock())
        return p;

      auto p = std::shared_ptr<ClassRegistry<T>>(new ClassRegistry<T>());
      s_instance = p;
      return p;
    }

    /// @brief Returns a reference to the `HookChain` for the reflected method @p
    /// Method.
    ///
    /// @tparam Method A reflection of a hookable member of @p T.
    template<std::meta::info Method>
    [[nodiscard]] __attribute__((always_inline)) auto &chain()
    {
      return std::get<typename splice::detail::ChainFor<T, Method>::type>(m_chains);
    }

    /// @brief Const overload of `chain()` for read-only contexts.
    ///
    /// @tparam Method A reflection of a hookable member of @p T.
    template<std::meta::info Method>
    [[nodiscard]] __attribute__((always_inline)) const auto &chain() const
    {
      return std::get<typename splice::detail::ChainFor<T, Method>::type>(m_chains);
    }

    /// @brief Registers a hook on @p Method at the given @p Point with the given
    /// @p priority.
    ///
    /// Lower priority values run first; prefer the `splice::hook::Priority::`
    /// named constants.
    ///
    /// The hook signature must be:
    /// @code
    /// void(CI&, T*, Params...)
    /// @endcode
    /// where `CI` is `splice::detail::CallbackInfo` for `void` methods or
    /// `splice::detail::CallbackInfoReturnable<Ret>` for non-void methods.
    ///
    /// @tparam Method   A reflection of a hookable member of @p T.
    /// @tparam Point    The injection point (`Head`, `Tail`, or `Return`).
    /// @tparam Fn       The callable type of the hook.
    /// @param  fn       The hook to register.
    /// @param  priority Execution order relative to other hooks at the same
    /// point.
    /// @returns `std::expected<void, HookError>`. Always check the return value,
    ///          unhandled errors produce a compiler warning.
    ///
    /// @par Example
    /// @code
    /// g_world->inject<^^GameWorld::mineBlock, splice::hook::InjectPoint::Head>(
    ///     [](splice::detail::CallbackInfo& ci, GameWorld*, Player* p, int, int
    ///     y, int) {
    ///         if (y == 0) ci.cancelled = true;
    ///     });
    /// @endcode
    template<std::meta::info Method, InjectPoint Point, typename Fn>
    [[nodiscard]] std::expected<void, HookError> inject(Fn &&fn, int priority = Priority::Normal)
    {
      static_assert(
          Point != InjectPoint::Return || !std::is_void_v<typename splice::detail::ChainFor<T, Method>::type::RetT>,
          "inject: InjectPoint::Return is not available on void methods.");

      using Hook = typename splice::detail::ChainFor<T, Method>::type::Hook;
      auto result = chain<Method>().add(Point, Hook(std::forward<Fn>(fn)), priority);

      SPLICE_ASSERT(result.has_value(), std::string(std::meta::identifier_of(Method)) + ": hook registration failed");

      return result;
    }

    /// @brief Registers a hook that rewrites a single argument of @p Method by
    /// index.
    ///
    /// The hook receives the current value of the argument and returns the
    /// replacement. Internally registers a `Head` hook.
    ///
    /// @tparam Method   A reflection of a hookable member of @p T.
    /// @tparam ArgIndex Zero-based index into the method's declared parameters,
    ///                  not counting the leading `T*` instance pointer.
    /// @tparam Fn       A callable with signature `ArgT(ArgT)`.
    /// @param  fn       The rewrite function.
    /// @param  priority Execution order relative to other hooks at the same
    /// point.
    /// @returns `std::expected<void, HookError>`.
    ///
    /// @par Example
    /// @code
    /// g_world->modify_arg<^^GameWorld::calcDamage, 1>(
    ///     [](float amount) -> float { return amount * 2.0f; });
    /// @endcode
    template<std::meta::info Method, size_t ArgIndex, typename Fn>
    [[nodiscard]] std::expected<void, HookError> modify_arg(Fn &&fn, int priority = Priority::Normal)
    {
      using Params = splice::detail::ParamTuple<Method>;

      static_assert(ArgIndex < std::tuple_size_v<Params>, "modify_arg: ArgIndex is out of range for this method.");

      // add 1 to skip `this` pointer from dispatch
      return chain<Method>().template add_modify_arg<ArgIndex + 1>(fn, priority);
    }

    /// @brief Registers a hook that rewrites the return value of @p Method.
    ///
    /// The hook receives the current return value and returns the replacement.
    /// Internally registers a `Return` hook. Only available on non-`void`
    /// methods.
    ///
    /// @tparam Method A reflection of a non-void hookable member of @p T.
    /// @tparam Fn     A callable with signature `Ret(Ret)`.
    /// @param  fn     The rewrite function.
    /// @param  priority Execution order relative to other hooks at the same
    /// point.
    /// @returns `std::expected<void, HookError>`.
    ///
    /// @par Example
    /// @code
    /// g_world->modify_return<^^GameWorld::calcDamage>(
    ///     [](float result) -> float {
    ///         return std::clamp(result, 0.0f, 20.0f);
    ///     });
    /// @endcode
    template<std::meta::info Method, typename Fn>
    [[nodiscard]] std::expected<void, HookError> modify_return(Fn &&fn, int priority = Priority::Normal)
    {
      using Chain = typename splice::detail::ChainFor<T, Method>::type;
      using Ret = typename Chain::RetT;

      static_assert(!std::is_void_v<Ret>, "modify_return: not available on void methods.");

      auto wrapper = [f = std::forward<Fn>(fn)](splice::detail::CallbackInfoReturnable<Ret> &ci, auto &&...) mutable
      {
        if (ci.return_value.has_value())
          ci.return_value = f(*ci.return_value);
      };

      return chain<Method>().add(InjectPoint::Return, typename Chain::Hook(std::move(wrapper)), priority);
    }

    /// @brief Registers the static functions annotated with `[[= splice::hooK::injection{/* ...
    /// */}]]` or `[[= splice::hook::modify_arg{/*...*/}]]` in @p Source as hooks based on the values present in the
    /// annotation
    ///
    /// @tparam Source The class containing the injections
    /// @returns `std::expected<void, HookError>`.
    template<typename Source>
    [[nodiscard]] std::expected<void, HookError> inject_all_static()
    {
      template for (constexpr std::meta::info m: splice::detail::injection_methods<Source>())
      {
        if constexpr (std::meta::is_static_member(m))
        {
          template for (constexpr std::meta::info a_m: [:std::meta::reflect_constant_array(
                                                             std::meta::annotations_of(m)):])
          {
            if constexpr (std::meta::type_of(a_m) == ^^const splice::hook::injection)
            {
              constexpr splice::hook::injection a = std::meta::extract<splice::hook::injection>(a_m);
              if constexpr (std::meta::parent_of(a.what) == ^^T) // only try to register hooks for the registry's type
              {
                using Chain = splice::detail::ChainFor<T, a.what>::type;

                auto ret = chain<a.what>().add(a.where, typename Chain::Hook([:m:]), a.priority);
                if (!ret)
                  return ret;
              }
            }
            if constexpr (std::meta::type_of(a_m) == ^^const splice::hook::modify_arg)
            {
              constexpr splice::hook::modify_arg a = std::meta::extract<splice::hook::modify_arg>(a_m);
              if constexpr (std::meta::parent_of(a.what) == ^^T)
              {
                auto ret = chain<a.what>().template add_modify_arg<a.arg + 1>([:m:], a.priority);
                if (!ret)
                  return ret;
              }
            }
          }
        }
      }
      return { };
    }

    /// @brief Registers the non-static functions annotated with `[[= injection{/* ...
    /// */}]]` in @p Source as hooks based on the values present in the annotation
    ///
    /// @param ptr The instance to use when calling the methods. If this pointer is
    /// discarded, the hook will do nothing
    ///
    /// @tparam Source The class containing the injections
    /// @returns `std::expected<void, HookError>`.
    template<typename Source>
    [[nodiscard]] std::expected<void, HookError> inject_all_instanced(std::shared_ptr<Source> ptr)
    {
      template for (constexpr std::meta::info m: splice::detail::injection_methods<Source>())
      {
        if constexpr (!std::meta::is_static_member(m))
        {
          template for (constexpr std::meta::info a_m: [:std::meta::reflect_constant_array(
                                                             std::meta::annotations_of(m)):])
          {
            if constexpr (std::meta::type_of(a_m) == ^^const splice::hook::injection)
            {
              constexpr splice::hook::injection a = std::meta::extract<splice::hook::injection>(a_m);
              if constexpr (std::meta::parent_of(a.what) == ^^T) // only try to register hooks for the registry's type
              {
                using Chain = splice::detail::ChainFor<T, a.what>::type;
                constexpr auto fn = unpackFunc<typename Chain::RetT, Source, splice::detail::ParamTuple<m>>(m);
                std::weak_ptr<Source> wp = ptr;
                auto wrapper = [src = std::move(wp), &fn](Chain::CI &ci, auto &&...args) mutable
                {
                  if (auto ptr = src.lock(); ptr)
                    (ptr.get()->*fn)(ci, (args)...);
                };

                auto ret = chain<a.what>().add(a.where, typename Chain::Hook(wrapper), a.priority);
                if (!ret)
                  return ret;
              }
            }
            if constexpr (std::meta::type_of(a_m) == ^^const splice::hook::modify_arg)
            {
              constexpr splice::hook::modify_arg a = std::meta::extract<splice::hook::modify_arg>(a_m);
              if constexpr (std::meta::parent_of(a.what) == ^^T)
              {
                using Type = std::tuple_element_t<a.arg, splice::detail::ParamTuple<a.what>>;
                using RType = std::conditional_t<std::is_reference_v<Type>, void, Type>;
                constexpr auto fn = std::meta::extract<RType (Source::*)(Type)>(m);
                std::weak_ptr<Source> wp = ptr;
                auto wrapper = [src = std::move(wp), &fn](auto &&arg) mutable
                {
                  if (auto ptr = src.lock(); ptr)
                    return (ptr.get()->*fn)(arg);
                  if constexpr (!std::is_void_v<RType>)
                    return arg;
                };
                auto ret = chain<a.what>().template add_modify_arg<a.arg + 1>(wrapper, a.priority);
                if (!ret)
                  return ret;
              }
            }
          }
        }
      }
      return { };
    }

    /// @brief Dispatches a call to @p Method through the full hook chain.
    ///
    /// The first argument must be a `T*` instance pointer, followed by the
    /// method's normal parameters.
    ///
    /// @tparam Method       A reflection of a hookable member of @p T.
    /// @tparam DispatchArgs Argument types forwarded to the chain.
    /// @param  args         The instance pointer followed by the method's
    /// parameters.
    ///
    /// @par Example
    /// @code
    /// g_world->dispatch<^^GameWorld::mineBlock>(&world, &steve, 10, 64, 5);
    /// @endcode
    template<std::meta::info Method, typename... DispatchArgs>
    auto dispatch(DispatchArgs &&...args)
    {
      return chain<Method>().dispatch(std::forward<DispatchArgs>(args)...);
    }

    /// @brief Prints a summary of all hookable methods and their registered hook
    /// counts to stdout.
    ///
    /// Useful during development to verify that hooks are being registered as
    /// expected.
    ///
    /// @par Example output
    /// @code
    /// [splice::hook] registry for GameWorld:
    ///   [mineBlock            ]  head: 1  tail: 1  return: 0
    ///   [calcDamage           ]  head: 0  tail: 0  return: 1
    /// @endcode
    void print_registry() const
    {
      std::println("[splice::hook] registry for {}:", std::string(std::meta::identifier_of(^^T)));
      template for (constexpr std::meta::info m: Methods)
      {
        std::println("  [{:<20}]  head: {}  tail: {}  return: {}", std::string(std::meta::identifier_of(m)),
            chain<m>().head_count(), chain<m>().tail_count(), chain<m>().return_count());
      }
    }

  private:
    /// @brief Constructs the original function wrapper for the reflected method
    /// @p m.
    ///
    /// The wrapper captures the instance pointer as the first argument, matching
    /// the `HookChain`'s expected signature.
    ///
    /// @tparam m      A reflection of a hookable member of @p T.
    /// @tparam Params The method's parameter types, deduced from the tuple
    /// pointer.
    template<std::meta::info m, typename... Params>
    static auto make_original(std::tuple<Params...> *)
    {
      using Fn = typename splice::detail::ChainFor<T, m>::type::Fn;
      return Fn { [](T *self, Params... args) { return (self->[:m:])(args...); } };
    }

    template<typename Ret, typename Source, typename ArgTuple, std::size_t... Idxs>
    consteval auto _unpackFuncImpl(std::meta::info m, std::index_sequence<Idxs...>)
    {
      return std::meta::extract<Ret (Source::*)(std::tuple_element_t<Idxs, ArgTuple>...)>(m);
    }

    template<typename Ret, typename Source, typename ArgTuple>
    consteval auto unpackFunc(std::meta::info m)
    {
      return _unpackFuncImpl<Ret, Source, ArgTuple>(m, std::make_index_sequence<std::tuple_size<ArgTuple>::value>());
    }
  };

} // namespace splice::hook

/// @brief Declares a shared hook registry handle for the given class.
///
/// Must be placed in a header file. The `inline` keyword ensures ODR safety
/// across translation units.
///
/// Expands to:
/// @code
/// inline auto name = splice::hook::ClassRegistry<Class>::shared();
/// @endcode
///
/// @param Class The class to create a registry for.
/// @param name  The variable name for the registry handle.
///
/// @par Example
/// @code
/// // gameworld_hooks.hpp
/// #include <splice/hook.hpp>
/// #include "gameworld.hpp"
///
/// SPLICE_HOOK_REGISTRY(GameWorld, g_world);
///
/// // mymod.cpp
/// #include "gameworld_hooks.hpp"
///
/// void init() {
///     g_world->inject<^^GameWorld::mineBlock,
///     splice::hook::InjectPoint::Head>(
///         [](splice::detail::CallbackInfo& ci, GameWorld*, Player* p, int, int
///         y, int) {
///             if (y == 0) ci.cancelled = true;
///         });
/// }
/// @endcode
#define SPLICE_HOOK_REGISTRY(Class, name) inline auto name = splice::hook::ClassRegistry<Class>::shared()

/// @brief Shorthand annotation for marking a method as hookable.
///
/// Expands to `[[= splice::hook::hookable{}]]`.
#define SPLICE_HOOKABLE                                                                                                \
  = splice::hook::hookable { }

/// @brief Shorthand annotation for marking a method as an injection at
/// `splice::hook::InjectPoint::Head` with the specified priority
#define SPLICE_PRIO_INJECT_HEAD(clazz, prio)                                                                           \
  = splice::hook::injection { .what = ^^clazz, .where = splice::hook::InjectPoint::Head, .priority = prio }

/// @brief Shorthand annotation for marking a method as an injection at
/// `splice::hook::InjectPoint::Head` with `Normal` priority
#define SPLICE_INJECT_HEAD(clazz) SPLICE_PRIO_INJECT_HEAD(clazz, splice::hook::Priority::Normal)

/// @brief Shorthand annotation for marking a method as an injection at
/// `splice::hook::InjectPoint::Tail` with the specified priority
#define SPLICE_PRIO_INJECT_TAIL(clazz, prio)                                                                           \
  = splice::hook::injection { .what = ^^clazz, .where = splice::hook::InjectPoint::Tail, .priority = prio }

/// @brief Shorthand annotation for marking a method as an injection at
/// `splice::hook::InjectPoint::Tail` with `Normal` priority
#define SPLICE_INJECT_TAIL(clazz) SPLICE_PRIO_INJECT_TAIL(clazz, splice::hook::Priority::Normal)

/// @brief Shorthand annotation for marking a method as an injection at
/// `splice::hook::InjectPoint::Return` with the specified priority
#define SPLICE_PRIO_INJECT_RETURN(clazz, prio)                                                                         \
  = splice::hook::injection { .what = ^^clazz, .where = splice::hook::InjectPoint::Return, .priority = prio }

/// @brief Shorthand annotation for marking a method as an injection at
/// `splice::hook::InjectPoint::Return` with `Normal` priority
#define SPLICE_INJECT_RETURN(clazz) SPLICE_PRIO_INJECT_TAIL(clazz, splice::hook::Priority::Normal)
