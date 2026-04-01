#include "mock_game.h"
#include "game_state.h"
#include "lauxlib.h"

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
    lua_pushinteger(L, gs->players[index - 1].id);
    return 1;
}

static int l_Game_GetLocalPlayer(lua_State* L) {
    GameState* gs = game_state_from_lua(L);
    if (gs->player_count > 0) {
        lua_pushinteger(L, gs->players[0].id);
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
}
