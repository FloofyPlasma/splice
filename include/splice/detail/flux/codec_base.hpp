#pragma once

#include <map>
#include <string>

namespace splice::flux
{
  template<typename SerializationTarget, typename ObjectType>
  SerializationTarget::IntermediaryType encode_intermediary(ObjectType object);

  /// @brief Type representing a key-value structure for a given serialization target
  ///
  /// @par Example
  /// @code
  /// splice::flux::MapObject<splice::flux::JSON>
  /// @endcode
  template <typename SerializationTarget>
  using MapObject = std::map<std::string, typename SerializationTarget::IntermediaryType>;
};