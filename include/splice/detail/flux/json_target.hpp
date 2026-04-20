#pragma once

#ifdef SPLICE_FLUX_ENABLE_JSON

#include <map>

#include <nlohmann/detail/conversions/to_json.hpp>
#include <nlohmann/json.hpp>
#include <string>

#include "splice/detail/flux/codec.hpp"

namespace splice::flux
{

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

  template<>
  struct serializer<JSON, MapObject<JSON>>
  {
    static JSON::IntermediaryType encode(MapObject<JSON> input)
    {
      return nlohmann::json(input);
      // return nlohmann::json();
    }

    static MapObject<JSON> decode(JSON::IntermediaryType ir)
    {
      // return {};
      return ir.get<MapObject<JSON>>();
    }
  };
};

#endif // SPLICE_FLUX_ENABLE_JSON