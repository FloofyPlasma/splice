#pragma once

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
#error "splice requires C++26 static reflection. Please use GCC 16+ and compile with -freflection."
#endif

#include "splice/detail/hook/hook_chain.hpp"
#include "splice/detail/hook/meta_utils.hpp"
#include "splice/detail/hook/registry.hpp"
#include "splice/detail/priority.hpp"
#include "splice/detail/result.hpp"
