#pragma once

// NOTE: splice::wire depends on splice::hook at the implementation level.
// Wire reuses HookChain as its slot dispatch mechanism. Signals have no "original" function, slots are registered as
// HEAD hooks, and emit a dispatch call with original left null. Including wire.hpp always includes files from hook.
// This is intentional and should not be removed.

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
#error "splice requires C++26 static reflection. Please use GCC 16+ and compile with -freflection."
#endif

#include "splice/detail/priority.hpp"
#include "splice/detail/result.hpp"
#include "splice/detail/wire/annotations.hpp"
#include "splice/detail/wire/connectable.hpp"
#include "splice/detail/wire/meta_utils.hpp"
#include "splice/detail/wire/registry.hpp"
