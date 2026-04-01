# scar-test API Reference

Complete reference for all functions and constants provided by the scar-test framework.

---

## Test Framework

### BDD Structure

| Function | Description |
|---|---|
| `describe(name, fn)` | Define a test group. Can be nested. |
| `it(name, fn)` | Define a single test case inside a `describe` block. |
| `before_each(fn)` | Run `fn` before each `it` in the current `describe` (and nested children). |
| `after_each(fn)` | Run `fn` after each `it` in the current `describe` (and nested children). |

### Assertions

| Function | Description |
|---|---|
| `assert_eq(expected, actual [, msg])` | Values are equal (`==`). Compares by type then value. Uses `__eq` metamethod for `Entity`, `Squad`, `Player`, `EGroup`, and `SGroup` userdata — equality is by id (integers) or name (groups). |
| `assert_ne(a, b [, msg])` | Values are not equal. Also uses `__eq` for userdata types. |
| `assert_true(val [, msg])` | Value is truthy (not `false` or `nil`). |
| `assert_false(val [, msg])` | Value is falsy (`false` or `nil`). |
| `assert_nil(val [, msg])` | Value is `nil`. |
| `assert_not_nil(val [, msg])` | Value is not `nil`. |
| `assert_gt(a, b [, msg])` | `a > b` (numbers only). |
| `assert_gte(a, b [, msg])` | `a >= b` (numbers only). |
| `assert_lt(a, b [, msg])` | `a < b` (numbers only). |
| `assert_lte(a, b [, msg])` | `a <= b` (numbers only). |
| `assert_error(fn [, msg])` | Calling `fn()` must raise an error. Returns the caught error message string so callers can inspect it. |
| `assert_called(spy, n [, msg])` | Spy was called exactly `n` times. |
| `assert_not_called(spy [, msg])` | Spy was never called. |
| `assert_called_with(spy, arg1, ...)` | At least one spy call matched the exact arguments. Uses `__eq` for userdata. On failure, the error message lists every recorded call with its arguments. |

### Resource Loading

| Function | Description |
|---|---|
| `resource(path)` | Load and execute a `.scar` file at `path`, resolved relative to the currently executing test file's directory. Any error in the loaded file propagates as a normal Lua error. |

This is distinct from `import()`, which resolves paths against `--scar-root`. Use `resource()` for test-only helpers and fixtures that live alongside your test files.

### Spies

| Function | Returns | Description |
|---|---|---|
| `spy([fn])` | spy object | Create a callable spy. Optionally wraps `fn`, forwarding calls and returning its results. If `fn` is already a spy, unwraps to the original function to prevent nesting. |

Spy objects record every call with arguments. Use `assert_called`, `assert_not_called`, and `assert_called_with` to verify behavior.

### Mock Helpers

| Function | Returns | Description |
|---|---|---|
| `Mock_CreateEntity(id, bp, x, y, z [, health_max])` | `Entity` | Create a mock entity with optional max health. |
| `Mock_CreateSquad(id, bp [, {entity, ...}])` | `Squad` | Create a mock squad with optional entity list. |
| `Mock_CreatePlayer(id [, team_id, name, race, is_human])` | `Player` | Create a mock player. |
| `Mock_SetEntityOwner(entity, player)` | — | Assign entity ownership to a player. |
| `Mock_SetSquadOwner(squad, player)` | — | Assign squad ownership to a player. |
| `Mock_SetResources(player, mp, fuel, mun)` | — | Set player resources (manpower, fuel, munitions). |
| `Mock_SetPlayerStartingPosition(player, x, y, z)` | — | Set a player's starting position. |
| `Mock_CreateTerritory(sector_id [, owner_id, x, y, z])` | — | Create a territory sector. |
| `Mock_CreateStrategyPoint(entity, sector_id [, is_vp])` | — | Link entity to a sector as a strategy/victory point. |
| `Mock_FireGlobalEvent(event_type, ...)` | — | Dispatch a global event to registered handlers. |
| `Mock_ResetState()` | — | Clear all game state (entities, squads, players, groups, rules). |
| `Mock_AdvanceTime(seconds)` | — | Advance game clock and fire due interval/oneshot rules. |

### SCAR Lifecycle

| Function | Description |
|---|---|
| `Scar_ForceInit()` | Fire all callbacks registered with `Game_AddInit`. |
| `Scar_ForceStart()` | Run full init sequence: `OnGameSetup` delegates → init funcs → `OnInit` delegates. |
| `Scar_ForceGameSetup()` | Run `OnGameSetup` only. |

---

## Mocked SCAR API

All values are typed userdata with metatables (`Entity`, `Squad`, `Player`, `EGroup`, `SGroup`). Passing the wrong type raises an error. Functions not listed below will raise a hard error if called, so you know immediately when something isn't mocked yet.

### Entity (24 functions)

| Function | Description |
|---|---|
| `Entity_GetHealth(entity)` | Current health. |
| `Entity_SetHealth(entity, hp)` | Set health. Sets alive=false if hp <= 0. |
| `Entity_GetHealthMax(entity)` | Maximum health. |
| `Entity_GetHealthPercentage(entity)` | Health as 0.0–1.0. |
| `Entity_GetPosition(entity)` | Returns `{x, y, z}` table. |
| `Entity_SetPosition(entity, pos)` | Set position from `{x, y, z}`. |
| `Entity_GetPlayerOwner(entity)` | Returns owning `Player`. |
| `Entity_SetPlayerOwner(entity, player)` | Set owner. |
| `Entity_IsAlive(entity)` | `true` if alive. |
| `Entity_IsInvulnerable(entity)` | `true` if invulnerable. |
| `Entity_SetInvulnerable(entity, bool)` | Set invulnerability. |
| `Entity_GetBlueprint(entity)` | Blueprint name string. |
| `Entity_Kill(entity)` | Kill the entity (health=0, alive=false). |
| `Entity_WarpToPos(entity, pos)` | Teleport to position. |
| `Entity_GetGameID(entity)` | Returns the entity's integer ID. |
| `Entity_FromWorldID(id)` | Look up entity by ID. |
| `Entity_GetSquad(entity)` | Returns owning `Squad` or `nil`. |
| `Entity_IsVictoryPoint(entity)` | `true` if entity is a victory point. |
| `Entity_Destroy(entity)` | Remove entity from game state. |
| `Entity_IsVehicle(entity)` | `true` if entity is a vehicle. |
| `Entity_IsPartOfSquad(entity)` | `true` if entity belongs to a squad. |
| `Entity_Create(bp, player, x, y, z)` | Create a new entity with auto-assigned ID. |
| `Entity_Spawn(entity)` | Mark entity as spawned. |
| `Entity_DeSpawn(entity)` | Mark entity as despawned. |

### Squad (23 functions)

| Function | Description |
|---|---|
| `Squad_GetHealth(squad)` | Current health (sum of entity health). |
| `Squad_SetHealth(squad, hp)` | Set health. |
| `Squad_GetHealthMax(squad)` | Maximum health. |
| `Squad_GetMaxHealth(squad)` | Alias for `GetHealthMax`. |
| `Squad_GetPosition(squad)` | Returns `{x, y, z}` table. |
| `Squad_GetPlayerOwner(squad)` | Returns owning `Player`. |
| `Squad_SetPlayerOwner(squad, player)` | Set owner. |
| `Squad_Count(squad)` | Number of entities in squad. |
| `Squad_IsAlive(squad)` | `true` if alive. |
| `Squad_SetInvulnerable(squad, bool)` | Set invulnerability. |
| `Squad_WarpToPos(squad, pos)` | Teleport to position. |
| `Squad_GetBlueprint(squad)` | Blueprint name string. |
| `Squad_Kill(squad)` | Kill the squad. |
| `Squad_GetGameID(squad)` | Returns the squad's integer ID. |
| `Squad_GetID(squad)` | Alias for `GetGameID`. |
| `Squad_FromWorldID(id)` | Look up squad by ID. |
| `Squad_DeSpawn(squad)` | Mark squad as despawned. |
| `Squad_IsVehicleSquad(squad)` | `true` if squad is a vehicle. |
| `Squad_IsUnderAttack(squad)` | Always returns `false`. |
| `Squad_IsRetreating(squad)` | Always returns `false`. |
| `Squad_GetVeterancy(squad)` | Current veterancy level. |
| `Squad_IncreaseVeterancy(squad, amount)` | Add veterancy. |
| `Squad_EntityAt(squad, index)` | Get entity at 0-based index. |

### Player (28 functions)

| Function | Description |
|---|---|
| `Player_GetID(player)` | Player's integer ID. |
| `Player_GetTeam(player)` | Team ID. |
| `Player_GetResource(player, resource_type)` | Get a resource amount. |
| `Player_SetResource(player, resource_type, amount)` | Set a resource amount. |
| `Player_SetResources(player, mp, fuel, mun)` | Set all three resources. |
| `Player_AddResource(player, resource_type, amount)` | Add to a resource. |
| `Player_OwnsEntity(player, entity)` | `true` if player owns entity. |
| `Player_OwnsSquad(player, squad)` | `true` if player owns squad. |
| `Player_IsHuman(player)` | `true` if player is human. |
| `Player_GetDisplayName(player)` | Display name string. |
| `Player_GetRaceName(player)` | Race/faction name string. |
| `Player_GetRace(player)` | Alias for `GetRaceName`. |
| `Player_GetStartingPosition(player)` | Starting position `{x, y, z}`. |
| `Player_HasMapEntryPosition(player)` | `true` if starting position is set. |
| `Player_GetMapEntryPosition(player)` | Alias for `GetStartingPosition`. |
| `Player_FindFirstEnemyPlayer(player)` | First player on a different team. |
| `Player_GetSlotIndex(player)` | 0-based index in player list. |
| `Player_GetSquadCount(player)` | Number of squads owned. |
| `Player_GetSquads(player)` | `SGroup` containing all owned squads. |
| `Player_GetEntities(player)` | `EGroup` containing all owned entities. |
| `Player_FromId(id)` | Look up player by ID. |
| `Player_FromWorldID(id)` | Alias for `FromId`. |
| `Player_SetRelationship(p1, p2, rel)` | No-op. |
| `Player_SetMaxPopulation(player, cap_type, val)` | Set pop cap. |
| `Player_GetMaxPopulation(player, cap_type)` | Get pop cap. |
| `Player_GetCurrentPopulation(player, cap_type)` | Get current population. |
| `Player_SetUpgradeAvailability(player, bp, avail)` | No-op. |
| `Player_RemoveUpgrade(player, bp)` | No-op. |
| `Player_SetEntityProductionAvailability(player, bp, avail)` | No-op. |

### EGroup (11 functions)

| Function | Description |
|---|---|
| `EGroup_Create(name)` | Create a named entity group. |
| `EGroup_CreateIfNotFound(name)` | Create or return existing group. |
| `EGroup_CreateUnique()` | Create a group with an auto-generated name. |
| `EGroup_Add(egroup, entity)` | Add entity to group. |
| `EGroup_Count(egroup)` | Number of entities. |
| `EGroup_Clear(egroup)` | Remove all entities. |
| `EGroup_ContainsEntity(egroup, entity)` | `true` if entity is in group. |
| `EGroup_GetEntityAt(egroup, index)` | Get entity at 1-based index. |
| `EGroup_Destroy(egroup)` | Clear the group. |
| `EGroup_ForEach(egroup, fn)` | Call `fn(egroup, index, entity)` for each entity. |
| `EGroup_SetInvulnerable(egroup, bool)` | No-op. |

### SGroup (13 functions)

| Function | Description |
|---|---|
| `SGroup_Create(name)` | Create a named squad group. |
| `SGroup_CreateIfNotFound(name)` | Create or return existing group. |
| `SGroup_Add(sgroup, squad)` | Add squad to group. |
| `SGroup_Count(sgroup)` | Number of squads. |
| `SGroup_Clear(sgroup)` | Remove all squads. |
| `SGroup_ContainsSquad(sgroup, squad)` | `true` if squad is in group. |
| `SGroup_GetSquadAt(sgroup, index)` | Get squad at 1-based index. |
| `SGroup_GetSpawnedSquadAt(sgroup, index)` | Alias for `GetSquadAt`. |
| `SGroup_Destroy(sgroup)` | Clear the group. |
| `SGroup_ForEach(sgroup, fn)` | Call `fn(sgroup, index, squad)` for each squad. |
| `SGroup_SetInvulnerable(sgroup, bool)` | No-op. |
| `SGroup_SetSelectable(sgroup, bool)` | No-op. |
| `SGroup_IsHoldingAny(sgroup)` | Always returns `false`. |

### Game & World (22 functions)

| Function | Description |
|---|---|
| `Game_AddInit(fn)` | Register a function to run on game init. |
| `Game_GetLocalPlayer()` | Returns player at index 1. |
| `World_GetPlayerCount()` | Number of players. |
| `World_GetPlayerAt(index)` | Get player at 1-based index. |
| `World_GetGameTime()` | Current game time in seconds. |
| `World_Pos(x, y, z)` | Create a position table `{x, y, z}`. |
| `World_OwnsEntity(entity)` | `true` if entity has no player owner. |
| `World_OwnsSquad(squad)` | `true` if squad has no player owner. |
| `World_GetStrategyPoints(egroup, include_vps)` | Populate egroup with strategy point entities. |
| `World_GetRand(min, max)` | Deterministic: always returns `min`. |
| `World_SetPlayerWin(player)` | Mark player as winner. |
| `World_SetPlayerLose(player)` | Mark player as loser. |
| `World_SetGameOver()` | Set game-over state. |
| `World_IsGameOver()` | `true` if game is over. |
| `World_DistancePointToPoint(p1, p2)` | Euclidean distance between two positions. |
| `World_GetSquadsWithinTerritorySector(sector_id)` | Returns empty table. |
| `Rule_AddInterval(fn, interval)` | Register a repeating rule. |
| `Rule_AddOneShot(fn, delay)` | Register a one-shot rule. |
| `Rule_Remove(fn)` | Remove a rule by function. |
| `Rule_Exists(fn)` | `true` if rule is active. |
| `Rule_AddGlobalEvent(fn, event_type)` | Register a global event handler. |
| `Rule_RemoveMe()` | Remove the currently executing rule. |

### Core (4 functions)

| Function | Description |
|---|---|
| `Core_RegisterModule(name, table, fn)` | Register a module with the core system. |
| `Core_CallDelegateFunctions(delegate, ...)` | Call `ModuleName_Delegate` on all registered modules. |
| `Core_GetPlayersTableEntry(player)` | Returns `{id, player, scarModel}` table. |
| `Core_OnGameOver()` | No-op. |

### Blueprint (13 functions)

| Function | Description |
|---|---|
| `BP_GetEntityBlueprint(name)` | Returns the name string (acts as blueprint handle). |
| `BP_GetSquadBlueprint(name)` | Returns the name string. |
| `BP_GetUpgradeBlueprint(name)` | Returns the name string. |
| `BP_GetAbilityBlueprint(name)` | Returns the name string. |
| `BP_GetName(bp)` | Returns the blueprint name. |
| `BP_UpgradeExists(name)` | Always returns `true`. |
| `BP_EntityExists(name)` | Always returns `true`. |
| `BP_SquadExists(name)` | Always returns `true`. |
| `EBP_Exists(name)` | Always returns `true`. |
| `BP_GetTechTreeBlueprintsByType(type)` | Returns empty table. |
| `BP_GetTechTreeBPInfo(bp)` | Returns stub info table. |
| `BP_GetSquadUIInfo(bp)` | Returns stub UI info table. |
| `BP_GetUpgradeUIInfo(bp)` | Returns stub UI info table. |

### Territory (3 functions)

| Function | Description |
|---|---|
| `Territory_GetSectorContainingPoint(pos)` | Returns sector ID of nearest territory. |
| `Territory_GetSectorOwnerID(sector_id)` | Returns owner player ID (-1 if unowned). |
| `Territory_SetSectorOwner(sector_id, player)` | Set territory owner. |

### Utility & Misc (15 real functions)

| Function | Description |
|---|---|
| `Loc_FormatText(key, ...)` | Returns key with args interpolated. |
| `Loc_Empty()` | Returns empty string. |
| `Loc_ToAnsi(str)` | Returns `str` unchanged. |
| `scartype(val)` | Returns SCAR type constant for userdata. |
| `scartype_tostring(val)` | Returns type name string. |
| `Misc_IsDevMode()` | Always returns `false`. |
| `Misc_IsSteamMode()` | Always returns `false`. |
| `fatal(msg)` | Raises a Lua error. |
| `Table_Contains(table, value)` | `true` if table contains value. |
| `Util_GetPosition(val)` | Extract position from entity/squad/table. |
| `Util_CreateSquads(player, bp, pos, count)` | Returns empty `SGroup` stub. |
| `Game_GetScenarioFileName()` | Returns `"test_scenario"`. |
| `Command_PlayerBroadcastMessage(player, msg)` | No-op. |
| `Modify_PlayerResourceCap(player, res, val)` | No-op. |

### No-op Stubs (~64 functions)

The following are registered as no-ops (accept any arguments, do nothing, return nothing). They exist so scripts can call them without errors:

`Bug_ReportBug`, `Network_CallEvent`, `EventCues_Register`, `EventCues_Trigger`,
`Sound_Play2D`, `Sound_Play3D`,
`UI_SetModal`, `UI_ClearModal`, `UI_CreateDataContext`, `UI_AddChild`, `UI_RemoveChild`, `UI_SetPropertyValue`, `UI_GetPropertyValue`, `UI_SetDataTemplate`, `UI_SetDataContext`, `UI_BindPropertyValue`, `UI_DestroyDataContext`, `UI_TitleDestroy`, `UI_CreateEventCue`, `UI_SetPlayerDataContext`, `UI_CreateCommand`, `UI_SetEnablePauseMenu`, `UI_ToggleDecorators`,
`Camera_MoveTo`, `Camera_FocusOnPosition`, `Camera_SetDefault`, `Camera_SetInputEnabled`,
`FOW_RevealAll`, `FOW_UnRevealAll`,
`Cmd_Ability`, `Cmd_Move`, `Cmd_Attack`, `Cmd_Stop`, `Cmd_Garrison`, `Cmd_AttackMove`, `Cmd_Retreat`, `Cmd_ReinforceUnit`, `Cmd_InstantUpgrade`, `Cmd_EjectOccupants`, `Cmd_MoveToAndDestroy`,
`AI_Enable`, `AI_Disable`, `AI_LockAll`, `AI_UnlockAll`, `AI_SetDifficulty`, `AI_SetPersonality`, `AI_LockSquads`,
`Modifier_Create`, `Modifier_Apply`, `Modifier_Remove`, `Modifier_Enable`, `Modifier_Disable`, `Modify_PlayerResourceRate`,
`Player_SetPopCapOverride`, `Player_ResetPopCapOverride`, `Player_CompleteUpgrade`, `Player_HasUpgrade`,
`Game_GetSPDifficulty`, `Game_SetGameRestoreAvailability`, `Game_GetSimRate`, `Game_TextTitleFade`,
`Misc_DoString`, `Misc_IsCommandLineOptionSet`, `Misc_ClearSelection`, `Misc_SetSelectionInputEnabled`, `Misc_SetDefaultCommandsEnabled`,
`Timer_Start`, `Timer_End`, `Timer_Exists`, `Timer_Pause`, `Timer_Resume`, `Timer_GetRemaining`,
`Obj_Create`, `Obj_SetState`, `Obj_Delete`, `Obj_SetVisible`, `Obj_SetIcon`, `Obj_SetDescription`,
`SGroup_EnableUIDecorator`, `EGroup_EnableUIDecorator`,
`Music_SetIntensity`, `Music_LockIntensity`, `Music_UnlockIntensity`,
`Marker_GetPosition`, `Marker_Exists`,
`TicketUI_SetVisibility`,
`World_IsCampaignMetamapGame`, `Loc_GetString`

---

## Constants

### Resource Types

| Name | Value |
|---|---|
| `RT_Manpower` | 0 |
| `RT_Fuel` | 1 |
| `RT_Munition` | 2 |
| `RT_Action` | 3 |
| `RT_Command` | 4 |

### Cap Types

| Name | Value |
|---|---|
| `CT_Personnel` | 0 |
| `CT_Vehicle` | 1 |

### SCAR Types

| Name | Value |
|---|---|
| `SCARTYPE_ENTITY` | 0 |
| `SCARTYPE_SQUAD` | 1 |
| `SCARTYPE_PLAYER` | 2 |
| `SCARTYPE_EGROUP` | 3 |
| `SCARTYPE_SGROUP` | 4 |
| `SCARTYPE_MARKER` | 5 |
| `ST_SCARPOS` | 6 |

### Game Events

| Name | Value |
|---|---|
| `GE_SquadKilled` | 100 |
| `GE_EntityKilled` | 101 |
| `GE_StrategicPointChanged` | 102 |
| `GE_PlayerTeamChanged` | 103 |
| `GE_DamageReceived` | 104 |
| `GE_BuildItemComplete` | 105 |
| `GE_UpgradeComplete` | 106 |
| `GE_AbilityExecuted` | 107 |
| `GE_ProjectileFired` | 108 |
| `GE_ConstructionComplete` | 109 |
| `GE_PlayerSurrendered` | 110 |
| `GE_PlayerDisconnected` | 111 |
| `GE_BattlegroupAbilityActivated` | 112 |
| `GE_BroadcastMessage` | 113 |
| `GE_SquadItemPickup` | 114 |
| `GE_EntityRecrewed` | 115 |

### Relationships

| Name | Value |
|---|---|
| `R_ENEMY` | 0 |
| `R_ALLY` | 1 |
| `R_NEUTRAL` | 2 |

### Win Reasons

| Name | Value |
|---|---|
| `WR_NONE` | 0 |
| `WR_ANNIHILATION` | 1 |
| `WR_SURRENDER` | 2 |
| `WR_VP` | 3 |

### Difficulty

| Name | Value |
|---|---|
| `GD_EASY` | 0 |
| `GD_NORMAL` | 1 |
| `GD_HARD` | 2 |
| `GD_EXPERT` | 3 |

### Owner Types

| Name | Value |
|---|---|
| `OT_Player` | 0 |
| `OT_Ally` | 1 |
| `OT_Enemy` | 2 |
| `OT_Neutral` | 3 |

### Miscellaneous

| Name | Value |
|---|---|
| `ANY` | `true` |
| `ALL` | `false` |
| `MUT_Multiplication` | 1 |
| `ITEM_REMOVED` | 0 |
| `ITEM_LOCKED` | 1 |
| `TEAMS` | `{}` (empty table) |
