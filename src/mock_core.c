#include "mock_core.h"
#include "game_state.h"
#include "lauxlib.h"

#include <string.h>
#include <stdio.h>

/* ── Core_RegisterModule(name) ───────────────────────────────────── */

static int l_Core_RegisterModule(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    GameState* gs = game_state_from_lua(L);
    if (gs->core_module_count >= MAX_CORE_MODULES) {
        return luaL_error(L, "Core_RegisterModule: maximum module count reached");
    }
    /* Avoid duplicates */
    for (int i = 0; i < gs->core_module_count; i++) {
        if (strcmp(gs->core_modules[i].name, name) == 0) {
            return 0;
        }
    }
    strncpy(gs->core_modules[gs->core_module_count].name, name, MAX_BLUEPRINT_LEN - 1);
    gs->core_module_count++;
    return 0;
}

/* ── Core_CallDelegateFunctions(delegateName, ...) ───────────────── */

static int l_Core_CallDelegateFunctions(lua_State* L) {
    const char* delegate = luaL_checkstring(L, 1);
    int nargs = lua_gettop(L) - 1; /* args after the delegate name */
    GameState* gs = game_state_from_lua(L);

    for (int i = 0; i < gs->core_module_count; i++) {
        /* Build function name: ModuleName_DelegateName */
        char func_name[256];
        snprintf(func_name, sizeof(func_name), "%s_%s",
                 gs->core_modules[i].name, delegate);

        lua_getglobal(L, func_name);
        if (lua_isfunction(L, -1)) {
            /* Push copies of all variadic arguments */
            for (int a = 0; a < nargs; a++) {
                lua_pushvalue(L, 2 + a);
            }
            if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                fprintf(stderr, "Core_CallDelegateFunctions: error in %s: %s\n",
                        func_name, err);
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }
    }
    return 0;
}

/* ── Core_GetPlayersTableEntry(player) ───────────────────────────── */

static int l_Core_GetPlayersTableEntry(lua_State* L) {
    int player_id = check_player_id(L, 1);
    GameState* gs = game_state_from_lua(L);
    MockPlayer* p = game_state_get_player(gs, player_id);
    if (!p) {
        return luaL_error(L, "Core_GetPlayersTableEntry: player %d not found", player_id);
    }

    /* Return a table: { id = player_id, player = Player userdata, scarModel = {...} } */
    lua_createtable(L, 0, 3);

    lua_pushinteger(L, player_id);
    lua_setfield(L, -2, "id");

    push_player(L, player_id);
    lua_setfield(L, -2, "player");

    /* scarModel subtable with UI flags (all default to true/false as needed) */
    lua_createtable(L, 0, 6);
    lua_pushboolean(L, 0); lua_setfield(L, -2, "hideMiniMap");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "hideCommandCard");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "hideGUC");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "hidePlayerAbilityPanel");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "hideBattleGroup");
    lua_setfield(L, -2, "scarModel");

    return 1;
}

/* ── Core_OnGameOver() ───────────────────────────────────────────── */

static int l_Core_OnGameOver(lua_State* L) {
    /* Call OnWinConditionTriggered and OnGameOver delegates */
    int nargs = lua_gettop(L);

    /* OnWinConditionTriggered with any args passed */
    lua_pushcfunction(L, l_Core_CallDelegateFunctions);
    lua_pushstring(L, "OnWinConditionTriggered");
    for (int i = 1; i <= nargs; i++) {
        lua_pushvalue(L, i);
    }
    lua_call(L, 1 + nargs, 0);

    /* OnGameOver */
    lua_pushcfunction(L, l_Core_CallDelegateFunctions);
    lua_pushstring(L, "OnGameOver");
    for (int i = 1; i <= nargs; i++) {
        lua_pushvalue(L, i);
    }
    lua_call(L, 1 + nargs, 0);

    return 0;
}

void mock_core_register(lua_State* L) {
    lua_register(L, "Core_RegisterModule",          l_Core_RegisterModule);
    lua_register(L, "Core_CallDelegateFunctions",   l_Core_CallDelegateFunctions);
    lua_register(L, "Core_GetPlayersTableEntry",    l_Core_GetPlayersTableEntry);
    lua_register(L, "Core_OnGameOver",              l_Core_OnGameOver);
}
