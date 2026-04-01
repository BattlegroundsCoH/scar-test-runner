#include "game_state.h"
#include "lauxlib.h"

#include <stdlib.h>
#include <string.h>

GameState* game_state_create(void) {
    GameState* gs = (GameState*)calloc(1, sizeof(GameState));
    return gs;
}

void game_state_destroy(GameState* gs) {
    if (gs) {
        free(gs);
    }
}

void game_state_reset(GameState* gs, lua_State* L) {
    /* Release Lua references for rules (but NOT init functions —
       those persist for the lifetime of the file's lua_State) */
    for (int i = 0; i < gs->rule_count; i++) {
        if (gs->rules[i].lua_func_ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, gs->rules[i].lua_func_ref);
        }
    }

    /* Preserve init funcs */
    int saved_init_count = gs->init_func_count;
    int saved_init_refs[MAX_INIT_FUNCS];
    for (int i = 0; i < saved_init_count; i++) {
        saved_init_refs[i] = gs->init_func_refs[i];
    }

    memset(gs, 0, sizeof(GameState));

    /* Restore init funcs */
    gs->init_func_count = saved_init_count;
    for (int i = 0; i < saved_init_count; i++) {
        gs->init_func_refs[i] = saved_init_refs[i];
    }
}

void game_state_store(lua_State* L, GameState* gs) {
    lua_pushlightuserdata(L, (void*)gs);
    lua_setfield(L, LUA_REGISTRYINDEX, GAME_STATE_REGISTRY_KEY);
}

GameState* game_state_from_lua(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, GAME_STATE_REGISTRY_KEY);
    GameState* gs = (GameState*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return gs;
}

/* ── Lookups ─────────────────────────────────────────────────────── */

MockEntity* game_state_get_entity(GameState* gs, int id) {
    for (int i = 0; i < gs->entity_count; i++) {
        if (gs->entities[i].id == id) {
            return &gs->entities[i];
        }
    }
    return NULL;
}

MockSquad* game_state_get_squad(GameState* gs, int id) {
    for (int i = 0; i < gs->squad_count; i++) {
        if (gs->squads[i].id == id) {
            return &gs->squads[i];
        }
    }
    return NULL;
}

MockPlayer* game_state_get_player(GameState* gs, int id) {
    for (int i = 0; i < gs->player_count; i++) {
        if (gs->players[i].id == id) {
            return &gs->players[i];
        }
    }
    return NULL;
}

MockEGroup* game_state_get_egroup(GameState* gs, const char* name) {
    for (int i = 0; i < gs->egroup_count; i++) {
        if (strcmp(gs->egroups[i].name, name) == 0) {
            return &gs->egroups[i];
        }
    }
    return NULL;
}

MockSGroup* game_state_get_sgroup(GameState* gs, const char* name) {
    for (int i = 0; i < gs->sgroup_count; i++) {
        if (strcmp(gs->sgroups[i].name, name) == 0) {
            return &gs->sgroups[i];
        }
    }
    return NULL;
}

/* ── Additions ───────────────────────────────────────────────────── */

MockEntity* game_state_add_entity(GameState* gs, int id) {
    if (gs->entity_count >= MAX_ENTITIES) return NULL;
    MockEntity* e = &gs->entities[gs->entity_count++];
    memset(e, 0, sizeof(MockEntity));
    e->id = id;
    e->alive = true;
    e->health = 1.0f;
    e->health_max = 1.0f;
    return e;
}

MockSquad* game_state_add_squad(GameState* gs, int id) {
    if (gs->squad_count >= MAX_SQUADS) return NULL;
    MockSquad* s = &gs->squads[gs->squad_count++];
    memset(s, 0, sizeof(MockSquad));
    s->id = id;
    s->alive = true;
    return s;
}

MockPlayer* game_state_add_player(GameState* gs, int id) {
    if (gs->player_count >= MAX_PLAYERS) return NULL;
    MockPlayer* p = &gs->players[gs->player_count++];
    memset(p, 0, sizeof(MockPlayer));
    p->id = id;
    p->population_cap = 100;
    return p;
}

MockEGroup* game_state_add_egroup(GameState* gs, const char* name) {
    if (gs->egroup_count >= MAX_EGROUPS) return NULL;
    MockEGroup* eg = &gs->egroups[gs->egroup_count++];
    memset(eg, 0, sizeof(MockEGroup));
    strncpy(eg->name, name, MAX_BLUEPRINT_LEN - 1);
    return eg;
}

MockSGroup* game_state_add_sgroup(GameState* gs, const char* name) {
    if (gs->sgroup_count >= MAX_SGROUPS) return NULL;
    MockSGroup* sg = &gs->sgroups[gs->sgroup_count++];
    memset(sg, 0, sizeof(MockSGroup));
    strncpy(sg->name, name, MAX_BLUEPRINT_LEN - 1);
    return sg;
}

/* ── Position helpers ────────────────────────────────────────────── */

void push_position(lua_State* L, Position pos) {
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, pos.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, pos.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, pos.z);
    lua_setfield(L, -2, "z");
}

Position read_position(lua_State* L, int idx) {
    Position pos = {0, 0, 0};
    if (lua_istable(L, idx)) {
        lua_getfield(L, idx, "x");
        pos.x = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, idx, "y");
        pos.y = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, idx, "z");
        pos.z = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    return pos;
}
