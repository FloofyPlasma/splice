#pragma once

#include <map>
#include <meta>

#include <string>

#include "splice/detail/flux/annotations.hpp"
#include "splice/detail/flux/codec_base.hpp"
#include "splice/detail/meta_core.hpp"

namespace splice::detail
{
  template<typename ObjectType>
  consteval bool is_serializable_struct()
  {
    // TODO: add proper checks later
    return std::is_aggregate_v<ObjectType>;
    // // TODO: Check to see if there are any *public* fields rather than just returning true straight awy
    // if (splice::detail::has_annotation<serializable>(^^ObjectType)) {
    //    return true;
    // }
  }

  template<typename IntermediaryType, typename StructType>
  using MemberEncoder = IntermediaryType(StructType &);

  template<typename SerializationTarget, typename StructType, typename MemberType,
      MemberType StructType::*MemberPointer>
  SerializationTarget::IntermediaryType struct_member_encoder(StructType &object)
  {
    return flux::encode_intermediary<SerializationTarget>(object.*MemberPointer);
  }

  template<typename StructType>
  consteval std::size_t serializable_field_count()
  {
    // Check whether to mark fields as serializable by default or not
    bool serializable_by_default = splice::detail::has_annotation<flux::serializable>(^^StructType);

    // Collect only non-static data members
    constexpr static auto members = std::define_static_array(
        std::meta::nonstatic_data_members_of(^^StructType, std::meta::access_context::current()));

    std::size_t count = 0;
    template for (constexpr auto member: members)
    {
      // Require that the member either be annotated with splice::flux::field or its parent struct be
      // annotated with splice::flux::serializable, and that it is public and has an identifier
      if ((serializable_by_default || splice::detail::has_annotation<splice::flux::field>(member))
          && std::meta::is_public(member) && std::meta::has_identifier(member))
      {
        count++;
      }
    }
    return count;
  }

  template<typename SerializationTarget, typename StructType>
  struct member_encoder
  {
    std::string_view member_name;
    MemberEncoder<typename SerializationTarget::IntermediaryType, StructType> *encode;
  };

  template<typename SerializationTarget, typename StructType>
  consteval std::array<member_encoder<SerializationTarget, StructType>, serializable_field_count<StructType>()>
  get_member_encoders()
  {
    // Check whether to mark fields as serializable by default or not
    bool serializable_by_default = splice::detail::has_annotation<flux::serializable>(^^StructType);

    constexpr static auto members = std::define_static_array(
        std::meta::nonstatic_data_members_of(^^StructType, std::meta::access_context::current()));

    std::array<member_encoder<SerializationTarget, StructType>, serializable_field_count<StructType>()> member_encoders;
    std::size_t i = 0;
    template for (constexpr auto member: members)
    {
      // Require that the member either be annotated with splice::flux::field or its parent struct be
      // annotated with splice::flux::serializable, and that it is public and has an identifier
      if ((serializable_by_default || splice::detail::has_annotation<splice::flux::field>(member))
          && std::meta::is_public(member) && std::meta::has_identifier(member))
      {
        using MemberType = typename[:std::meta::type_of(member):];
        member_encoders[i++] = member_encoder<SerializationTarget, StructType> {
          .member_name = std::meta::identifier_of(member),
          .encode = &struct_member_encoder<SerializationTarget, StructType, MemberType, &[:member:]>,
        };
      }
    }

    return member_encoders;
  }

  template<typename SerializationTarget, typename StructType>
  SerializationTarget::IntermediaryType encode_struct(StructType object)
  {
    constexpr std::array member_encoders = get_member_encoders<SerializationTarget, StructType>();

    // Apply member encoders to a map
    flux::MapObject<SerializationTarget> result;
    for (auto encoder: member_encoders)
    {
      std::string key = static_cast<std::string>(encoder.member_name);
      result[key] = encoder.encode(object);
    }
    return flux::encode_intermediary<SerializationTarget>(result);
  }
};
