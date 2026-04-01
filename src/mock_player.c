#include "mock_player.h"
#include "game_state.h"
#include "lauxlib.h"

#include <stdio.h>
#include <string.h>

static MockPlayer* check_player(lua_State* L, int arg_idx) {
    GameState* gs = game_state_from_lua(L);
    int id = check_player_id(L, arg_idx);
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
    lua_pushnumber(L, res ? *res : 0.0f);
    return 1;
}

static int l_Player_SetResource(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    int rt = (int)luaL_checkinteger(L, 2);
    float val = (float)luaL_checknumber(L, 3);
    float* res = player_resource_ptr(p, rt);
    if (res) {
        *res = val;
    }
    /* Silently ignore unknown resource types (e.g. RT_Action, RT_Command) */
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
    int eid = check_entity_id(L, 2);
    MockEntity* e = game_state_get_entity(gs, eid);
    lua_pushboolean(L, e && e->player_owner_id == p->id);
    return 1;
}

static int l_Player_OwnsSquad(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    GameState* gs = game_state_from_lua(L);
    int sid = check_squad_id(L, 2);
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

static int l_Player_FromWorldID(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockPlayer* p = game_state_get_player(gs, id);
    if (!p) {
        return luaL_error(L, "Player_FromWorldID: player with id %d not found", id);
    }
    push_player(L, id);
    return 1;
}

static int l_Player_IsHuman(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    lua_pushboolean(L, p->is_human);
    return 1;
}

static int l_Player_GetDisplayName(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    lua_pushstring(L, p->display_name);
    return 1;
}

static int l_Player_GetRaceName(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    lua_pushstring(L, p->race_name);
    return 1;
}

static int l_Player_GetStartingPosition(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    push_position(L, p->starting_position);
    return 1;
}

static int l_Player_HasMapEntryPosition(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    lua_pushboolean(L, p->has_starting_position);
    return 1;
}

static int l_Player_GetMapEntryPosition(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    push_position(L, p->starting_position);
    return 1;
}

static int l_Player_FindFirstEnemyPlayer(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    GameState* gs = game_state_from_lua(L);
    for (int i = 0; i < gs->player_count; i++) {
        if (gs->players[i].id != p->id && gs->players[i].team_id != p->team_id) {
            push_player(L, gs->players[i].id);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int l_Player_GetSlotIndex(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    GameState* gs = game_state_from_lua(L);
    for (int i = 0; i < gs->player_count; i++) {
        if (gs->players[i].id == p->id) {
            lua_pushinteger(L, i);
            return 1;
        }
    }
    lua_pushinteger(L, 0);
    return 1;
}

static int l_Player_GetSquadCount(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    GameState* gs = game_state_from_lua(L);
    int count = 0;
    for (int i = 0; i < gs->squad_count; i++) {
        if (gs->squads[i].player_owner_id == p->id && gs->squads[i].alive) {
            count++;
        }
    }
    lua_pushinteger(L, count);
    return 1;
}

static int l_Player_GetSquads(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    GameState* gs = game_state_from_lua(L);
    /* Create a temporary SGroup and fill with player's squads */
    char name[MAX_BLUEPRINT_LEN];
    snprintf(name, sizeof(name), "__player_%d_squads", p->id);
    MockSGroup* sg = game_state_get_sgroup(gs, name);
    if (!sg) {
        sg = game_state_add_sgroup(gs, name);
    }
    if (sg) {
        sg->count = 0;
        for (int i = 0; i < gs->squad_count; i++) {
            if (gs->squads[i].player_owner_id == p->id && gs->squads[i].alive && sg->count < MAX_GROUP_ITEMS) {
                sg->squad_ids[sg->count++] = gs->squads[i].id;
            }
        }
    }
    push_sgroup(L, name);
    return 1;
}

static int l_Player_GetEntities(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    GameState* gs = game_state_from_lua(L);
    /* Create a temporary EGroup and fill with player's entities */
    char name[MAX_BLUEPRINT_LEN];
    snprintf(name, sizeof(name), "__player_%d_entities", p->id);
    MockEGroup* eg = game_state_get_egroup(gs, name);
    if (!eg) {
        eg = game_state_add_egroup(gs, name);
    }
    if (eg) {
        eg->count = 0;
        for (int i = 0; i < gs->entity_count; i++) {
            if (gs->entities[i].player_owner_id == p->id && gs->entities[i].alive && eg->count < MAX_GROUP_ITEMS) {
                eg->entity_ids[eg->count++] = gs->entities[i].id;
            }
        }
    }
    push_egroup(L, name);
    return 1;
}

static int l_Player_FromId(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockPlayer* p = game_state_get_player(gs, id);
    if (!p) {
        return luaL_error(L, "Player_FromId: player with id %d not found", id);
    }
    push_player(L, id);
    return 1;
}

static int l_Player_SetRelationship(lua_State* L) {
    /* No-op: we use team_id for relationship logic */
    (void)L;
    return 0;
}

static int l_Player_AddResource(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    int rt = (int)luaL_checkinteger(L, 2);
    float amount = (float)luaL_checknumber(L, 3);
    float* res = player_resource_ptr(p, rt);
    if (res) {
        *res += amount;
        if (*res < 0.0f) *res = 0.0f;
    }
    return 0;
}

static int l_Player_GetRace(lua_State* L) {
    MockPlayer* p = check_player(L, 1);
    lua_pushstring(L, p->race_name);
    return 1;
}

static int l_Player_SetUpgradeAvailability(lua_State* L) {
    (void)L; /* No-op */
    return 0;
}

static int l_Player_RemoveUpgrade(lua_State* L) {
    (void)L; /* No-op */
    return 0;
}

static int l_Player_SetEntityProductionAvailability(lua_State* L) {
    (void)L; /* No-op */
    return 0;
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
    lua_register(L, "Player_FromWorldID",          l_Player_FromWorldID);
    lua_register(L, "Player_IsHuman",              l_Player_IsHuman);
    lua_register(L, "Player_GetDisplayName",       l_Player_GetDisplayName);
    lua_register(L, "Player_GetRaceName",          l_Player_GetRaceName);
    lua_register(L, "Player_GetStartingPosition",  l_Player_GetStartingPosition);
    lua_register(L, "Player_HasMapEntryPosition",  l_Player_HasMapEntryPosition);
    lua_register(L, "Player_GetMapEntryPosition",  l_Player_GetMapEntryPosition);
    lua_register(L, "Player_FindFirstEnemyPlayer", l_Player_FindFirstEnemyPlayer);
    lua_register(L, "Player_GetSlotIndex",         l_Player_GetSlotIndex);
    lua_register(L, "Player_GetSquadCount",        l_Player_GetSquadCount);
    lua_register(L, "Player_GetSquads",            l_Player_GetSquads);
    lua_register(L, "Player_GetEntities",          l_Player_GetEntities);
    lua_register(L, "Player_FromId",               l_Player_FromId);
    lua_register(L, "Player_SetRelationship",      l_Player_SetRelationship);
    lua_register(L, "Player_AddResource",           l_Player_AddResource);
    lua_register(L, "Player_GetRace",               l_Player_GetRace);
    lua_register(L, "Player_SetUpgradeAvailability", l_Player_SetUpgradeAvailability);
    lua_register(L, "Player_RemoveUpgrade",         l_Player_RemoveUpgrade);
    lua_register(L, "Player_SetEntityProductionAvailability", l_Player_SetEntityProductionAvailability);
}
