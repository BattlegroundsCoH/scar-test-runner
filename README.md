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
  --scar-data <path>   Extra fallback path for resolving imports (optional)
  --help               Show help
```

`import()` resolves files from `--scar-root` first. Common engine scripts (`scarutil.scar`, `core.scar`, `game_modifiers.scar`, etc.) have built-in stubs — their functions are already provided by the C mock layer, so no copies of copyrighted game files are needed. If you need to import additional `.scar` files that aren't in your mod, you can optionally use `--scar-data` as a secondary search path.

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

1. **`import()`** loads your production `.scar` files (resolved from `--scar-root`; engine scripts like `scarutil.scar` are handled by built-in stubs).
2. **`describe` / `it` / `before_each` / `after_each`** register the test tree.
3. **`Mock_Create*`** functions set up game state for each test.
4. **`Scar_ForceInit()`** fires all `Game_AddInit` callbacks (just like the engine does on match start).
5. Each `it` block runs in an isolated game state — `before_each` resets between tests.

### Assertions

| Function | Description |
|---|---|
| `assert_eq(expected, actual [, msg])` | Deep equality (`==`); uses `__eq` metamethod for userdata types |
| `assert_ne(a, b [, msg])` | Not equal |
| `assert_true(val [, msg])` | Truthy |
| `assert_false(val [, msg])` | Falsy |
| `assert_nil(val [, msg])` | Is nil |
| `assert_not_nil(val [, msg])` | Not nil |
| `assert_lt(a, b [, msg])` | `a < b` |
| `assert_lte(a, b [, msg])` | `a <= b` |
| `assert_gt(a, b [, msg])` | `a > b` |
| `assert_gte(a, b [, msg])` | `a >= b` |
| `assert_error(fn [, msg])` | `fn()` must raise an error; returns the caught error message string |
| `assert_called(spy, n [, msg])` | Spy was called exactly `n` times |
| `assert_not_called(spy [, msg])` | Spy was never called |
| `assert_called_with(spy, arg1, ...)` | At least one spy call matched the given arguments; on failure, lists all actual calls |

### Resource loading

| Function | Description |
|---|---|
| `resource(path)` | Load a `.scar` file relative to the current test file's directory. Errors propagate normally. |

Useful for splitting large test suites into helper files stored alongside the test file:

```lua
-- test file at examples/test/test_foo.scar
resource("test_resources/helpers.scar") -- loads examples/test/test_resources/helpers.scar
```

### Spies

`spy([fn])` creates a callable object that records every call. Optionally wraps an existing function (forwarding calls and returning its results).

```lua
-- Spy on a function
local callback = spy(function(x) return x * 2 end)
callback(5)
callback(10)
assert_called(callback, 2)
assert_called_with(callback, 5)

-- No-op spy
local s = spy()
s("hello", 123)
assert_called(s, 1)
assert_called_with(s, "hello", 123)

-- Spy on a global (automatically unwraps if already a spy)
print = spy(print)
print("test output")
assert_called(print, 1)
```

### Mock helpers

| Function | Returns | Description |
|---|---|---|
| `Mock_CreateEntity(id, bp, x, y, z [, health_max])` | `Entity` | Create a mock entity |
| `Mock_CreateSquad(id, bp [, {entity, ...}])` | `Squad` | Create a mock squad (optional entity list) |
| `Mock_CreatePlayer(id [, team_id, name, race, is_human])` | `Player` | Create a mock player |
| `Mock_SetEntityOwner(entity, player)` | — | Assign entity to player |
| `Mock_SetSquadOwner(squad, player)` | — | Assign squad to player |
| `Mock_SetResources(player, mp, fuel, mun)` | — | Set player resources |
| `Mock_SetPlayerStartingPosition(player, x, y, z)` | — | Set player spawn position |
| `Mock_CreateTerritory(sector_id [, owner_id, x, y, z])` | — | Create a territory sector |
| `Mock_CreateStrategyPoint(entity, sector_id [, is_vp])` | — | Link an entity to a sector as a strategy point |
| `Mock_FireGlobalEvent(event_type, ...)` | — | Dispatch a global event to registered handlers |
| `Mock_ResetState()` | — | Clear all entities, squads, players, groups |
| `Mock_AdvanceTime(seconds)` | — | Advance game clock and fire due rules |
| `Scar_ForceInit()` | — | Fire all `Game_AddInit` callbacks |
| `Scar_ForceStart()` | — | Run `OnGameSetup` → init funcs → `OnInit` delegates |
| `Scar_ForceGameSetup()` | — | Run `OnGameSetup` only |

All `Entity`, `Squad`, `Player`, `EGroup`, and `SGroup` values are typed userdata with metatables — passing the wrong type raises an error, just like the real engine.

For the complete list of all mocked SCAR functions, constants, and test framework APIs, see [API.md](API.md).

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
            "https://github.com/BattlegroundsCoH/scar-test-runner/releases/latest/download/scar-test-linux-amd64.tar.gz" \
            -o scar-test.tar.gz
          tar xzf scar-test.tar.gz
          chmod +x scar-test

      - name: Run SCAR tests
        run: ./scar-test --scar-root my_mod/assets/scar tests/
```

Common engine imports like `scarutil.scar` and `core.scar` are handled automatically by built-in stubs — no game files needed.

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
