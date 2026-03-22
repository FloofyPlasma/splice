#pragma once

#include <array>
#include <meta>

namespace splice::detail
{

  /// @brief Returns `true` if @p m is a non-special member function, i.e. not a constructor, destructor, or operator
  /// overload, and has a plain identifier name.
  ///
  /// Used as a base predicate by all module-specific member scanners.
  consteval bool is_normal_function(std::meta::info m)
  {
    return std::meta::is_function(m) && std::meta::has_identifier(m) && !std::meta::is_constructor(m)
           && !std::meta::is_destructor(m) && !std::meta::is_operator_function(m);
  }

  /// @brief Returns `true` if the reflected member @p m has at least one annotation of type @p Annotation.
  ///
  /// @tparam Annotation The annotation type to search for.
  template<typename Annotation>
  consteval bool has_annotation(std::meta::info m)
  {
    return !std::meta::annotations_of_with_type(m, ^^Annotation).empty();
  }

  /// @brief Returns the number of reflected members of @p T that satisfy the compile-time predicate @p Pred.
  ///
  /// Companion to `member_array`, same predicate, returns a count instead of an array. Useful in `if constexpr` and
  /// `static_assert` contexts where the array itself isn't needed.
  ///
  /// @tparam T The class to scan.
  /// @tparam Pred A consteval predicate: `bool(std::meta::info)`
  template<typename T, auto Pred>
  consteval std::size_t member_count()
  {
    std::size_t n = 0;
    for (auto m: std::meta::members_of(^^T, std::meta::access_context::unchecked()))
      if (Pred(m))
        n++;
    return n;
  }

  /// @brief Returns a `std::array` of reflected members of @p T that satisfy the compile-time predicate @p Pred, in
  /// declaration order.
  ///
  /// @p Pred must be a `consteval`-compatible callable with signature `bool(std::meta::info m)`. Lambdas and function
  /// pointers both work as non-template arguments in C++26.
  ///
  /// This is the canonical two-pass count-then-fill pattern used by all Splice module member scanners. Each module's
  /// `meta_utils.hpp` composes this with its own predicates rather than reimplementing the loop.
  ///
  /// @tparam T The class to scan.
  /// @tparam Pred A consteval predicate `bool(std::meta::info)`.
  ///
  /// @par Example
  /// @code
  /// // Collect all non-static data members
  /// consteval auto fields = member_array<MyStruct,
  ///   [](std::meta::info m) {
  ///     return std::meta::is_nonstatic_data_member(m);
  ///   }<>();
  /// @endcode
  template<typename T, auto Pred>
  consteval auto member_array()
  {
    constexpr std::size_t count = member_count<T, Pred>();

    std::array<std::meta::info, count> result { };
    std::size_t i = 0;
    for (auto m: std::meta::members_of(^^T, std::meta::access_context::unchecked()))
      if (Pred(m))
        result[i++] = m;
    return result;
  }
} // namespace splice::detail
