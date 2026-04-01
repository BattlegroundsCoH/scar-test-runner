#include "mock_territory.h"
#include "game_state.h"
#include "lauxlib.h"

#include <math.h>

/* Territory_GetSectorContainingPoint(pos) — find sector whose position is closest */
static int l_Territory_GetSectorContainingPoint(lua_State* L) {
    Position pos = read_position(L, 1);
    GameState* gs = game_state_from_lua(L);

    if (gs->territory_count == 0) {
        lua_pushinteger(L, -1);
        return 1;
    }

    int best = 0;
    float best_dist = 1e30f;
    for (int i = 0; i < gs->territory_count; i++) {
        float dx = gs->territories[i].position.x - pos.x;
        float dz = gs->territories[i].position.z - pos.z;
        float d = dx * dx + dz * dz;
        if (d < best_dist) {
            best_dist = d;
            best = i;
        }
    }
    lua_pushinteger(L, gs->territories[best].sector_id);
    return 1;
}

/* Territory_GetSectorOwnerID(sector_id) — returns player_id or -1 */
static int l_Territory_GetSectorOwnerID(lua_State* L) {
    int sector_id = (int)luaL_checkinteger(L, 1);
    GameState* gs = game_state_from_lua(L);

    for (int i = 0; i < gs->territory_count; i++) {
        if (gs->territories[i].sector_id == sector_id) {
            lua_pushinteger(L, gs->territories[i].owner_player_id);
            return 1;
        }
    }
    lua_pushinteger(L, -1);
    return 1;
}

/* Territory_SetSectorOwner(sector_id, player) — mock helper */
static int l_Territory_SetSectorOwner(lua_State* L) {
    int sector_id = (int)luaL_checkinteger(L, 1);
    int player_id = check_player_id(L, 2);
    GameState* gs = game_state_from_lua(L);

    for (int i = 0; i < gs->territory_count; i++) {
        if (gs->territories[i].sector_id == sector_id) {
            gs->territories[i].owner_player_id = player_id;
            return 0;
        }
    }
    return luaL_error(L, "Territory_SetSectorOwner: sector %d not found", sector_id);
}

void mock_territory_register(lua_State* L) {
    lua_register(L, "Territory_GetSectorContainingPoint", l_Territory_GetSectorContainingPoint);
    lua_register(L, "Territory_GetSectorOwnerID",         l_Territory_GetSectorOwnerID);
    lua_register(L, "Territory_SetSectorOwner",           l_Territory_SetSectorOwner);
}
