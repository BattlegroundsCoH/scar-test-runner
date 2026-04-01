#include "mock_egroup.h"
#include "game_state.h"
#include "lauxlib.h"

#include <string.h>
#include <stdio.h>

static int l_EGroup_Create(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    GameState* gs = game_state_from_lua(L);
    /* Return existing group if name already exists */
    MockEGroup* eg = game_state_get_egroup(gs, name);
    if (!eg) {
        eg = game_state_add_egroup(gs, name);
        if (!eg) {
            return luaL_error(L, "EGroup_Create: maximum egroup count reached");
        }
    }
    push_egroup(L, eg->name);
    return 1;
}

static MockEGroup* check_egroup(lua_State* L, int arg_idx) {
    const char* name = check_egroup_name(L, arg_idx);
    GameState* gs = game_state_from_lua(L);
    MockEGroup* eg = game_state_get_egroup(gs, name);
    if (!eg) {
        luaL_error(L, "EGroup_*: egroup '%s' not found", name);
    }
    return eg;
}

static int l_EGroup_Add(lua_State* L) {
    MockEGroup* eg = check_egroup(L, 1);
    int entity_id = check_entity_id(L, 2);
    if (eg->count >= MAX_GROUP_ITEMS) {
        return luaL_error(L, "EGroup_Add: group '%s' is full", eg->name);
    }
    eg->entity_ids[eg->count++] = entity_id;
    return 0;
}

static int l_EGroup_Count(lua_State* L) {
    MockEGroup* eg = check_egroup(L, 1);
    lua_pushinteger(L, eg->count);
    return 1;
}

static int l_EGroup_Clear(lua_State* L) {
    MockEGroup* eg = check_egroup(L, 1);
    eg->count = 0;
    return 0;
}

static int l_EGroup_ContainsEntity(lua_State* L) {
    MockEGroup* eg = check_egroup(L, 1);
    int entity_id = check_entity_id(L, 2);
    for (int i = 0; i < eg->count; i++) {
        if (eg->entity_ids[i] == entity_id) {
            lua_pushboolean(L, 1);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int l_EGroup_GetEntityAt(lua_State* L) {
    MockEGroup* eg = check_egroup(L, 1);
    int index = (int)luaL_checkinteger(L, 2);
    /* SCAR uses 1-based indexing */
    if (index < 1 || index > eg->count) {
        return luaL_error(L, "EGroup_GetEntityAt: index %d out of range (1..%d)", index, eg->count);
    }
    push_entity(L, eg->entity_ids[index - 1]);
    return 1;
}

static int l_EGroup_Destroy(lua_State* L) {
    /* Just clear the group — we don't actually remove it from the array */
    MockEGroup* eg = check_egroup(L, 1);
    eg->count = 0;
    eg->name[0] = '\0';
    return 0;
}

/* EGroup_ForEach(egroup, func) — calls func(egroup, index, entity) for each entity */
static int l_EGroup_ForEach(lua_State* L) {
    MockEGroup* eg = check_egroup(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    for (int i = 0; i < eg->count; i++) {
        lua_pushvalue(L, 2);           /* func */
        lua_pushvalue(L, 1);           /* egroup */
        lua_pushinteger(L, i + 1);     /* 1-based index */
        push_entity(L, eg->entity_ids[i]);
        if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
            return luaL_error(L, "EGroup_ForEach: %s", lua_tostring(L, -1));
        }
    }
    return 0;
}

/* EGroup_CreateUnique() — returns a new unique empty EGroup */
static int l_EGroup_CreateUnique(lua_State* L) {
    GameState* gs = game_state_from_lua(L);
    char name[MAX_BLUEPRINT_LEN];
    snprintf(name, sizeof(name), "__egroup_unique_%d", gs->egroup_unique_counter++);
    MockEGroup* eg = game_state_add_egroup(gs, name);
    if (!eg) {
        return luaL_error(L, "EGroup_CreateUnique: maximum egroup count reached");
    }
    push_egroup(L, eg->name);
    return 1;
}

/* EGroup_SetInvulnerable(egroup, invulnerable, time) */
static int l_EGroup_SetInvulnerable(lua_State* L) {
    MockEGroup* eg = check_egroup(L, 1);
    int invuln = lua_toboolean(L, 2);
    GameState* gs = game_state_from_lua(L);
    for (int i = 0; i < eg->count; i++) {
        MockEntity* e = game_state_get_entity(gs, eg->entity_ids[i]);
        if (e) e->invulnerable = invuln;
    }
    return 0;
}

void mock_egroup_register(lua_State* L) {
    lua_register(L, "EGroup_Create",         l_EGroup_Create);
    lua_register(L, "EGroup_Add",            l_EGroup_Add);
    lua_register(L, "EGroup_Count",          l_EGroup_Count);
    lua_register(L, "EGroup_Clear",          l_EGroup_Clear);
    lua_register(L, "EGroup_ContainsEntity", l_EGroup_ContainsEntity);
    lua_register(L, "EGroup_GetEntityAt",    l_EGroup_GetEntityAt);
    lua_register(L, "EGroup_Destroy",         l_EGroup_Destroy);
    lua_register(L, "EGroup_ForEach",          l_EGroup_ForEach);
    lua_register(L, "EGroup_CreateUnique",     l_EGroup_CreateUnique);
    lua_register(L, "EGroup_SetInvulnerable",  l_EGroup_SetInvulnerable);

    /* EGroup_CreateIfNotFound is a common alias */
    lua_register(L, "EGroup_CreateIfNotFound", l_EGroup_Create);
}
