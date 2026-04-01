#include "mock_entity.h"
#include "game_state.h"
#include "lauxlib.h"

#include <string.h>

static MockEntity* check_entity(lua_State* L, int arg_idx) {
    GameState* gs = game_state_from_lua(L);
    int id = check_entity_id(L, arg_idx);
    MockEntity* e = game_state_get_entity(gs, id);
    if (!e) {
        luaL_error(L, "Entity_*: entity with id %d not found", id);
    }
    return e;
}

static int l_Entity_GetHealth(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    lua_pushnumber(L, e->health);
    return 1;
}

static int l_Entity_SetHealth(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    float health = (float)luaL_checknumber(L, 2);
    e->health = health;
    if (health <= 0.0f) {
        e->alive = false;
    }
    return 0;
}

static int l_Entity_GetHealthMax(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    lua_pushnumber(L, e->health_max);
    return 1;
}

static int l_Entity_GetHealthPercentage(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    if (e->health_max > 0.0f) {
        lua_pushnumber(L, e->health / e->health_max);
    } else {
        lua_pushnumber(L, 0.0);
    }
    return 1;
}

static int l_Entity_GetPosition(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    push_position(L, e->position);
    return 1;
}

static int l_Entity_SetPosition(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    e->position = read_position(L, 2);
    return 0;
}

static int l_Entity_GetPlayerOwner(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    push_player(L, e->player_owner_id);
    return 1;
}

static int l_Entity_SetPlayerOwner(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    e->player_owner_id = check_player_id(L, 2);
    return 0;
}

static int l_Entity_SetInvulnerable(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    e->invulnerable = lua_toboolean(L, 2);
    /* reset_time (arg 3) ignored in mock */
    return 0;
}

static int l_Entity_IsAlive(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    lua_pushboolean(L, e->alive);
    return 1;
}

static int l_Entity_WarpToPos(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    e->position = read_position(L, 2);
    return 0;
}

static int l_Entity_IsInvulnerable(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    lua_pushboolean(L, e->invulnerable);
    return 1;
}

static int l_Entity_GetBlueprint(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    lua_pushstring(L, e->blueprint);
    return 1;
}

static int l_Entity_Kill(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    e->alive = false;
    e->health = 0.0f;
    return 0;
}

static int l_Entity_GetGameID(lua_State* L) {
    int id = check_entity_id(L, 1);
    lua_pushinteger(L, id);
    return 1;
}

static int l_Entity_FromWorldID(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockEntity* e = game_state_get_entity(gs, id);
    if (!e) {
        return luaL_error(L, "Entity_FromWorldID: entity with id %d not found", id);
    }
    push_entity(L, id);
    return 1;
}

static int l_Entity_GetSquad(lua_State* L) {
    MockEntity* e = check_entity(L, 1);
    GameState* gs = game_state_from_lua(L);
    /* Find squad containing this entity */
    for (int i = 0; i < gs->squad_count; i++) {
        for (int j = 0; j < gs->squads[i].entity_count; j++) {
            if (gs->squads[i].entity_ids[j] == e->id) {
                push_squad(L, gs->squads[i].id);
                return 1;
            }
        }
    }
    lua_pushnil(L);
    return 1;
}

void mock_entity_register(lua_State* L) {
    lua_register(L, "Entity_GetHealth",           l_Entity_GetHealth);
    lua_register(L, "Entity_SetHealth",           l_Entity_SetHealth);
    lua_register(L, "Entity_GetHealthMax",        l_Entity_GetHealthMax);
    lua_register(L, "Entity_GetHealthPercentage", l_Entity_GetHealthPercentage);
    lua_register(L, "Entity_GetPosition",         l_Entity_GetPosition);
    lua_register(L, "Entity_SetPosition",         l_Entity_SetPosition);
    lua_register(L, "Entity_GetPlayerOwner",      l_Entity_GetPlayerOwner);
    lua_register(L, "Entity_SetPlayerOwner",      l_Entity_SetPlayerOwner);
    lua_register(L, "Entity_SetInvulnerable",     l_Entity_SetInvulnerable);
    lua_register(L, "Entity_IsAlive",             l_Entity_IsAlive);
    lua_register(L, "Entity_WarpToPos",           l_Entity_WarpToPos);
    lua_register(L, "Entity_IsInvulnerable",      l_Entity_IsInvulnerable);
    lua_register(L, "Entity_GetBlueprint",        l_Entity_GetBlueprint);
    lua_register(L, "Entity_Kill",                l_Entity_Kill);
    lua_register(L, "Entity_GetGameID",           l_Entity_GetGameID);
    lua_register(L, "Entity_FromWorldID",         l_Entity_FromWorldID);
    lua_register(L, "Entity_GetSquad",            l_Entity_GetSquad);
}
