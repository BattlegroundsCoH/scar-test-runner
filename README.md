# scar-test

A cross-platform BDD testing framework for [Company of Heroes 3](https://www.companyofheroes.com/) SCAR scripts. It embeds Lua 5.3, mocks the COH3 SCAR API, and runs `.scar` test files with colored TAP-style output.

## Quick start

Download a prebuilt binary from [Releases](../../releases), or build from source:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run tests:

```bash
# Single file
./scar-test path/to/test_foo.scar

# Directory (discovers test_*.scar and *_test.scar files)
./scar-test --scar-root my_mod/assets/scar tests/
```

## CLI

```
scar-test [options] <file_or_directory>

Options:
  --scar-root <path>   Base path for resolving import() calls (default: cwd)
  --help               Show help
```

## Writing tests

Tests use a BDD-style API inspired by Busted / Mocha.

```lua
import("winconditions/annihilation.scar")

describe("Annihilation", function()

    local player, entity

    before_each(function()
        player = Mock_CreatePlayer(1, 1)
        entity = Mock_CreateEntity(101, "riflemen", 0, 0, 0, 100)
        Mock_SetEntityOwner(entity, player)
        Scar_ForceInit()
    end)

    it("should start alive", function()
        assert_true(Entity_IsAlive(entity))
    end)

    it("should die at 0 health", function()
        Entity_SetHealth(entity, 0)
        assert_false(Entity_IsAlive(entity))
    end)
end)
```

### Test lifecycle

1. **`import()`** loads your production `.scar` files (resolved relative to `--scar-root`).
2. **`describe` / `it` / `before_each` / `after_each`** register the test tree.
3. **`Mock_Create*`** functions set up game state for each test.
4. **`Scar_ForceInit()`** fires all `Game_AddInit` callbacks (just like the engine does on match start).
5. Each `it` block runs in an isolated game state — `before_each` resets between tests.

### Assertions

| Function | Description |
|---|---|
| `assert_eq(expected, actual [, msg])` | Deep equality (`==`) |
| `assert_ne(a, b [, msg])` | Not equal |
| `assert_true(val [, msg])` | Truthy |
| `assert_false(val [, msg])` | Falsy |
| `assert_nil(val [, msg])` | Is nil |
| `assert_not_nil(val [, msg])` | Not nil |
| `assert_lt(a, b [, msg])` | `a < b` |
| `assert_le(a, b [, msg])` | `a <= b` |
| `assert_gt(a, b [, msg])` | `a > b` |
| `assert_ge(a, b [, msg])` | `a >= b` |
| `assert_error(fn [, msg])` | `fn()` must raise an error |

### Mock helpers

| Function | Returns | Description |
|---|---|---|
| `Mock_CreateEntity(id, bp, x, y, z [, health_max])` | `Entity` | Create a mock entity |
| `Mock_CreateSquad(id, bp [, {entity, ...}])` | `Squad` | Create a mock squad (optional entity list) |
| `Mock_CreatePlayer(id [, team_id])` | `Player` | Create a mock player |
| `Mock_SetEntityOwner(entity, player)` | — | Assign entity to player |
| `Mock_SetSquadOwner(squad, player)` | — | Assign squad to player |
| `Mock_SetResources(player, mp, fuel, mun)` | — | Set player resources |
| `Mock_ResetState()` | — | Clear all entities, squads, players, groups |
| `Mock_AdvanceTime(seconds)` | — | Advance game clock and fire due rules |

All `Entity`, `Squad`, `Player`, `EGroup`, and `SGroup` values are typed userdata with metatables — passing the wrong type raises an error, just like the real engine.

## Mocked SCAR API

The following SCAR modules are implemented as mocks:

| Module | Functions |
|---|---|
| **Entity** | `GetHealth`, `SetHealth`, `GetHealthMax`, `GetHealthPercentage`, `GetPosition`, `SetPosition`, `GetPlayerOwner`, `SetPlayerOwner`, `IsAlive`, `IsInvulnerable`, `SetInvulnerable`, `GetBlueprint`, `Kill`, `WarpToPos`, `GetGameID`, `FromWorldID`, `GetSquad` |
| **Squad** | `GetHealth`, `GetHealthMax`, `GetHealthPercentage`, `GetPosition`, `SetPosition`, `GetPlayerOwner`, `SetPlayerOwner`, `Count`, `GetBlueprint`, `GetGameID`, `FromWorldID` |
| **Player** | `GetID`, `GetTeam`, `GetResource`, `SetResource`, `OwnsEntity`, `OwnsSquad`, `FromWorldID` |
| **EGroup** | `Create`, `Add`, `Count`, `Clear`, `ContainsEntity`, `GetEntityAt` |
| **SGroup** | `Create`, `Add`, `Count`, `Clear`, `ContainsSquad`, `GetSquadAt` |
| **Game** | `AddInit`, `GetLocalPlayer`, `World_GetPlayerCount`, `World_GetPlayerAt`, `World_GetGameTime` |
| **Rules** | `Rule_AddInterval`, `Rule_AddOneShot`, `Rule_Exists`, `Rule_Remove` |
| **Misc** | `Loc_FormatText`, `scartype`, resource/scartype/game constants, no-op stubs for Sound/UI/Camera/Cmd |

Functions not listed above will raise a hard error if called, so you know immediately when your script uses something that isn't mocked yet.

## Using in GitHub Actions

Download a release binary and run your mod's tests in CI:

```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Download scar-test
        run: |
          curl -fsSL \
            "https://github.com/BattlegroundsCoH/scar-test/releases/latest/download/scar-test-linux-amd64.tar.gz" \
            -o scar-test.tar.gz
          tar xzf scar-test.tar.gz
          chmod +x scar-test

      - name: Run SCAR tests
        run: ./scar-test --scar-root my_mod/assets/scar tests/
```

## Project structure

```
src/
  main.c            CLI entry point
  game_state.{c,h}  Central mock game state and typed userdata
  scar_state.{c,h}  Lua state factory (registers all mocks)
  test_api.{c,h}    BDD framework, assertions, Mock_* helpers
  test_runner.{c,h}  File discovery and TAP output
  mock_entity.c      Entity_* mocks
  mock_squad.c       Squad_* mocks
  mock_player.c      Player_* mocks
  mock_egroup.c      EGroup_* mocks
  mock_sgroup.c      SGroup_* mocks
  mock_game.c        Game/World/Rule mocks
  mock_misc.c        Constants, stubs, no-ops
vendor/lua/          Lua 5.3.6 source (vendored)
examples/
  mod/               Example CoH3 mod with a wincondition
  test/              Example test files
```

## License

Lua 5.3 is licensed under the [MIT License](https://www.lua.org/license.html).
