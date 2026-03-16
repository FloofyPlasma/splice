#pragma once

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
#error "splice requires C++26 static reflection support. Please use GCC 16 or later and compile with -freflection."
#endif

#include "splice/detail/hook_chain.hpp"
#include "splice/detail/meta_utils.hpp"
#include "splice/detail/priority.hpp"
#include "splice/detail/registry.hpp"
#include "splice/detail/result.hpp"

/// @brief Declares a shared hook registry handle for the given class.
///
/// Must be placed in a header file. The `inline` keyword ensures ODR safety
/// across translation units.
///
/// Expands to:
/// @code
/// inline auto name = splice::ClassRegistry<Class>::shared();
/// @endcode
///
/// @param Class The class to create a registry for.
/// @param name  The variable name for the registry handle.
///
/// @par Example
/// @code
/// // gameworld_hooks.h
/// #include <splice/splice.hpp>
/// #include "gameworld.h"
///
/// SPLICE_REGISTRY(GameWorld, g_world);
///
/// // mymod.cpp
/// #include "gameworld_hooks.h"
///
/// void myModInit() {
///     g_world->inject<^^GameWorld::mineBlock, splice::InjectPoint::Head>(
///         [](GameWorld*, splice::CallbackInfo& ci,
///            Player* p, int, int y, int) {
///             if (y == 0) ci.cancelled = true;
///         });
/// }
/// @endcode
#define SPLICE_REGISTRY(Class, name) inline auto name = splice::ClassRegistry<Class>::shared()
