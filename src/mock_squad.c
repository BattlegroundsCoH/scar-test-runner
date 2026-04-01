#include "mock_squad.h"
#include "game_state.h"
#include "lauxlib.h"

#include <string.h>

static MockSquad* check_squad(lua_State* L, int arg_idx) {
    GameState* gs = game_state_from_lua(L);
    int id = check_squad_id(L, arg_idx);
    MockSquad* s = game_state_get_squad(gs, id);
    if (!s) {
        luaL_error(L, "Squad_*: squad with id %d not found", id);
    }
    return s;
}

static int l_Squad_GetHealth(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    /* Aggregate health from member entities */
    GameState* gs = game_state_from_lua(L);
    float total = 0.0f;
    for (int i = 0; i < s->entity_count; i++) {
        MockEntity* e = game_state_get_entity(gs, s->entity_ids[i]);
        if (e) total += e->health;
    }
    /* If no entities, fall back to 0 */
    lua_pushnumber(L, total);
    return 1;
}

static int l_Squad_SetHealth(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    float health = (float)luaL_checknumber(L, 2);
    /* Distribute evenly to member entities */
    GameState* gs = game_state_from_lua(L);
    if (s->entity_count > 0) {
        float each = health / s->entity_count;
        for (int i = 0; i < s->entity_count; i++) {
            MockEntity* e = game_state_get_entity(gs, s->entity_ids[i]);
            if (e) {
                e->health = each;
                e->alive = (each > 0.0f);
            }
        }
    }
    return 0;
}

static int l_Squad_GetHealthMax(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    GameState* gs = game_state_from_lua(L);
    float total = 0.0f;
    for (int i = 0; i < s->entity_count; i++) {
        MockEntity* e = game_state_get_entity(gs, s->entity_ids[i]);
        if (e) total += e->health_max;
    }
    lua_pushnumber(L, total);
    return 1;
}

static int l_Squad_GetPosition(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    push_position(L, s->position);
    return 1;
}

static int l_Squad_GetPlayerOwner(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    push_player(L, s->player_owner_id);
    return 1;
}

static int l_Squad_SetPlayerOwner(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    s->player_owner_id = check_player_id(L, 2);
    /* Also update member entities */
    GameState* gs = game_state_from_lua(L);
    for (int i = 0; i < s->entity_count; i++) {
        MockEntity* e = game_state_get_entity(gs, s->entity_ids[i]);
        if (e) e->player_owner_id = s->player_owner_id;
    }
    return 0;
}

static int l_Squad_Count(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    lua_pushinteger(L, s->entity_count);
    return 1;
}

static int l_Squad_IsAlive(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    lua_pushboolean(L, s->alive);
    return 1;
}

static int l_Squad_SetInvulnerable(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    s->invulnerable = lua_toboolean(L, 2);
    /* Also set on member entities */
    GameState* gs = game_state_from_lua(L);
    for (int i = 0; i < s->entity_count; i++) {
        MockEntity* e = game_state_get_entity(gs, s->entity_ids[i]);
        if (e) e->invulnerable = s->invulnerable;
    }
    return 0;
}

static int l_Squad_WarpToPos(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    s->position = read_position(L, 2);
    return 0;
}

static int l_Squad_GetBlueprint(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    lua_pushstring(L, s->blueprint);
    return 1;
}

static int l_Squad_Kill(lua_State* L) {
    MockSquad* s = check_squad(L, 1);
    s->alive = false;
    GameState* gs = game_state_from_lua(L);
    for (int i = 0; i < s->entity_count; i++) {
        MockEntity* e = game_state_get_entity(gs, s->entity_ids[i]);
        if (e) {
            e->alive = false;
            e->health = 0.0f;
        }
    }
    return 0;
}

static int l_Squad_GetGameID(lua_State* L) {
    int id = check_squad_id(L, 1);
    lua_pushinteger(L, id);
    return 1;
}

static int l_Squad_FromWorldID(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockSquad* s = game_state_get_squad(gs, id);
    if (!s) {
        return luaL_error(L, "Squad_FromWorldID: squad with id %d not found", id);
    }
    push_squad(L, id);
    return 1;
}

void mock_squad_register(lua_State* L) {
    lua_register(L, "Squad_GetHealth",       l_Squad_GetHealth);
    lua_register(L, "Squad_SetHealth",       l_Squad_SetHealth);
    lua_register(L, "Squad_GetHealthMax",    l_Squad_GetHealthMax);
    lua_register(L, "Squad_GetPosition",     l_Squad_GetPosition);
    lua_register(L, "Squad_GetPlayerOwner",  l_Squad_GetPlayerOwner);
    lua_register(L, "Squad_SetPlayerOwner",  l_Squad_SetPlayerOwner);
    lua_register(L, "Squad_Count",           l_Squad_Count);
    lua_register(L, "Squad_IsAlive",         l_Squad_IsAlive);
    lua_register(L, "Squad_SetInvulnerable", l_Squad_SetInvulnerable);
    lua_register(L, "Squad_WarpToPos",       l_Squad_WarpToPos);
    lua_register(L, "Squad_GetBlueprint",    l_Squad_GetBlueprint);
    lua_register(L, "Squad_Kill",            l_Squad_Kill);
    lua_register(L, "Squad_GetGameID",       l_Squad_GetGameID);
    lua_register(L, "Squad_FromWorldID",     l_Squad_FromWorldID);
}
