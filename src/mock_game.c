#include "mock_game.h"
#include "game_state.h"
#include "lauxlib.h"

#include <math.h>

static int l_Game_AddInit(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    GameState* gs = game_state_from_lua(L);
    if (gs->init_func_count >= MAX_INIT_FUNCS) {
        return luaL_error(L, "Game_AddInit: maximum init function count reached");
    }
    lua_pushvalue(L, 1);
    gs->init_func_refs[gs->init_func_count++] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static int l_Rule_AddInterval(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    float interval = (float)luaL_checknumber(L, 2);
    GameState* gs = game_state_from_lua(L);
    if (gs->rule_count >= MAX_RULES) {
        return luaL_error(L, "Rule_AddInterval: maximum rule count reached");
    }
    MockRule* r = &gs->rules[gs->rule_count++];
    lua_pushvalue(L, 1);
    r->lua_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    r->type = RULE_INTERVAL;
    r->interval = interval;
    r->elapsed = 0.0f;
    r->active = true;
    return 0;
}

static int l_Rule_AddOneShot(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    float delay = (float)luaL_checknumber(L, 2);
    GameState* gs = game_state_from_lua(L);
    if (gs->rule_count >= MAX_RULES) {
        return luaL_error(L, "Rule_AddOneShot: maximum rule count reached");
    }
    MockRule* r = &gs->rules[gs->rule_count++];
    lua_pushvalue(L, 1);
    r->lua_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    r->type = RULE_ONESHOT;
    r->interval = delay;
    r->elapsed = 0.0f;
    r->active = true;
    return 0;
}

static int l_Rule_Remove(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    GameState* gs = game_state_from_lua(L);
    /* Find rule by comparing function references */
    /* Push the function to compare */
    for (int i = 0; i < gs->rule_count; i++) {
        if (!gs->rules[i].active) continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, gs->rules[i].lua_func_ref);
        if (lua_rawequal(L, 1, -1)) {
            lua_pop(L, 1);
            gs->rules[i].active = false;
            luaL_unref(L, LUA_REGISTRYINDEX, gs->rules[i].lua_func_ref);
            gs->rules[i].lua_func_ref = LUA_NOREF;
            return 0;
        }
        lua_pop(L, 1);
    }
    return 0;
}

static int l_Rule_Exists(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    GameState* gs = game_state_from_lua(L);
    for (int i = 0; i < gs->rule_count; i++) {
        if (!gs->rules[i].active) continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, gs->rules[i].lua_func_ref);
        if (lua_rawequal(L, 1, -1)) {
            lua_pop(L, 1);
            lua_pushboolean(L, 1);
            return 1;
        }
        lua_pop(L, 1);
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int l_World_Pos(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float z = (float)luaL_checknumber(L, 3);
    Position pos = {x, y, z};
    push_position(L, pos);
    return 1;
}

static int l_World_GetPlayerCount(lua_State* L) {
    GameState* gs = game_state_from_lua(L);
    lua_pushinteger(L, gs->player_count);
    return 1;
}

static int l_World_GetPlayerAt(lua_State* L) {
    GameState* gs = game_state_from_lua(L);
    int index = (int)luaL_checkinteger(L, 1);
    /* 1-based index */
    if (index < 1 || index > gs->player_count) {
        return luaL_error(L, "World_GetPlayerAt: index %d out of range", index);
    }
    push_player(L, gs->players[index - 1].id);
    return 1;
}

static int l_Game_GetLocalPlayer(lua_State* L) {
    GameState* gs = game_state_from_lua(L);
    if (gs->player_count > 0) {
        push_player(L, gs->players[0].id);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int l_World_GetGameTime(lua_State* L) {
    GameState* gs = game_state_from_lua(L);
    lua_pushnumber(L, gs->game_time);
    return 1;
}

/* ── Rule_AddGlobalEvent ─────────────────────────────────────────── */

static int l_Rule_AddGlobalEvent(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    int event_type = (int)luaL_checkinteger(L, 2);
    GameState* gs = game_state_from_lua(L);
    if (gs->global_event_count >= MAX_GLOBAL_EVENT_RULES) {
        return luaL_error(L, "Rule_AddGlobalEvent: maximum global event rules reached");
    }
    GlobalEventRule* r = &gs->global_event_rules[gs->global_event_count++];
    lua_pushvalue(L, 1);
    r->lua_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    r->event_type = event_type;
    r->active = true;
    return 0;
}

static int l_Rule_RemoveMe(lua_State* L) {
    /* Deactivate current rule — find the one currently executing.
       In practice this is a no-op since we handle it in AdvanceTime;
       the caller expects the rule to stop repeating. We mark the
       calling rule inactive by convention. */
    (void)L;
    return 0;
}

/* ── World ownership queries ─────────────────────────────────────── */

static int l_World_OwnsEntity(lua_State* L) {
    int entity_id = check_entity_id(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockEntity* e = game_state_get_entity(gs, entity_id);
    /* Returns true if entity exists and has no player owner (world-owned) */
    lua_pushboolean(L, e && e->player_owner_id == 0);
    return 1;
}

static int l_World_OwnsSquad(lua_State* L) {
    int squad_id = check_squad_id(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockSquad* s = game_state_get_squad(gs, squad_id);
    /* Returns true if squad exists and has no player owner (world-owned) */
    lua_pushboolean(L, s && s->player_owner_id == 0);
    return 1;
}

/* ── World_GetStrategyPoints ─────────────────────────────────────── */

static int l_World_GetStrategyPoints(lua_State* L) {
    /* World_GetStrategyPoints(egroup, include_vps) — populates EGroup */
    const char* eg_name = check_egroup_name(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockEGroup* eg = game_state_get_egroup(gs, eg_name);
    if (!eg) {
        return luaL_error(L, "World_GetStrategyPoints: EGroup '%s' not found", eg_name);
    }
    for (int i = 0; i < gs->strategy_point_count; i++) {
        if (eg->count < MAX_GROUP_ITEMS) {
            eg->entity_ids[eg->count++] = gs->strategy_points[i].entity_id;
        }
    }
    return 0;
}

/* ── World_GetRand ────────────────────────────────────────────────── */

static int l_World_GetRand(lua_State* L) {
    int min_val = (int)luaL_checkinteger(L, 1);
    int max_val = (int)luaL_checkinteger(L, 2);
    /* Deterministic: always return min for reproducible tests */
    lua_pushinteger(L, min_val);
    (void)max_val;
    return 1;
}

/* ── Win/Lose/GameOver ────────────────────────────────────────────── */

static int l_World_SetPlayerWin(lua_State* L) {
    int player_id = check_player_id(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockPlayer* p = game_state_get_player(gs, player_id);
    if (p) p->won = true;
    return 0;
}

static int l_World_SetPlayerLose(lua_State* L) {
    int player_id = check_player_id(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockPlayer* p = game_state_get_player(gs, player_id);
    if (p) p->lost = true;
    return 0;
}

static int l_World_SetGameOver(lua_State* L) {
    GameState* gs = game_state_from_lua(L);
    gs->game_over = true;
    return 0;
}

static int l_World_IsGameOver(lua_State* L) {
    GameState* gs = game_state_from_lua(L);
    lua_pushboolean(L, gs->game_over);
    return 1;
}

/* ── World_DistancePointToPoint ───────────────────────────────────── */

static int l_World_DistancePointToPoint(lua_State* L) {
    Position p1 = read_position(L, 1);
    Position p2 = read_position(L, 2);
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float dz = p2.z - p1.z;
    lua_pushnumber(L, (float)sqrt(dx*dx + dy*dy + dz*dz));
    return 1;
}

/* ── World_GetSquadsWithinTerritorySector ─────────────────────────── */

static int l_World_GetSquadsWithinTerritorySector(lua_State* L) {
    /* Returns an SGroup of squads in the sector.
       For mock: sector_id is arg1, we push squads near sector pos. */
    /* Simplified: return empty SGroup for now — tests can pre-populate */
    (void)L;
    lua_newtable(L);
    return 1;
}

void mock_game_register(lua_State* L) {
    lua_register(L, "Game_AddInit",         l_Game_AddInit);
    lua_register(L, "Rule_AddInterval",     l_Rule_AddInterval);
    lua_register(L, "Rule_AddOneShot",      l_Rule_AddOneShot);
    lua_register(L, "Rule_Remove",          l_Rule_Remove);
    lua_register(L, "Rule_Exists",          l_Rule_Exists);
    lua_register(L, "World_Pos",            l_World_Pos);
    lua_register(L, "World_GetPlayerCount", l_World_GetPlayerCount);
    lua_register(L, "World_GetPlayerAt",    l_World_GetPlayerAt);
    lua_register(L, "Game_GetLocalPlayer",  l_Game_GetLocalPlayer);
    lua_register(L, "World_GetGameTime",    l_World_GetGameTime);
    lua_register(L, "Rule_AddGlobalEvent",  l_Rule_AddGlobalEvent);
    lua_register(L, "Rule_RemoveMe",        l_Rule_RemoveMe);
    lua_register(L, "World_OwnsEntity",     l_World_OwnsEntity);
    lua_register(L, "World_OwnsSquad",      l_World_OwnsSquad);
    lua_register(L, "World_GetStrategyPoints", l_World_GetStrategyPoints);
    lua_register(L, "World_GetRand",        l_World_GetRand);
    lua_register(L, "World_SetPlayerWin",   l_World_SetPlayerWin);
    lua_register(L, "World_SetPlayerLose",  l_World_SetPlayerLose);
    lua_register(L, "World_SetGameOver",    l_World_SetGameOver);
    lua_register(L, "World_IsGameOver",     l_World_IsGameOver);
    lua_register(L, "World_DistancePointToPoint", l_World_DistancePointToPoint);
    lua_register(L, "World_GetSquadsWithinTerritorySector", l_World_GetSquadsWithinTerritorySector);
}
