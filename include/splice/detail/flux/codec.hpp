#pragma once

#include <concepts>

#include "splice/detail/flux/struct_encode.hpp"

namespace splice::flux
{
  /// @brief Specialized template type used to define encoders/decoders for a particular C++ type
  ///
  /// Implementors should add a static function `encode` which accepts the desired type and returns the
  /// serialization target's IntermediaryType, and a static `decode` function which accepts the IntermediaryType
  /// and returns the desired type
  ///
  /// @par Example
  /// @code
  /// struct serializer<JSON, float>
  /// {
  ///   static JSON::IntermediaryType encode(float input) { return nlohmann::json(input); }
  ///
  ///   static float decode(JSON::IntermediaryType ir)
  ///   {
  ///     return ir.get<float>();
  ///   }
  /// };
  /// @endcode
  template<typename SerializationTarget, typename InputOutputType>
  struct serializer
  {
  };

  namespace detail {
    /// @brief Checks that a specific type has a serializer specialization that is well-defined
    ///
    /// @par Example
    /// @code
    /// has_serializer<JSON, float>
    /// @endcode
    template<typename SerializationTarget, typename ObjectType>
    concept has_serializer = requires(ObjectType o, SerializationTarget::IntermediaryType ir) {
      {
        serializer<SerializationTarget, ObjectType>::encode(o)
      } -> std::same_as<typename SerializationTarget::IntermediaryType>;
      { serializer<SerializationTarget, ObjectType>::decode(o) } -> std::same_as<ObjectType>;
    };
  };

  template<typename SerializationTarget, typename ObjectType>
  SerializationTarget::IntermediaryType encode_intermediary(ObjectType object)
  {
    using Serializer = serializer<SerializationTarget, ObjectType>;

    // If serializer exists, use that
    if constexpr (detail::has_serializer<SerializationTarget, ObjectType>)
    {
      return Serializer::encode(object);
    } else if constexpr (detail::is_serializable_struct<ObjectType>())
    {
      return detail::encode_struct<SerializationTarget>(object);
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
