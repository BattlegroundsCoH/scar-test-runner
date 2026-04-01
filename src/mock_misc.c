#include "mock_misc.h"
#include "game_state.h"
#include "lauxlib.h"
#include "lualib.h"

#include <string.h>

/* Loc_FormatText — returns the format string or first argument as-is */
static int l_Loc_FormatText(lua_State* L) {
    if (lua_gettop(L) == 0) {
        lua_pushstring(L, "");
    } else {
        /* Just return the first argument as a string */
        lua_pushvalue(L, 1);
    }
    return 1;
}

/* Loc_Empty — returns empty locstring */
static int l_Loc_Empty(lua_State* L) {
    lua_pushstring(L, "");
    return 1;
}

/* scartype — returns a SCAR type string for the value */
static int l_scartype(lua_State* L) {
    int t = lua_type(L, 1);
    switch (t) {
        case LUA_TNIL:      lua_pushstring(L, "nil"); break;
        case LUA_TBOOLEAN:  lua_pushstring(L, "boolean"); break;
        case LUA_TNUMBER:   lua_pushstring(L, "number"); break;
        case LUA_TSTRING:   lua_pushstring(L, "string"); break;
        case LUA_TTABLE:    lua_pushstring(L, "table"); break;
        case LUA_TFUNCTION: lua_pushstring(L, "function"); break;
        default:            lua_pushstring(L, lua_typename(L, t)); break;
    }
    return 1;
}

/* Misc_IsDevMode — always false in test env */
static int l_Misc_IsDevMode(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

/* Misc_IsSteamMode — always false */
static int l_Misc_IsSteamMode(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

/* fatal — trigger a Lua error (SCAR fatal) */
static int l_fatal(lua_State* L) {
    const char* msg = luaL_optstring(L, 1, "fatal error");
    return luaL_error(L, "%s", msg);
}

/* Bug_ReportBug — no-op in test env */
static int l_noop(lua_State* L) {
    (void)L;
    return 0;
}

/* Returns a reasonable noop for bool — true by default */
static int l_noop_true(lua_State* L) {
    (void)L;
    lua_pushboolean(L, 1);
    return 1;
}

static int l_noop_false(lua_State* L) {
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

static int l_noop_zero(lua_State* L) {
    (void)L;
    lua_pushinteger(L, 0);
    return 1;
}

static int l_noop_empty_string(lua_State* L) {
    (void)L;
    lua_pushstring(L, "");
    return 1;
}

static void register_constants(lua_State* L) {
    /* Resource types */
    lua_pushinteger(L, 0); lua_setglobal(L, "RT_Manpower");
    lua_pushinteger(L, 1); lua_setglobal(L, "RT_Fuel");
    lua_pushinteger(L, 2); lua_setglobal(L, "RT_Munition");

    /* Population cap types */
    lua_pushinteger(L, 0); lua_setglobal(L, "CT_Personnel");
    lua_pushinteger(L, 1); lua_setglobal(L, "CT_Vehicle");

    /* SCAR types */
    lua_pushinteger(L, 0);  lua_setglobal(L, "SCARTYPE_ENTITY");
    lua_pushinteger(L, 1);  lua_setglobal(L, "SCARTYPE_SQUAD");
    lua_pushinteger(L, 2);  lua_setglobal(L, "SCARTYPE_PLAYER");
    lua_pushinteger(L, 3);  lua_setglobal(L, "SCARTYPE_EGROUP");
    lua_pushinteger(L, 4);  lua_setglobal(L, "SCARTYPE_SGROUP");
    lua_pushinteger(L, 5);  lua_setglobal(L, "SCARTYPE_MARKER");

    /* Boolean constants */
    lua_pushboolean(L, 1); lua_setglobal(L, "ANY");
    lua_pushboolean(L, 0); lua_setglobal(L, "ALL");

    /* Game difficulty */
    lua_pushinteger(L, 0); lua_setglobal(L, "GD_EASY");
    lua_pushinteger(L, 1); lua_setglobal(L, "GD_NORMAL");
    lua_pushinteger(L, 2); lua_setglobal(L, "GD_HARD");
    lua_pushinteger(L, 3); lua_setglobal(L, "GD_EXPERT");

    /* Commonly used empty table for TEAMS placeholder */
    lua_newtable(L); lua_setglobal(L, "TEAMS");

    /* Ownership */
    lua_pushinteger(L, 0); lua_setglobal(L, "OT_Player");
    lua_pushinteger(L, 1); lua_setglobal(L, "OT_Ally");
    lua_pushinteger(L, 2); lua_setglobal(L, "OT_Enemy");
    lua_pushinteger(L, 3); lua_setglobal(L, "OT_Neutral");
}

void mock_misc_register(lua_State* L) {
    lua_register(L, "Loc_FormatText",  l_Loc_FormatText);
    lua_register(L, "Loc_Empty",       l_Loc_Empty);
    lua_register(L, "scartype",        l_scartype);
    lua_register(L, "Misc_IsDevMode",  l_Misc_IsDevMode);
    lua_register(L, "Misc_IsSteamMode", l_Misc_IsSteamMode);
    lua_register(L, "fatal",           l_fatal);

    /* Common no-ops */
    lua_register(L, "Bug_ReportBug",    l_noop);
    lua_register(L, "Network_CallEvent", l_noop);
    lua_register(L, "EventCues_Register", l_noop);
    lua_register(L, "Sound_Play2D",     l_noop);
    lua_register(L, "Sound_Play3D",     l_noop);
    lua_register(L, "UI_SetModal",      l_noop);
    lua_register(L, "UI_ClearModal",    l_noop);
    lua_register(L, "Camera_MoveTo",    l_noop);
    lua_register(L, "FOW_RevealAll",    l_noop);
    lua_register(L, "FOW_UnRevealAll",  l_noop);
    lua_register(L, "Cmd_Ability",      l_noop);
    lua_register(L, "Cmd_Move",         l_noop);
    lua_register(L, "Cmd_Attack",       l_noop);
    lua_register(L, "Cmd_Stop",         l_noop);

    /* No-ops with return values */
    lua_register(L, "Misc_IsCommandLineOptionSet", l_noop_false);
    lua_register(L, "World_IsGameOver",  l_noop_false);
    lua_register(L, "World_IsCampaignMetamapGame", l_noop_false);
    lua_register(L, "Timer_GetRemaining", l_noop_zero);
    lua_register(L, "Loc_GetString",     l_noop_empty_string);

    register_constants(L);
}
