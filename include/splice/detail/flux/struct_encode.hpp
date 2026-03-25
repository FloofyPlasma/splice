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

  template<typename SerializationTarget, typename StructType>
  SerializationTarget::IntermediaryType encode_struct(StructType object)
  {
    constexpr std::array member_encoders = []
    {
      constexpr static auto all_members = std::define_static_array(
          std::meta::nonstatic_data_members_of(^^StructType, std::meta::access_context::current()));
      constexpr static std::size_t count = []()
      {
        std::size_t count = 0;
        template for (constexpr auto member: all_members)
        {
          if (splice::detail::has_annotation<splice::flux::field>(member))
          {
            count++;
          }
        }
        return count;
      }();

      std::array<
          std::pair<std::string_view, MemberEncoder<typename SerializationTarget::IntermediaryType, StructType> *>,
          count>
          member_encoders;
      std::size_t i = 0;
      template for (constexpr auto member: all_members)
      {
        if (splice::detail::has_annotation<splice::flux::field>(member))
        {
          using MemberType = typename[:std::meta::type_of(member):];
          member_encoders[i++] = std::make_pair(std::meta::identifier_of(member),
              &struct_member_encoder<SerializationTarget, StructType, MemberType, &[:member:]>);
        }
      }

      return member_encoders;
    }();

    flux::MapObject<SerializationTarget> result;
    for (auto encoder_pair: member_encoders)
    {
      std::string key = static_cast<std::string>(std::get<0>(encoder_pair));
      result[key] = std::get<1>(encoder_pair)(object);
    }
    return flux::encode_intermediary<SerializationTarget>(result);
  }
};
