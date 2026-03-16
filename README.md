# splice

A header-only C++26 hook and mixin library powered by static reflection.
Splice lets you register before, after and return hooks on annotated class
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
- CMake 3.25 or later (for consuming via CMake)

> **Note:** C++26 static reflection is currently only supported by GCC 16 with
> the `-freflection` flag. Clang and MSVC support is not yet available.

## Quick Start

### 1. Annotate your class
```cpp
#include <splice/splice.hpp>

class GameWorld {
public:
    [[= splice::hookable{}]] void mineBlock(Player* player, int x, int y, int z) {
        // original implementation
    }

    [[= splice::hookable{}]] float calcDamage(Player* player, float amount) {
        return amount;
    }
};
```

### 2. Declare a registry header
```cpp
// gameworld_hooks.h
#pragma once
#include <splice/splice.hpp>
#include "gameworld.h"

SPLICE_REGISTRY(GameWorld, g_world);
```

### 3. Register hooks
```cpp
#include "gameworld_hooks.h"

void myModInit() {
    // Cancel bedrock mining
    g_world->inject<^^GameWorld::mineBlock, splice::InjectPoint::Head>(
        [](splice::CallbackInfo& ci, GameWorld*, Player* p,
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
// Void method, CI has no return_value field
void(splice::CallbackInfo& ci, T* self, Params...)

// Non-void method, CI carries the return value
void(splice::CallbackInfoReturnable<Ret>& ci, T* self, Params...)
```

**Inject points:**

| Point | Runs | Can cancel | Has return value |
|-------|------|------------|-----------------|
| `Head` | Before original | Yes | No |
| `Tail` | After original | No | No |
| `Return` | At every return path | No | Yes |

**Priority constants** (lower runs first):
```cpp
splice::Priority::Highest  // 0
splice::Priority::High     // 250
splice::Priority::Normal   // 500  (default)
splice::Priority::Low      // 750
splice::Priority::Lowest   // 1000
```

Arithmetic is supported: `Priority::Normal + 1`.

## Building Tests
```bash
git clone https://github.com/FloofyPlasma/splice
cd splice
cmake -B build -DSPLICE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## Current Status

**Working:**
- Head, Tail, and Return inject points
- CallbackInfo cancellation and return value override
- Priority-ordered hook execution
- `modify_arg` and `modify_return` focused hooks
- Per-class shared registry with `weak_ptr` lifetime management
- `SPLICE_REGISTRY` convenience macro

**Limitations:**
- Requires GCC 16 with `-freflection`, no other compiler support yet
- No CI compilation until GCC 16 is available on GitHub Actions runners
- C++26 reflection is still an evolving standard; the API may need to
  adapt as the spec is finalised

**Future Goals:**
- Conflict detection and reporting
- Broader compiler support as reflection standardises

## License

MIT, see [LICENSE](LICENSE) for details.