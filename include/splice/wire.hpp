#pragma once

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
#error "splice requires C++26 static reflection. Please use GCC 16+ and compile with -freflection."
#endif

#include "splice/detail/priority.hpp"
#include "splice/detail/result.hpp"
#include "splice/detail/wire/annotations.hpp"
#include "splice/detail/wire/connectable.hpp"
#include "splice/detail/wire/meta_utils.hpp"
#include "splice/detail/wire/registry.hpp"
