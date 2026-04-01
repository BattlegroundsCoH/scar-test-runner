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
    for (int i = 0; i < gs->global_event_count; i++) {
        if (gs->global_event_rules[i].lua_func_ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, gs->global_event_rules[i].lua_func_ref);
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
    e->spawned = true;
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
    s->spawned = true;
    return s;
}

MockPlayer* game_state_add_player(GameState* gs, int id) {
    if (gs->player_count >= MAX_PLAYERS) return NULL;
    MockPlayer* p = &gs->players[gs->player_count++];
    memset(p, 0, sizeof(MockPlayer));
    p->id = id;
    p->population_cap = 100;
    p->is_human = true;
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

/* ── Removal ─────────────────────────────────────────────────────── */

void game_state_remove_entity(GameState* gs, int id) {
    for (int i = 0; i < gs->entity_count; i++) {
        if (gs->entities[i].id == id) {
            /* Shift remaining elements down */
            for (int j = i; j < gs->entity_count - 1; j++) {
                gs->entities[j] = gs->entities[j + 1];
            }
            gs->entity_count--;
            return;
        }
    }
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

/* ── Typed userdata helpers ──────────────────────────────────────── */

void push_entity(lua_State* L, int id) {
    EntityUD* ud = (EntityUD*)lua_newuserdata(L, sizeof(EntityUD));
    ud->id = id;
    luaL_setmetatable(L, SCAR_ENTITY_MT);
}

void push_squad(lua_State* L, int id) {
    SquadUD* ud = (SquadUD*)lua_newuserdata(L, sizeof(SquadUD));
    ud->id = id;
    luaL_setmetatable(L, SCAR_SQUAD_MT);
}

void push_player(lua_State* L, int id) {
    PlayerUD* ud = (PlayerUD*)lua_newuserdata(L, sizeof(PlayerUD));
    ud->id = id;
    luaL_setmetatable(L, SCAR_PLAYER_MT);
}

void push_egroup(lua_State* L, const char* name) {
    EGroupUD* ud = (EGroupUD*)lua_newuserdata(L, sizeof(EGroupUD));
    strncpy(ud->name, name, MAX_BLUEPRINT_LEN - 1);
    ud->name[MAX_BLUEPRINT_LEN - 1] = '\0';
    luaL_setmetatable(L, SCAR_EGROUP_MT);
}

void push_sgroup(lua_State* L, const char* name) {
    SGroupUD* ud = (SGroupUD*)lua_newuserdata(L, sizeof(SGroupUD));
    strncpy(ud->name, name, MAX_BLUEPRINT_LEN - 1);
    ud->name[MAX_BLUEPRINT_LEN - 1] = '\0';
    luaL_setmetatable(L, SCAR_SGROUP_MT);
}

int check_entity_id(lua_State* L, int idx) {
    EntityUD* ud = (EntityUD*)luaL_checkudata(L, idx, SCAR_ENTITY_MT);
    return ud->id;
}

int check_squad_id(lua_State* L, int idx) {
    SquadUD* ud = (SquadUD*)luaL_checkudata(L, idx, SCAR_SQUAD_MT);
    return ud->id;
}

int check_player_id(lua_State* L, int idx) {
    PlayerUD* ud = (PlayerUD*)luaL_checkudata(L, idx, SCAR_PLAYER_MT);
    return ud->id;
}

const char* check_egroup_name(lua_State* L, int idx) {
    EGroupUD* ud = (EGroupUD*)luaL_checkudata(L, idx, SCAR_EGROUP_MT);
    return ud->name;
}

const char* check_sgroup_name(lua_State* L, int idx) {
    SGroupUD* ud = (SGroupUD*)luaL_checkudata(L, idx, SCAR_SGROUP_MT);
    return ud->name;
}

/* ── __tostring metamethods ──────────────────────────────────────── */

static int entity_tostring(lua_State* L) {
    EntityUD* ud = (EntityUD*)luaL_checkudata(L, 1, SCAR_ENTITY_MT);
    lua_pushfstring(L, "Entity[%d]", ud->id);
    return 1;
}

static int squad_tostring(lua_State* L) {
    SquadUD* ud = (SquadUD*)luaL_checkudata(L, 1, SCAR_SQUAD_MT);
    lua_pushfstring(L, "Squad[%d]", ud->id);
    return 1;
}

static int player_tostring(lua_State* L) {
    PlayerUD* ud = (PlayerUD*)luaL_checkudata(L, 1, SCAR_PLAYER_MT);
    lua_pushfstring(L, "Player[%d]", ud->id);
    return 1;
}

static int egroup_tostring(lua_State* L) {
    EGroupUD* ud = (EGroupUD*)luaL_checkudata(L, 1, SCAR_EGROUP_MT);
    lua_pushfstring(L, "EGroup[%s]", ud->name);
    return 1;
}

static int sgroup_tostring(lua_State* L) {
    SGroupUD* ud = (SGroupUD*)luaL_checkudata(L, 1, SCAR_SGROUP_MT);
    lua_pushfstring(L, "SGroup[%s]", ud->name);
    return 1;
}

/* ── __eq metamethods (same type + same id/name = equal) ─────────── */

static int entity_eq(lua_State* L) {
    EntityUD* a = (EntityUD*)luaL_checkudata(L, 1, SCAR_ENTITY_MT);
    EntityUD* b = (EntityUD*)luaL_checkudata(L, 2, SCAR_ENTITY_MT);
    lua_pushboolean(L, a->id == b->id);
    return 1;
}

static int squad_eq(lua_State* L) {
    SquadUD* a = (SquadUD*)luaL_checkudata(L, 1, SCAR_SQUAD_MT);
    SquadUD* b = (SquadUD*)luaL_checkudata(L, 2, SCAR_SQUAD_MT);
    lua_pushboolean(L, a->id == b->id);
    return 1;
}

static int player_eq(lua_State* L) {
    PlayerUD* a = (PlayerUD*)luaL_checkudata(L, 1, SCAR_PLAYER_MT);
    PlayerUD* b = (PlayerUD*)luaL_checkudata(L, 2, SCAR_PLAYER_MT);
    lua_pushboolean(L, a->id == b->id);
    return 1;
}

static int egroup_eq(lua_State* L) {
    EGroupUD* a = (EGroupUD*)luaL_checkudata(L, 1, SCAR_EGROUP_MT);
    EGroupUD* b = (EGroupUD*)luaL_checkudata(L, 2, SCAR_EGROUP_MT);
    lua_pushboolean(L, strcmp(a->name, b->name) == 0);
    return 1;
}

static int sgroup_eq(lua_State* L) {
    SGroupUD* a = (SGroupUD*)luaL_checkudata(L, 1, SCAR_SGROUP_MT);
    SGroupUD* b = (SGroupUD*)luaL_checkudata(L, 2, SCAR_SGROUP_MT);
    lua_pushboolean(L, strcmp(a->name, b->name) == 0);
    return 1;
}

/* ── Register all metatables ─────────────────────────────────────── */

static void register_metatable(lua_State* L, const char* name,
                               lua_CFunction tostring_fn, lua_CFunction eq_fn) {
    luaL_newmetatable(L, name);
    lua_pushcfunction(L, tostring_fn);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, eq_fn);
    lua_setfield(L, -2, "__eq");
    lua_pop(L, 1);
}

void scar_types_register(lua_State* L) {
    register_metatable(L, SCAR_ENTITY_MT, entity_tostring, entity_eq);
    register_metatable(L, SCAR_SQUAD_MT,  squad_tostring,  squad_eq);
    register_metatable(L, SCAR_PLAYER_MT, player_tostring, player_eq);
    register_metatable(L, SCAR_EGROUP_MT, egroup_tostring, egroup_eq);
    register_metatable(L, SCAR_SGROUP_MT, sgroup_tostring, sgroup_eq);
}
