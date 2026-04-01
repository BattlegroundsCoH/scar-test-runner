#include "mock_sgroup.h"
#include "game_state.h"
#include "lauxlib.h"

#include <string.h>

static int l_SGroup_Create(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockSGroup* sg = game_state_get_sgroup(gs, name);
    if (!sg) {
        sg = game_state_add_sgroup(gs, name);
        if (!sg) {
            return luaL_error(L, "SGroup_Create: maximum sgroup count reached");
        }
    }
    push_sgroup(L, sg->name);
    return 1;
}

static MockSGroup* check_sgroup(lua_State* L, int arg_idx) {
    const char* name = check_sgroup_name(L, arg_idx);
    GameState* gs = game_state_from_lua(L);
    MockSGroup* sg = game_state_get_sgroup(gs, name);
    if (!sg) {
        luaL_error(L, "SGroup_*: sgroup '%s' not found", name);
    }
    return sg;
}

static int l_SGroup_Add(lua_State* L) {
    MockSGroup* sg = check_sgroup(L, 1);
    int squad_id = check_squad_id(L, 2);
    if (sg->count >= MAX_GROUP_ITEMS) {
        return luaL_error(L, "SGroup_Add: group '%s' is full", sg->name);
    }
    sg->squad_ids[sg->count++] = squad_id;
    return 0;
}

static int l_SGroup_Count(lua_State* L) {
    MockSGroup* sg = check_sgroup(L, 1);
    lua_pushinteger(L, sg->count);
    return 1;
}

static int l_SGroup_Clear(lua_State* L) {
    MockSGroup* sg = check_sgroup(L, 1);
    sg->count = 0;
    return 0;
}

static int l_SGroup_ContainsSquad(lua_State* L) {
    MockSGroup* sg = check_sgroup(L, 1);
    int squad_id = check_squad_id(L, 2);
    for (int i = 0; i < sg->count; i++) {
        if (sg->squad_ids[i] == squad_id) {
            lua_pushboolean(L, 1);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int l_SGroup_GetSquadAt(lua_State* L) {
    MockSGroup* sg = check_sgroup(L, 1);
    int index = (int)luaL_checkinteger(L, 2);
    /* SCAR uses 1-based indexing */
    if (index < 1 || index > sg->count) {
        return luaL_error(L, "SGroup_GetSquadAt: index %d out of range (1..%d)", index, sg->count);
    }
    push_squad(L, sg->squad_ids[index - 1]);
    return 1;
}

static int l_SGroup_Destroy(lua_State* L) {
    MockSGroup* sg = check_sgroup(L, 1);
    sg->count = 0;
    sg->name[0] = '\0';
    return 0;
}

/* SGroup_ForEach(sgroup, func) — calls func(sgroup, index, squad) for each squad */
static int l_SGroup_ForEach(lua_State* L) {
    MockSGroup* sg = check_sgroup(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    for (int i = 0; i < sg->count; i++) {
        lua_pushvalue(L, 2);           /* func */
        lua_pushvalue(L, 1);           /* sgroup */
        lua_pushinteger(L, i + 1);     /* 1-based index */
        push_squad(L, sg->squad_ids[i]);
        if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
            return luaL_error(L, "SGroup_ForEach: %s", lua_tostring(L, -1));
        }
    }
    return 0;
}

static int l_noop_sgroup(lua_State* L) {
    (void)L;
    return 0;
}

static int l_noop_sgroup_false(lua_State* L) {
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

/* SGroup_SetInvulnerable(sgroup, invulnerable, time) */
static int l_SGroup_SetInvulnerable(lua_State* L) {
    MockSGroup* sg = check_sgroup(L, 1);
    int invuln = lua_toboolean(L, 2);
    GameState* gs = game_state_from_lua(L);
    for (int i = 0; i < sg->count; i++) {
        MockSquad* s = game_state_get_squad(gs, sg->squad_ids[i]);
        if (s) s->invulnerable = invuln;
    }
    return 0;
}

void mock_sgroup_register(lua_State* L) {
    lua_register(L, "SGroup_Create",        l_SGroup_Create);
    lua_register(L, "SGroup_Add",           l_SGroup_Add);
    lua_register(L, "SGroup_Count",         l_SGroup_Count);
    lua_register(L, "SGroup_Clear",         l_SGroup_Clear);
    lua_register(L, "SGroup_ContainsSquad", l_SGroup_ContainsSquad);
    lua_register(L, "SGroup_GetSquadAt",    l_SGroup_GetSquadAt);
    lua_register(L, "SGroup_Destroy",         l_SGroup_Destroy);
    lua_register(L, "SGroup_ForEach",          l_SGroup_ForEach);
    lua_register(L, "SGroup_SetInvulnerable",  l_SGroup_SetInvulnerable);

    lua_register(L, "SGroup_CreateIfNotFound", l_SGroup_Create);
    lua_register(L, "SGroup_GetSpawnedSquadAt", l_SGroup_GetSquadAt);
    lua_register(L, "SGroup_SetSelectable",     l_noop_sgroup);
    lua_register(L, "SGroup_IsHoldingAny",      l_noop_sgroup_false);
}
