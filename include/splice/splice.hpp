#pragma once

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
#error "splice requires C++26 static reflection support. Please use GCC 16 or later and compile with -freflection."
#endif

#include "splice/hook.hpp"
#include "splice/wire.hpp"