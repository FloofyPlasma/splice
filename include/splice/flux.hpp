#pragma once

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
#error "splice requires C++26 static reflection. Please use GCC 16+ and compile with -freflection."
#endif

#include "splice/detail/flux/codec.hpp"
#include "splice/detail/flux/json_target.hpp"
