#include "mock_blueprint.h"
#include "game_state.h"
#include "lauxlib.h"

#include <string.h>

/* BP_GetEntityBlueprint(name) — returns the name as a string blueprint */
static int l_BP_GetEntityBlueprint(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushstring(L, name);
    return 1;
}

/* BP_GetSquadBlueprint(name) — returns the name as a string blueprint */
static int l_BP_GetSquadBlueprint(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushstring(L, name);
    return 1;
}

/* BP_GetUpgradeBlueprint(name) — returns the name as a string */
static int l_BP_GetUpgradeBlueprint(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushstring(L, name);
    return 1;
}

/* BP_GetAbilityBlueprint(name) — returns the name as a string */
static int l_BP_GetAbilityBlueprint(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushstring(L, name);
    return 1;
}

/* BP_GetName(bp) — blueprints are strings, just return as-is */
static int l_BP_GetName(lua_State* L) {
    const char* bp = luaL_checkstring(L, 1);
    lua_pushstring(L, bp);
    return 1;
}

/* BP_UpgradeExists(name) — always returns true in mock */
static int l_BP_UpgradeExists(lua_State* L) {
    (void)L;
    lua_pushboolean(L, 1);
    return 1;
}

/* BP_EntityExists(name) — always returns true */
static int l_BP_EntityExists(lua_State* L) {
    (void)L;
    lua_pushboolean(L, 1);
    return 1;
}

/* BP_SquadExists(name) — always returns true */
static int l_BP_SquadExists(lua_State* L) {
    (void)L;
    lua_pushboolean(L, 1);
    return 1;
}

/* BP_GetTechTreeBlueprintsByType(type) — returns empty table */
static int l_BP_GetTechTreeBlueprintsByType(lua_State* L) {
    (void)L;
    lua_newtable(L);
    return 1;
}

/* BP_GetTechTreeBPInfo(bp) — returns stub info table */
static int l_BP_GetTechTreeBPInfo(lua_State* L) {
    (void)L;
    lua_createtable(L, 0, 2);
    lua_pushstring(L, "");
    lua_setfield(L, -2, "activation_upgrade");
    lua_pushstring(L, "");
    lua_setfield(L, -2, "name");
    return 1;
}

/* BP_GetSquadUIInfo(sbp, race) — returns stub UI info */
static int l_BP_GetSquadUIInfo(lua_State* L) {
    (void)L;
    lua_createtable(L, 0, 4);
    lua_pushstring(L, ""); lua_setfield(L, -2, "screenName");
    lua_pushstring(L, ""); lua_setfield(L, -2, "briefText");
    lua_pushstring(L, ""); lua_setfield(L, -2, "iconName");
    lua_pushstring(L, ""); lua_setfield(L, -2, "symbolIconName");
    return 1;
}

/* BP_GetUpgradeUIInfo(bp) — returns stub UI info */
static int l_BP_GetUpgradeUIInfo(lua_State* L) {
    (void)L;
    lua_createtable(L, 0, 3);
    lua_pushstring(L, ""); lua_setfield(L, -2, "screenName");
    lua_pushstring(L, ""); lua_setfield(L, -2, "briefText");
    lua_pushstring(L, ""); lua_setfield(L, -2, "symbolIconName");
    return 1;
}

/* EBP_Exists(name) — always true */
static int l_EBP_Exists(lua_State* L) {
    (void)L;
    lua_pushboolean(L, 1);
    return 1;
}

void mock_blueprint_register(lua_State* L) {
    lua_register(L, "BP_GetEntityBlueprint",  l_BP_GetEntityBlueprint);
    lua_register(L, "BP_GetSquadBlueprint",   l_BP_GetSquadBlueprint);
    lua_register(L, "BP_GetUpgradeBlueprint", l_BP_GetUpgradeBlueprint);
    lua_register(L, "BP_GetAbilityBlueprint", l_BP_GetAbilityBlueprint);
    lua_register(L, "BP_GetName",             l_BP_GetName);
    lua_register(L, "BP_UpgradeExists",       l_BP_UpgradeExists);
    lua_register(L, "BP_EntityExists",        l_BP_EntityExists);
    lua_register(L, "BP_SquadExists",                  l_BP_SquadExists);
    lua_register(L, "BP_GetTechTreeBlueprintsByType",   l_BP_GetTechTreeBlueprintsByType);
    lua_register(L, "BP_GetTechTreeBPInfo",             l_BP_GetTechTreeBPInfo);
    lua_register(L, "BP_GetSquadUIInfo",                l_BP_GetSquadUIInfo);
    lua_register(L, "BP_GetUpgradeUIInfo",              l_BP_GetUpgradeUIInfo);
    lua_register(L, "EBP_Exists",                       l_EBP_Exists);
}
