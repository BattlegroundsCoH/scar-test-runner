#include "mock_player.h"
#include "game_state.h"
#include "lauxlib.h"

static MockPlayer* check_player(lua_State* L, int arg_idx) {
    GameState* gs = game_state_from_lua(L);
    int id = (int)luaL_checkinteger(L, arg_idx);
    MockPlayer* p = game_state_get_player(gs, id);
    if (!p) {
        luaL_error(L, "Player_*: player with id %d not found", id);
    }
    return p;
}

/* RT_Manpower=0, RT_Fuel=1, RT_Munition=2 */
static float* player_resource_ptr(MockPlayer* p, int resource_type) {
    switch (resource_type) {
        case 0: return &p->resources.manpower;
        case 1: return &p->resources.fuel;
        case 2: return &p->resources.munitions;
        default: return NULL;
    }
}

static int l_Player_GetResource(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    int rt = (int)luaL_checkinteger(L, 2);
    float* res = player_resource_ptr(p, rt);
    if (!res) {
        return luaL_error(L, "Player_GetResource: invalid resource type %d", rt);
    }
    lua_pushnumber(L, *res);
    return 1;
}

static int l_Player_SetResource(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    int rt = (int)luaL_checkinteger(L, 2);
    float val = (float)luaL_checknumber(L, 3);
    float* res = player_resource_ptr(p, rt);
    if (!res) {
        return luaL_error(L, "Player_SetResource: invalid resource type %d", rt);
    }
    *res = val;
    return 0;
}

static int l_Player_SetResources(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, "manpower");
    if (!lua_isnil(L, -1)) p->resources.manpower = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "fuel");
    if (!lua_isnil(L, -1)) p->resources.fuel = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "munitions");
    if (!lua_isnil(L, -1)) p->resources.munitions = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return 0;
}

static int l_Player_OwnsEntity(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    GameState* gs = game_state_from_lua(L);
    int eid = (int)luaL_checkinteger(L, 2);
    MockEntity* e = game_state_get_entity(gs, eid);
    lua_pushboolean(L, e && e->player_owner_id == p->id);
    return 1;
}

static int l_Player_OwnsSquad(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    GameState* gs = game_state_from_lua(L);
    int sid = (int)luaL_checkinteger(L, 2);
    MockSquad* s = game_state_get_squad(gs, sid);
    lua_pushboolean(L, s && s->player_owner_id == p->id);
    return 1;
}

static int l_Player_SetMaxPopulation(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    /* captype (arg 2) ignored — we use a single cap */
    int newcap = (int)luaL_checkinteger(L, 3);
    p->population_cap = newcap;
    return 0;
}

static int l_Player_GetMaxPopulation(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    /* captype (arg 2) ignored */
    lua_pushinteger(L, p->population_cap);
    return 1;
}

static int l_Player_GetCurrentPopulation(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    /* captype (arg 2) ignored */
    lua_pushinteger(L, p->population_current);
    return 1;
}

static int l_Player_GetID(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    lua_pushinteger(L, p->id);
    return 1;
}

static int l_Player_GetTeam(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    lua_pushinteger(L, p->team_id);
    return 1;
}

void mock_player_register(lua_State* L) {
    lua_register(L, "Player_GetResource",          l_Player_GetResource);
    lua_register(L, "Player_SetResource",          l_Player_SetResource);
    lua_register(L, "Player_SetResources",         l_Player_SetResources);
    lua_register(L, "Player_OwnsEntity",           l_Player_OwnsEntity);
    lua_register(L, "Player_OwnsSquad",            l_Player_OwnsSquad);
    lua_register(L, "Player_SetMaxPopulation",     l_Player_SetMaxPopulation);
    lua_register(L, "Player_GetMaxPopulation",     l_Player_GetMaxPopulation);
    lua_register(L, "Player_GetCurrentPopulation", l_Player_GetCurrentPopulation);
    lua_register(L, "Player_GetID",                l_Player_GetID);
    lua_register(L, "Player_GetTeam",              l_Player_GetTeam);
}
