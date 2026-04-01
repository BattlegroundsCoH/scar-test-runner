# Plan: SCAR Test Framework for CoH3 Mods

## TL;DR
Build a cross-platform C executable (`scar-test`) that embeds Lua 5.3 to run BDD-style tests against Company of Heroes 3 SCAR mod scripts. Engine functions are manually mocked in C with simulated game state (entities, squads, players, groups). Each test file runs in an isolated `lua_State`. Unregistered SCAR function calls produce hard errors. Output is TAP-like colored terminal output. Built with CMake, CI via GitHub Actions.

## Project Structure

```
scar-test/
├── CMakeLists.txt                # Root build config
├── .github/workflows/ci.yml     # GitHub Actions CI
├── vendor/lua/                   # Vendored Lua 5.3 source
│   ├── CMakeLists.txt            # Builds liblua53 as static library
│   └── src/*.c, *.h              # Official Lua 5.3.6 source
├── src/
│   ├── main.c                    # CLI: arg parsing, test discovery, entry point
│   ├── test_runner.c/h           # Orchestrates test execution, collects results, TAP output
│   ├── scar_state.c/h            # lua_State setup: opens libs, registers all mocks, sets up import()
│   ├── game_state.c/h            # Central game state structs (Entity, Squad, Player, EGroup, SGroup)
│   ├── mock_entity.c/h           # Entity_* function implementations
│   ├── mock_squad.c/h            # Squad_* function implementations
│   ├── mock_player.c/h           # Player_* function implementations
│   ├── mock_egroup.c/h           # EGroup_* function implementations
│   ├── mock_sgroup.c/h           # SGroup_* function implementations
│   ├── mock_game.c/h             # Game_* lifecycle (Game_AddInit, Rule_Add*, etc.)
│   ├── mock_misc.c/h             # Stubs for World_*, Marker_*, misc utilities
│   └── test_api.c/h              # BDD API: describe/it/before_each/after_each + assertions + Mock_* helpers
├── scar/
│   └── test_helpers.scar         # Optional Lua-side test convenience functions
└── examples/
    ├── mod/assets/scar/
    │   └── winconditions/example_wc.scar
    └── test/test_example_wc.scar
```

## Steps

### Phase 1: Project Skeleton & Lua Embedding

1. **Create CMakeLists.txt root** — Define `scar-test` executable target, C11 standard, cross-platform flags. Add `vendor/lua` as subdirectory.
2. **Vendor Lua 5.3.6 source** — Download and place in `vendor/lua/src/`. Create `vendor/lua/CMakeLists.txt` to build a static `lua53` library (exclude `lua.c` and `luac.c` to avoid duplicate `main`).
3. **Implement `src/main.c`** — CLI argument parsing:
   - `scar-test [options] <file_or_dir>` 
   - `--scar-root <path>` — base path for resolving `import()` calls (default: current directory)
   - `--help` — usage info
   - If argument is a `.scar` file, run that single test. If a directory, discover all `test_*.scar` / `*_test.scar` files recursively.
4. **Implement `src/scar_state.c/h`** — Functions to create a configured `lua_State`:
   - `scar_state_new(const char* scar_root)` → `lua_State*`: creates state, opens standard libs, registers all mock modules, sets up `import()` and `dofile()` to resolve paths relative to `scar_root`
   - `scar_state_destroy(lua_State* L)`: closes state
   - Override global `import` function to resolve `import("X.scar")` → `<scar_root>/X.scar`
   - Store `scar_root` and `GameState*` as light userdata in Lua registry

### Phase 2: Game State & Core Mocks

5. **Implement `src/game_state.c/h`** — Central data structures:
   - `GameState` struct containing dynamic arrays of `MockEntity`, `MockSquad`, `MockPlayer`, `MockEGroup`, `MockSGroup`
   - `MockEntity`: id, health, health_max, position (x,y,z), player_owner_id, blueprint string, invulnerable flag, alive flag
   - `MockSquad`: id, entity_ids array, position, player_owner_id, blueprint, invulnerable
   - `MockPlayer`: id, team_id, resources (manpower, fuel, munitions), population caps
   - `MockEGroup`: name, entity_ids array
   - `MockSGroup`: name, squad_ids array
   - `game_state_create()` / `game_state_destroy()` / `game_state_reset()` for lifecycle
   - Lookup functions: `game_state_get_entity(id)`, etc.
   - `GameState` is stored in the Lua registry and retrievable via `game_state_from_lua(L)`

6. **Implement `src/mock_entity.c/h`** — Register Entity_* C functions into Lua:
   - `Entity_GetHealth(EntityID)` → Real
   - `Entity_SetHealth(EntityID, Real)`
   - `Entity_GetHealthMax(EntityID)` → Real
   - `Entity_GetPosition(EntityID)` → Position (Lua table {x,y,z})
   - `Entity_SetPosition(EntityID, Position)`
   - `Entity_GetPlayerOwner(EntityID)` → PlayerID
   - `Entity_SetPlayerOwner(EntityID, PlayerID)`
   - `Entity_SetInvulnerable(EntityID, Bool, Float)`
   - `Entity_IsAlive(EntityID)` → Boolean
   - Add more as needed. Each function retrieves `GameState` from registry, validates entity exists, reads/writes state.

7. **Implement `src/mock_squad.c/h`** — Register Squad_* C functions:
   - `Squad_GetHealth`, `Squad_SetHealth`, `Squad_GetPosition`, `Squad_GetPlayerOwner`, `Squad_SetPlayerOwner`, `Squad_Count` (entity count), `Squad_IsAlive`, etc.
   - Squad health could aggregate entity health

8. **Implement `src/mock_player.c/h`** — Register Player_* C functions:
   - `Player_GetResource(PlayerID, ResourceType)` → Real
   - `Player_SetResources(PlayerID, Table)`
   - `Player_OwnsEntity(PlayerID, EntityID)` → Boolean
   - `Player_OwnsSquad(PlayerID, SquadID)` → Boolean
   - `Player_SetMaxPopulation`, `Player_GetCurrentPopulation`

9. **Implement `src/mock_egroup.c/h`** — Register EGroup_* C functions:
   - `EGroup_Create(name)` → EGroupID
   - `EGroup_Add(EGroupID, EntityID)`
   - `EGroup_Count(EGroupID)` → Integer
   - `EGroup_Clear(EGroupID)`
   - `EGroup_ContainsEntity(EGroupID, EntityID)` → Boolean

10. **Implement `src/mock_sgroup.c/h`** — Register SGroup_* C functions (mirrors EGroup pattern):
    - `SGroup_Create`, `SGroup_Add`, `SGroup_Count`, `SGroup_Clear`, `SGroup_ContainsSquad`

11. **Implement `src/mock_game.c/h`** — Game lifecycle mocks:
    - `Game_AddInit(LuaFunction)` — stores function reference in GameState init list
    - `Rule_AddInterval(LuaFunction, interval)` — stores rule (could be simulated or just tracked)
    - `Rule_AddOneShot(LuaFunction, delay)` — stores rule
    - `Rule_Remove(LuaFunction)` — removes rule
    - `World_GetPlayerCount()` → Integer
    - `World_Pos(x, y, z)` → Position table

12. **Implement `src/mock_misc.c/h`** — Minimal stubs for commonly used utility functions:
    - `Loc_FormatText` → passthrough/format string
    - `scartype(val)` → type string
    - `print` → stdout print (already in Lua stdlib but may need override)
    - Constants registration: load key globals from merged_scardoc.json constants (SCARTYPE_*, resource types, team constants, etc.) — hardcoded subset in C

### Phase 3: Test API (BDD)

13. **Implement `src/test_api.c/h`** — BDD framework registered into Lua:
    - **Test structure functions** (registered as Lua globals):
      - `describe(name, fn)` — defines a test suite. Supports nesting.
      - `it(name, fn)` — defines a test case within a describe block.
      - `before_each(fn)` — hook called before each `it` in current `describe`.
      - `after_each(fn)` — hook called after each `it` in current `describe`.
    - **Assertion functions** (registered as Lua globals):
      - `assert_eq(expected, actual, msg?)` — deep equality
      - `assert_ne(a, b, msg?)`
      - `assert_true(val, msg?)`
      - `assert_false(val, msg?)`
      - `assert_nil(val, msg?)`
      - `assert_not_nil(val, msg?)`
      - `assert_error(fn, msg?)` — expects fn to raise
      - `assert_gt(a, b, msg?)`, `assert_gte(a, b, msg?)`, `assert_lt(a, b, msg?)`
    - **Mock state helpers** (registered as Lua globals):
      - `Mock_CreateEntity(id, blueprint, x, y, z)` — adds entity to GameState
      - `Mock_CreateSquad(id, blueprint, {entity_ids})` — adds squad to GameState
      - `Mock_CreatePlayer(id, team_id)` — adds player to GameState
      - `Mock_SetResources(player_id, manpower, fuel, munitions)` — sets player resources
      - `Mock_ResetState()` — clears entire GameState
    - **SCAR lifecycle helpers**:
      - `Scar_ForceInit()` — calls all functions registered via `Game_AddInit`
      - `Scar_ForceGameSetup()` — calls the global `OnGameSetup()` if it exists
    - **Execution model**: `describe`/`it` calls collect test definitions into a tree structure (C-side). After the test file finishes loading, the runner walks the tree, executing each `it` with its `before_each`/`after_each` hooks. Game state is reset between tests via `game_state_reset()`.

### Phase 4: Test Runner & Output

14. **Implement `src/test_runner.c/h`** — Orchestrates everything:
    - `test_runner_run_file(const char* filepath, const char* scar_root)` → TestResult
      1. Create fresh `lua_State` via `scar_state_new()`
      2. Create fresh `GameState` via `game_state_create()`
      3. Register test API into state
      4. `luaL_dofile(L, filepath)` — loads test definitions
      5. Execute collected test tree (walk describes, run before_each → it → after_each for each test)
      6. Collect pass/fail/error results
      7. Destroy state and game state
    - `test_runner_run_dir(const char* dirpath, const char* scar_root)` → aggregate results
      - Recursively find `test_*.scar` and `*_test.scar` files
      - Run each file via `test_runner_run_file`
    - **TAP-like output format**:
      ```
      # test/test_example_wc.scar
      ok 1 - My Wincondition > should initialize correctly
      not ok 2 - My Wincondition > should award victory when HQ destroyed
        --- Expected: true, Got: false
      ok 3 - My Wincondition > should track points correctly

      Results: 2/3 passed, 1 failed
      ```
    - Exit code: 0 if all pass, 1 if any fail

### Phase 5: CI & Examples

15. **Create `.github/workflows/ci.yml`** — GitHub Actions workflow:
    - Matrix: Ubuntu (gcc), macOS (clang), Windows (MSVC)
    - Steps: checkout, configure CMake, build, run example tests
16. **Create example mod + test** — Demonstrates full workflow:
    - `examples/mod/assets/scar/winconditions/example_wc.scar` — simple wincondition that tracks kills and awards victory
    - `examples/test/test_example_wc.scar` — BDD tests for the wincondition

## Relevant Files

- `CMakeLists.txt` — Root build config: define scar-test target, link lua53, set C11, add platform-specific flags
- `vendor/lua/CMakeLists.txt` — Build Lua 5.3 as static library
- `src/main.c` — CLI entry: parse args (`--scar-root`, file/dir), invoke test_runner
- `src/scar_state.c/h` — `scar_state_new()` creates configured lua_State with all mocks registered
- `src/game_state.c/h` — `GameState` struct and CRUD operations for all mock objects
- `src/mock_entity.c/h` — `Entity_*` C functions (get/set health, position, owner)
- `src/mock_squad.c/h` — `Squad_*` C functions
- `src/mock_player.c/h` — `Player_*` C functions (resources, ownership)
- `src/mock_egroup.c/h` — `EGroup_*` C functions (group management)
- `src/mock_sgroup.c/h` — `SGroup_*` C functions (group management)
- `src/mock_game.c/h` — `Game_AddInit`, `Rule_Add*`, `World_Pos` lifecycle functions
- `src/mock_misc.c/h` — Utility stubs (Loc_*, scartype, constants)
- `src/test_api.c/h` — BDD framework (describe/it/before_each/after_each) + assertions + Mock_* helpers
- `src/test_runner.c/h` — File/dir execution, test tree walking, TAP output, exit codes
- `merged_scardoc.json` — Reference for function signatures (not used at runtime)

## Verification

1. **Build on all 3 platforms** — `cmake --build .` succeeds on Linux/macOS/Windows (verified via CI)
2. **Run example tests** — `./scar-test --scar-root examples/mod/assets/scar examples/test/` produces correct TAP output with mix of pass/fail
3. **Test isolation** — Two test files that both set globals prove they don't leak between files
4. **Unregistered function error** — A test calling an unmocked function (e.g., `FOW_RevealAll()`) produces a clear Lua error
5. **Game state assertions** — Test that creates entities, sets health, and asserts `Entity_GetHealth` returns correct value
6. **Import resolution** — Test that uses `import("ScarUtil.scar")` resolves to `<scar_root>/ScarUtil.scar`
7. **Lifecycle simulation** — Test that calls `Game_AddInit(fn)`, then `Scar_ForceInit()`, confirms fn was called

## Decisions

- **Isolation model**: Separate `lua_State` per test FILE (not per `it` block). Game state is reset between `it` blocks via C-side `game_state_reset()` in the before_each lifecycle.
- **Unregistered functions**: Hard error. Unknown globals that are called will produce Lua's native "attempt to call a nil value" error. For better DX, a `__index` metamethod on `_G` could optionally warn about missing SCAR functions.
- **Import resolution**: `import("X.scar")` → `<scar_root>/X.scar`. The `--scar-root` CLI flag configures this.
- **No code generation**: All mock functions are manually implemented in C. This keeps the project simple and allows full control over mock behavior. Functions are added as needed.
- **Game state is C-side**: All mock state lives in C structs, not Lua tables. This ensures clean reset between tests and consistent behavior.
- **EntityID/SquadID/PlayerID are integers**: Lua side passes plain integers. C side looks them up in GameState arrays.
- **Position is a Lua table**: `{x=N, y=N, z=N}` — matches SCAR convention.

## Further Considerations

1. **ScarUtil.scar and other game .scar files**: The actual game ships .scar utility files (ScarUtil.scar, etc.) that mods commonly import. The test framework can load these if available, but they may call unmocked engine functions. Consider a `--scar-data <path>` flag pointing to extracted game .scar files, or provide stripped-down stubs. Recommend starting without this and adding as needed.
2. **Rule execution simulation**: `Rule_AddInterval` and `Rule_AddOneShot` register timed callbacks. A `Mock_AdvanceTime(seconds)` helper could simulate time passage and fire due rules. This is a natural extension but can be deferred to a later phase.
3. **Userdata vs integers for IDs**: Using plain integer IDs is simpler but doesn't prevent misuse (passing an EntityID where SquadID is expected). Full userdata with metatables would be type-safe but more complex. Recommend starting with integers and upgrading to userdata if type confusion becomes an issue.
