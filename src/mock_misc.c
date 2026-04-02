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

/* Loc_ToAnsi — identity, returns string as-is */
static int l_Loc_ToAnsi(lua_State* L) {
    if (lua_gettop(L) >= 1) {
        lua_pushvalue(L, 1);
    } else {
        lua_pushstring(L, "");
    }
    return 1;
}

/* scartype_tostring — converts value to a string description */
static int l_scartype_tostring(lua_State* L) {
    const char* s = luaL_tolstring(L, 1, NULL);
    lua_pushstring(L, s ? s : "");
    return 1;
}

/* Table_Contains(table, value) — checks if table contains value */
static int l_Table_Contains(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        if (lua_rawequal(L, -1, 2)) {
            lua_pushboolean(L, 1);
            return 1;
        }
        lua_pop(L, 1); /* pop value, keep key */
    }
    lua_pushboolean(L, 0);
    return 1;
}

/* Util_GetPosition — gets position from entity, squad, or pos table */
static int l_Util_GetPosition(lua_State* L) {
    /* Check if it's already a position table */
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "x");
        if (!lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_pushvalue(L, 1);
            return 1;
        }
        lua_pop(L, 1);
    }
    /* Try as Entity userdata */
    if (luaL_testudata(L, 1, SCAR_ENTITY_MT)) {
        int id = check_entity_id(L, 1);
        GameState* gs = game_state_from_lua(L);
        MockEntity* e = game_state_get_entity(gs, id);
        if (e) { push_position(L, e->position); return 1; }
    }
    /* Try as Squad userdata */
    if (luaL_testudata(L, 1, SCAR_SQUAD_MT)) {
        int id = check_squad_id(L, 1);
        GameState* gs = game_state_from_lua(L);
        MockSquad* s = game_state_get_squad(gs, id);
        if (s) { push_position(L, s->position); return 1; }
    }
    /* Fallback: return origin */
    Position origin = {0, 0, 0};
    push_position(L, origin);
    return 1;
}

/* Util_CreateSquads — simplified: create single squad, return in SGroup */
static int l_Util_CreateSquads(lua_State* L) {
    /* Util_CreateSquads(player, sgroup_name, sbp, spawn_pos, dest_pos, count, ...) */
    int player_id = check_player_id(L, 1);
    /* arg 2: sgroup name or sgroup */
    const char* sbp = luaL_checkstring(L, 3);
    /* arg 4: spawn position */
    /* arg 5: dest position (optional) */
    int count = (int)luaL_optinteger(L, 6, 1);

    GameState* gs = game_state_from_lua(L);
    Position spawn_pos = {0, 0, 0};
    if (lua_gettop(L) >= 4 && lua_istable(L, 4)) {
        spawn_pos = read_position(L, 4);
    }

    /* Create the squads */
    for (int i = 0; i < count; i++) {
        int sid = ++gs->next_auto_entity_id + 20000;
        MockSquad* s = game_state_add_squad(gs, sid);
        if (!s) break;
        strncpy(s->blueprint, sbp, MAX_BLUEPRINT_LEN - 1);
        s->player_owner_id = player_id;
        s->position = spawn_pos;
    }

    return 0;
}

/* Game_GetScenarioFileName — returns a mock scenario name */
static int l_Game_GetScenarioFileName(lua_State* L) {
    lua_pushstring(L, "mock_scenario");
    return 1;
}

/* Command_PlayerBroadcastMessage — no-op */
static int l_Command_PlayerBroadcastMessage(lua_State* L) {
    (void)L;
    return 0;
}

/* Modify_PlayerResourceCap — no-op */
static int l_Modify_PlayerResourceCap(lua_State* L) {
    (void)L;
    return 0;
}

/* noop for bool return as table */
static int l_noop_empty_table(lua_State* L) {
    (void)L;
    lua_newtable(L);
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

    /* Global events */
    lua_pushinteger(L, 100); lua_setglobal(L, "GE_SquadKilled");
    lua_pushinteger(L, 101); lua_setglobal(L, "GE_EntityKilled");
    lua_pushinteger(L, 102); lua_setglobal(L, "GE_StrategicPointChanged");
    lua_pushinteger(L, 103); lua_setglobal(L, "GE_PlayerTeamChanged");
    lua_pushinteger(L, 104); lua_setglobal(L, "GE_DamageReceived");
    lua_pushinteger(L, 105); lua_setglobal(L, "GE_BuildItemComplete");
    lua_pushinteger(L, 106); lua_setglobal(L, "GE_UpgradeComplete");
    lua_pushinteger(L, 107); lua_setglobal(L, "GE_AbilityExecuted");
    lua_pushinteger(L, 108); lua_setglobal(L, "GE_ProjectileFired");
    lua_pushinteger(L, 109); lua_setglobal(L, "GE_ConstructionComplete");
    lua_pushinteger(L, 110); lua_setglobal(L, "GE_PlayerSurrendered");
    lua_pushinteger(L, 111); lua_setglobal(L, "GE_PlayerDisconnected");
    lua_pushinteger(L, 112); lua_setglobal(L, "GE_BattlegroupAbilityActivated");

    /* Relationship constants */
    lua_pushinteger(L, 0); lua_setglobal(L, "R_ENEMY");
    lua_pushinteger(L, 1); lua_setglobal(L, "R_ALLY");
    lua_pushinteger(L, 2); lua_setglobal(L, "R_NEUTRAL");

    /* Win reason */
    lua_pushinteger(L, 0); lua_setglobal(L, "WR_NONE");
    lua_pushinteger(L, 1); lua_setglobal(L, "WR_ANNIHILATION");
    lua_pushinteger(L, 2); lua_setglobal(L, "WR_SURRENDER");
    lua_pushinteger(L, 3); lua_setglobal(L, "WR_VP");

    /* Additional global events */
    lua_pushinteger(L, 113); lua_setglobal(L, "GE_BroadcastMessage");
    lua_pushinteger(L, 114); lua_setglobal(L, "GE_SquadItemPickup");
    lua_pushinteger(L, 115); lua_setglobal(L, "GE_EntityRecrewed");

    /* Additional resource types */
    lua_pushinteger(L, 3); lua_setglobal(L, "RT_Action");
    lua_pushinteger(L, 4); lua_setglobal(L, "RT_Command");

    /* Modifier usage types */
    lua_pushinteger(L, 1); lua_setglobal(L, "MUT_Multiplication");

    /* Availability states */
    lua_pushinteger(L, 0); lua_setglobal(L, "ITEM_REMOVED");
    lua_pushinteger(L, 1); lua_setglobal(L, "ITEM_LOCKED");

    /* SCAR type for position */
    lua_pushinteger(L, 6); lua_setglobal(L, "ST_SCARPOS");

}

void mock_misc_register(lua_State* L) {
    lua_register(L, "Loc_FormatText",  l_Loc_FormatText);
    lua_register(L, "Loc_Empty",       l_Loc_Empty);
    lua_register(L, "Loc_ToAnsi",      l_Loc_ToAnsi);
    lua_register(L, "scartype",        l_scartype);
    lua_register(L, "scartype_tostring", l_scartype_tostring);
    lua_register(L, "Misc_IsDevMode",  l_Misc_IsDevMode);
    lua_register(L, "Misc_IsSteamMode", l_Misc_IsSteamMode);
    lua_register(L, "fatal",           l_fatal);
    lua_register(L, "Table_Contains",  l_Table_Contains);
    lua_register(L, "Util_GetPosition", l_Util_GetPosition);
    lua_register(L, "Util_CreateSquads", l_Util_CreateSquads);
    lua_register(L, "Game_GetScenarioFileName", l_Game_GetScenarioFileName);
    lua_register(L, "Command_PlayerBroadcastMessage", l_Command_PlayerBroadcastMessage);
    lua_register(L, "Modify_PlayerResourceCap", l_Modify_PlayerResourceCap);

    /* Common no-ops */
    lua_register(L, "Bug_ReportBug",    l_noop);
    lua_register(L, "Network_CallEvent", l_noop);
    lua_register(L, "EventCues_Register", l_noop);
    lua_register(L, "Sound_Play2D",     l_noop);
    lua_register(L, "Sound_Play3D",     l_noop);
    lua_register(L, "Camera_MoveTo",    l_noop);
    lua_register(L, "FOW_RevealAll",    l_noop);
    lua_register(L, "FOW_UnRevealAll",  l_noop);
    lua_register(L, "Cmd_Ability",      l_noop);
    lua_register(L, "Cmd_Move",         l_noop);
    lua_register(L, "Cmd_Attack",       l_noop);
    lua_register(L, "Cmd_Stop",         l_noop);

    /* No-ops with return values */
    lua_register(L, "Misc_IsCommandLineOptionSet", l_noop_false);
    lua_register(L, "World_IsCampaignMetamapGame", l_noop_false);
    lua_register(L, "Timer_GetRemaining", l_noop_zero);
    lua_register(L, "Loc_GetString",     l_noop_empty_string);

    /* Additional no-ops for Battlegrounds support */
    lua_register(L, "UI_CreateEventCue",        l_noop);
    lua_register(L, "EventCues_Trigger",        l_noop);
    lua_register(L, "Camera_FocusOnPosition",   l_noop);
    lua_register(L, "Camera_SetDefault",        l_noop);
    lua_register(L, "AI_Enable",                l_noop);
    lua_register(L, "AI_Disable",               l_noop);
    lua_register(L, "AI_LockAll",               l_noop);
    lua_register(L, "AI_UnlockAll",             l_noop);
    lua_register(L, "AI_SetDifficulty",         l_noop);
    lua_register(L, "AI_SetPersonality",        l_noop);
    lua_register(L, "Cmd_Garrison",             l_noop);
    lua_register(L, "Cmd_AttackMove",           l_noop);
    lua_register(L, "Cmd_Retreat",              l_noop);
    lua_register(L, "Cmd_ReinforceUnit",        l_noop);
    lua_register(L, "Modifier_Create",          l_noop_zero);
    lua_register(L, "Modifier_Apply",           l_noop);
    lua_register(L, "Modifier_Remove",          l_noop);
    lua_register(L, "Modifier_Enable",          l_noop);
    lua_register(L, "Modifier_Disable",         l_noop);
    lua_register(L, "Modify_PlayerResourceRate", l_noop);
    lua_register(L, "Player_SetPopCapOverride", l_noop);
    lua_register(L, "Player_ResetPopCapOverride", l_noop);
    lua_register(L, "Player_CompleteUpgrade",   l_noop);
    lua_register(L, "Player_HasUpgrade",        l_noop_false);
    lua_register(L, "Game_GetSPDifficulty",     l_noop_zero);
    lua_register(L, "Game_SetGameRestoreAvailability", l_noop);
    lua_register(L, "Game_GetSimRate",          l_noop_zero);
    lua_register(L, "Misc_DoString",            l_noop);
    lua_register(L, "Timer_Start",              l_noop);
    lua_register(L, "Timer_End",                l_noop);
    lua_register(L, "Timer_Exists",             l_noop_false);
    lua_register(L, "Timer_Pause",              l_noop);
    lua_register(L, "Timer_Resume",             l_noop);
    lua_register(L, "Obj_Create",               l_noop_zero);
    lua_register(L, "Obj_SetState",             l_noop);
    lua_register(L, "Obj_Delete",               l_noop);
    lua_register(L, "Obj_SetVisible",           l_noop);
    lua_register(L, "Obj_SetIcon",              l_noop);
    lua_register(L, "Obj_SetDescription",       l_noop);
    lua_register(L, "SGroup_EnableUIDecorator", l_noop);
    lua_register(L, "EGroup_EnableUIDecorator", l_noop);
    lua_register(L, "Music_SetIntensity",       l_noop);
    lua_register(L, "Music_LockIntensity",      l_noop);
    lua_register(L, "Music_UnlockIntensity",    l_noop);
    lua_register(L, "Marker_GetPosition",       l_noop_zero);
    lua_register(L, "Marker_Exists",            l_noop_false);
    lua_register(L, "TicketUI_SetVisibility",   l_noop);
    lua_register(L, "UI_SetPlayerDataContext",  l_noop);
    lua_register(L, "UI_CreateCommand",         l_noop);
    lua_register(L, "UI_SetEnablePauseMenu",    l_noop);
    lua_register(L, "UI_ToggleDecorators",      l_noop);
    lua_register(L, "Game_TextTitleFade",        l_noop);
    lua_register(L, "Cmd_InstantUpgrade",       l_noop);
    lua_register(L, "Cmd_EjectOccupants",       l_noop);
    lua_register(L, "Cmd_MoveToAndDestroy",     l_noop);
    lua_register(L, "AI_LockSquads",            l_noop);
    lua_register(L, "Misc_ClearSelection",      l_noop);
    lua_register(L, "Misc_SetSelectionInputEnabled",    l_noop);
    lua_register(L, "Misc_SetDefaultCommandsEnabled",   l_noop);
    lua_register(L, "Camera_SetInputEnabled",   l_noop);

    register_constants(L);
}
