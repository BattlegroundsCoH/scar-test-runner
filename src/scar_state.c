#include "scar_state.h"
#include "lauxlib.h"
#include "lualib.h"

#include "mock_entity.h"
#include "mock_squad.h"
#include "mock_player.h"
#include "mock_egroup.h"
#include "mock_sgroup.h"
#include "mock_game.h"
#include "mock_misc.h"

#include <stdio.h>
#include <string.h>

/* ── import() implementation ─────────────────────────────────────── */

static int l_import(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    /* Get scar_root from registry */
    lua_getfield(L, LUA_REGISTRYINDEX, SCAR_ROOT_REGISTRY_KEY);
    const char* scar_root = lua_tostring(L, -1);
    lua_pop(L, 1);

    if (!scar_root) {
        return luaL_error(L, "import: scar_root not configured");
    }

    /* Build full path: scar_root/path */
    char fullpath[1024];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", scar_root, path);

    /* Normalize path separators */
    for (char* p = fullpath; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    /* Check if already loaded (use a registry table to track imports) */
    lua_getfield(L, LUA_REGISTRYINDEX, "scar_imported_files");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "scar_imported_files");
    }
    lua_getfield(L, -1, fullpath);
    if (lua_toboolean(L, -1)) {
        lua_pop(L, 2); /* pop the value and table */
        return 0; /* already imported */
    }
    lua_pop(L, 1); /* pop nil */

    /* Mark as imported before loading (prevents circular imports) */
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, fullpath);
    lua_pop(L, 1); /* pop imports table */

    /* Load and execute the file */
    if (luaL_dofile(L, fullpath) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        return luaL_error(L, "import('%s'): %s", path, err);
    }

    return 0;
}

/* ── SCAR dofile override ────────────────────────────────────────── */

static int l_scar_dofile(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    /* If path is absolute, use as-is. Otherwise resolve relative to scar_root */
    char fullpath[1024];
#ifdef _WIN32
    int is_absolute = (path[0] != '\0' && path[1] == ':') || path[0] == '\\' || path[0] == '/';
#else
    int is_absolute = (path[0] == '/');
#endif
    if (is_absolute) {
        strncpy(fullpath, path, sizeof(fullpath) - 1);
        fullpath[sizeof(fullpath) - 1] = '\0';
    } else {
        lua_getfield(L, LUA_REGISTRYINDEX, SCAR_ROOT_REGISTRY_KEY);
        const char* scar_root = lua_tostring(L, -1);
        lua_pop(L, 1);
        if (!scar_root) {
            return luaL_error(L, "dofile: scar_root not configured");
        }
        snprintf(fullpath, sizeof(fullpath), "%s/%s", scar_root, path);
    }

    if (luaL_dofile(L, fullpath) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        return luaL_error(L, "dofile('%s'): %s", path, err);
    }
    return lua_gettop(L); /* Return whatever dofile returned */
}

/* ── State creation ──────────────────────────────────────────────── */

lua_State* scar_state_new(const char* scar_root, GameState* gs) {
    lua_State* L = luaL_newstate();
    if (!L) return NULL;

    luaL_openlibs(L);

    /* Store scar_root in registry */
    lua_pushstring(L, scar_root);
    lua_setfield(L, LUA_REGISTRYINDEX, SCAR_ROOT_REGISTRY_KEY);

    /* Store game state in registry */
    game_state_store(L, gs);

    /* Override import and dofile */
    lua_register(L, "import", l_import);
    lua_register(L, "dofile",  l_scar_dofile);

    /* Register all mock modules */
    mock_entity_register(L);
    mock_squad_register(L);
    mock_player_register(L);
    mock_egroup_register(L);
    mock_sgroup_register(L);
    mock_game_register(L);
    mock_misc_register(L);

    return L;
}

void scar_state_destroy(lua_State* L) {
    if (L) {
        lua_close(L);
    }
}

const char* scar_state_get_root(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, SCAR_ROOT_REGISTRY_KEY);
    const char* root = lua_tostring(L, -1);
    lua_pop(L, 1);
    return root;
}
