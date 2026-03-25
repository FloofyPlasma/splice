#pragma once

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
}
