# splice

A header-only C++26 hook and mixin library powered by static reflection.
Splice lets you register before, after, and return hooks on annotated class
methods at runtime, with zero overhead for unannotated code.

Inspired by Java's Fabric Mixin system, but designed from the ground up for
modern C++.

## Features

- **Hook registration**: Inject hooks at Head, Tail, or Return points on any
  annotated method.
- **CallbackInfo**: Cancel calls and override return values from within hooks.
- **Priority ordering**: Control hook execution order across independently
  registered hooks.
- **Focused modifiers**: `modify_arg` and `modify_return` for single-value
  overrides without writing a full hook.
- **Per-class shared registry**: Hooks are shared across all instances of a
  class.
- **Header-only**: Single include, no build step required for the library
  itself.

## Requirements

- GCC 16 or later
- `-freflection` compiler flag
- C++26 (`-std=c++26`)
- CMake 3.30 or later (for consuming via CMake)

> **Note:** C++26 static reflection is currently only supported by GCC 16 with
> the `-freflection` flag. Clang and MSVC support is not yet available.

## Quick Start

### 1. Annotate your class
```cpp
#include <splice/splice.hpp>

class GameWorld {
public:
    [[= splice::hook::hookable{}]] void mineBlock(Player* player, int x, int y, int z) {
        // original implementation
    }

    [[= splice::hook::hookable{}]] float calcDamage(Player* player, float amount) {
        return amount;
    }
};
```

### 2. Declare a registry header
```cpp
// gameworld_hooks.hpp
#pragma once
#include <splice/splice.hpp>
#include "gameworld.hpp"

SPLICE_HOOK_REGISTRY(GameWorld, g_world);
```

### 3. Register hooks
```cpp
#include "gameworld_hooks.hpp"

void myModInit() {
    // Cancel bedrock mining
    g_world->inject<^^GameWorld::mineBlock, splice::hook::InjectPoint::Head>(
        [](splice::detail::CallbackInfo& ci, GameWorld*, Player* p,
           int, int y, int) {
            if (y == 0) ci.cancelled = true;
        });

    // Double all damage
    g_world->modify_arg<^^GameWorld::calcDamage, 1>(
        [](float amount) -> float { return amount * 2.0f; });

    // Clamp final damage
    g_world->modify_return<^^GameWorld::calcDamage>(
        [](float result) -> float {
            return std::clamp(result, 0.0f, 20.0f);
        });
}
```

### 4. Dispatch through the registry
```cpp
GameWorld world;
Player steve{"Steve", 100};

g_world->dispatch<^^GameWorld::mineBlock>(&world, &steve, 10, 64, 5);
float dmg = g_world->dispatch<^^GameWorld::calcDamage>(&world, &steve, 15.0f);
```

## Installation

### FetchContent
```cmake
include(FetchContent)
FetchContent_Declare(
    splice
    GIT_REPOSITORY https://github.com/FloofyPlasma/splice
    GIT_TAG main
)
FetchContent_MakeAvailable(splice)

target_link_libraries(your_target PRIVATE splice::splice)
```

### find_package
```cmake
find_package(splice REQUIRED)
target_link_libraries(your_target PRIVATE splice::splice)
```

### Manual

Copy the `include/splice` directory into your project and add it to your
include path. You must also pass `-freflection` and `-std=c++26` to your
compiler.

## API Overview

**Hook signatures:**
```cpp
// Void method
void(splice::detail::CallbackInfo& ci, T* self, Params...)

// Non-void method
void(splice::detail::CallbackInfoReturnable<Ret>& ci, T* self, Params...)
```

**Inject points:**

| Point | Runs | Can cancel | Has return value |
|-------|------|------------|-----------------|
| `splice::hook::InjectPoint::Head`   | Before original | Yes | No  |
| `splice::hook::InjectPoint::Tail`   | After original  | No  | No  |
| `splice::hook::InjectPoint::Return` | At every return | No  | Yes |

**Priority constants** (lower value runs first):
```cpp
splice::hook::Priority::Highest  // 0
splice::hook::Priority::High     // 250
splice::hook::Priority::Normal   // 500  (default)
splice::hook::Priority::Low      // 750
splice::hook::Priority::Lowest   // 1000
```

Arithmetic is supported: `splice::hook::Priority::Normal + 1`.

## Building Tests
```bash
git clone https://github.com/FloofyPlasma/splice
cd splice
cmake -B build -DSPLICE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## License

MIT, see [LICENSE](LICENSE) for details.