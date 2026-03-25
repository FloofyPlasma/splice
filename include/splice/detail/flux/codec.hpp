#pragma once

#include <concepts>
#include <map>
#include <meta>

#include <nlohmann/detail/conversions/to_json.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <type_traits>

#include "splice/detail/meta_core.hpp"

namespace splice::flux
{
  struct serializable
  {
  };
  struct field
  {
    const char *name;
    bool replicate;
  };

  struct JSON
  {
    using IntermediaryType = nlohmann::json;
    using InputOutputType = std::string;

    static IntermediaryType decode_init(InputOutputType input) { return nlohmann::json::parse(input); }

    static InputOutputType encode_finish(IntermediaryType ir) { return ir.dump(); }

    struct State
    {
    };
  };

  template<typename SerializationTarget, typename InputOutputType>
  struct serializer
  {
  };

  template<>
  struct serializer<JSON, float>
  {
    static JSON::IntermediaryType encode(float input) { return nlohmann::json(input); }

    static float decode(JSON::IntermediaryType ir)
    {
      // TODO: Add some checking
      return ir.get<float>();
    }
  };

  template <typename SerializationTarget>
  using MapObject = std::map<std::string, typename SerializationTarget::IntermediaryType>;

  template<>
  struct serializer<JSON, MapObject<JSON>>
  {
    static JSON::IntermediaryType encode(MapObject<JSON> input) {
      return nlohmann::json(input);
      // return nlohmann::json();
    }

    static MapObject<JSON> decode(JSON::IntermediaryType ir)
    {
      // return {};
      return ir.get<MapObject<JSON>>();
    }
  };

  template<typename SerializationTarget, typename ObjectType>
  concept has_serializer = requires(ObjectType o, SerializationTarget::IntermediaryType ir) {
    {
      serializer<SerializationTarget, ObjectType>::encode(o)
    } -> std::same_as<typename SerializationTarget::IntermediaryType>;
    { serializer<SerializationTarget, ObjectType>::decode(o) } -> std::same_as<ObjectType>;
  };

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

  // Fwd declaring this
  template<typename SerializationTarget, typename ObjectType>
  SerializationTarget::IntermediaryType encode_intermediary(ObjectType object);

  template<typename IntermediaryType, typename StructType>
  using MemberEncoder = IntermediaryType(StructType &);

  template<typename SerializationTarget, typename StructType, typename MemberType,
      MemberType StructType::*MemberPointer>
  SerializationTarget::IntermediaryType struct_member_encoder(StructType &object)
  {
    return encode_intermediary<SerializationTarget>(object.*MemberPointer);
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
          if (splice::detail::has_annotation<field>(member))
          {
            count++;
          }
        }
        return count;
      }();

      std::array<std::pair<std::string_view, MemberEncoder<typename SerializationTarget::IntermediaryType, StructType> *>, count> member_encoders;
      std::size_t i = 0;
      template for (constexpr auto member: all_members)
      {
        if (splice::detail::has_annotation<field>(member))
        {
          using MemberType = typename[:std::meta::type_of(member):];
          member_encoders[i++] = std::make_pair(std::meta::identifier_of(member), &struct_member_encoder<SerializationTarget, StructType, MemberType, &[:member:]>);
        }
      }

      return member_encoders;
    }();

    std::map<std::string, typename SerializationTarget::IntermediaryType> result;
    for (auto encoder_pair: member_encoders)
    {
      std::string key = static_cast<std::string>(std::get<0>(encoder_pair));
      result[key] = std::get<1>(encoder_pair)(object);
    }
    return encode_intermediary<SerializationTarget>(result);
  }

  template<typename SerializationTarget, typename ObjectType>
  SerializationTarget::IntermediaryType encode_intermediary(ObjectType object)
  {
    using Serializer = serializer<SerializationTarget, ObjectType>;

    // If serializer exists, use that
    if constexpr (has_serializer<SerializationTarget, ObjectType>)
    {
      return Serializer::encode(object);
    } else if constexpr (is_serializable_struct<ObjectType>())
    {
      return encode_struct<SerializationTarget>(object);
    } else
    {
      static_assert(false, "No valid serializer found for type");
      // throw "No valid serializer found for type";
    }
  }

  template<typename SerializationTarget, typename ObjectType>
  SerializationTarget::InputOutputType encode(ObjectType object)
  {
    typename SerializationTarget::IntermediaryType result = encode_intermediary<SerializationTarget>(object);
    return SerializationTarget::encode_finish(result);
  }
};
