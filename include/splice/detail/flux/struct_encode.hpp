#pragma once

#include <meta>

#include <optional>

#include "splice/detail/flux/annotations.hpp"
#include "splice/detail/flux/codec_base.hpp"
#include "splice/detail/meta_core.hpp"

namespace splice::detail
{
  consteval bool is_serializable_field(bool serializable_by_default, std::meta::info member) {
    return (serializable_by_default || splice::detail::has_annotation<splice::flux::field>(member))
          && std::meta::is_public(member) && std::meta::has_identifier(member);
  }

  template<typename StructType>
  consteval bool is_serializable_struct()
  {
    // Check whether to mark fields as serializable by default or not
    constexpr bool serializable_by_default = splice::detail::has_annotation<flux::serializable>(^^StructType);

    return member_count<StructType, [](std::meta::info member) {
      return is_serializable_field(serializable_by_default, member);
    }>() > 0;
  }

  template <std::meta::info member>
  consteval std::optional<flux::field> get_field_annotation() {
    if (has_annotation<flux::field>(member)) {
      return std::meta::extract<flux::field>(std::meta::annotations_of_with_type(member, ^^flux::field)[0]);
    } else {
      return std::nullopt;
    }
  }

  template<typename SerializationTarget, typename StructType>
  SerializationTarget::IntermediaryType encode_struct(StructType object)
  {
    flux::MapObject<SerializationTarget> result;

    // Check whether to mark fields as serializable by default or not
    constexpr bool serializable_by_default = splice::detail::has_annotation<flux::serializable>(^^StructType);

    // Iterate over members
    template for (constexpr auto member : [:std::meta::reflect_constant_array(std::meta::nonstatic_data_members_of(^^StructType, std::meta::access_context::current())):]) {
      // Require that the member either be annotated with splice::flux::field or its parent struct be
      // annotated with splice::flux::serializable, and that it is public and has an identifier
      if (is_serializable_field(serializable_by_default, member))
      {
        // TODO: GCC doesn't support strings in fields (yet), so this is a TBI
        // std::optional<flux::field> field_annotation = get_field_annotation<member>();

        constexpr auto member_pointer = &[:member:];
        std::string key = static_cast<std::string>(std::meta::identifier_of(member));
        auto value = object.*member_pointer;

        result[key] = flux::encode_intermediary<SerializationTarget>(value);
      }
    }

    return flux::encode_intermediary<SerializationTarget>(result);
  }
};
