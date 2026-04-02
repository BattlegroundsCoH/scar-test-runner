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
#include "mock_core.h"
#include "mock_blueprint.h"
#include "mock_territory.h"
#include "mock_ui.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ── File existence check ────────────────────────────────────────── */

static int file_exists(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
#endif
}

/* Try to resolve a scar import path. Returns 1 and writes to resolved if found. */
static int resolve_scar_path(lua_State* L, const char* path, char* resolved, size_t resolved_sz) {
    /* Try scar_root first */
    lua_getfield(L, LUA_REGISTRYINDEX, SCAR_ROOT_REGISTRY_KEY);
    const char* scar_root = lua_tostring(L, -1);
    lua_pop(L, 1);

    if (scar_root) {
        snprintf(resolved, resolved_sz, "%s/%s", scar_root, path);
        for (char* p = resolved; *p; p++) { if (*p == '\\') *p = '/'; }
        if (file_exists(resolved)) return 1;
    }

    /* Fallback to scar_data */
    lua_getfield(L, LUA_REGISTRYINDEX, SCAR_DATA_REGISTRY_KEY);
    const char* scar_data = lua_tostring(L, -1);
    lua_pop(L, 1);

    if (scar_data) {
        snprintf(resolved, resolved_sz, "%s/%s", scar_data, path);
        for (char* p = resolved; *p; p++) { if (*p == '\\') *p = '/'; }
        if (file_exists(resolved)) return 1;
    }

    /* Not found — fall back to scar_root path for the error message */
    if (scar_root) {
        snprintf(resolved, resolved_sz, "%s/%s", scar_root, path);
        for (char* p = resolved; *p; p++) { if (*p == '\\') *p = '/'; }
    }
    return 0;
}

/* ── Built-in engine stubs ────────────────────────────────────────
 * These paths are known CoH3 engine .scar files whose functions are
 * already provided by the C mock layer.  When an import() cannot be
 * resolved on disk, any path listed here is silently skipped instead
 * of producing an error.  This removes the need for consumers to
 * ship or reference copies of the copyrighted engine scripts.
 * ─────────────────────────────────────────────────────────────────── */

static const char* const builtin_stubs[] = {
    "scarutil.scar",
    "core.scar",
    "game_modifiers.scar",
    "ui/ticket_ui.scar",
    "audio/audio.scar",
    "ai/ai_match_observer.scar",
    NULL
};

static int is_builtin_stub(const char* path) {
    /* Normalize separators for comparison */
    char norm[256];
    size_t len = strlen(path);
    if (len >= sizeof(norm)) return 0;
    for (size_t i = 0; i <= len; i++)
        norm[i] = (path[i] == '\\') ? '/' : path[i];

    for (const char* const* s = builtin_stubs; *s; s++) {
        if (strcmp(norm, *s) == 0) return 1;
    }
    return 0;
}

/* ── import() implementation ─────────────────────────────────────── */

static int l_import(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    /* Resolve the import path (scar_root first, then scar_data) */
    char fullpath[1024];
    int found = resolve_scar_path(L, path, fullpath, sizeof(fullpath));

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

    /* If the file wasn't found on disk, check built-in stubs */
    if (!found) {
        if (is_builtin_stub(path)) {
            return 0; /* silently skip — C mocks provide everything */
        }
        return luaL_error(L, "import('%s'): file not found", path);
    }

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
        resolve_scar_path(L, path, fullpath, sizeof(fullpath));
    }

    if (luaL_dofile(L, fullpath) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        return luaL_error(L, "dofile('%s'): %s", path, err);
    }
    return lua_gettop(L); /* Return whatever dofile returned */
}

/* ── State creation ──────────────────────────────────────────────── */

lua_State* scar_state_new(const char* scar_root, const char* scar_data, GameState* gs) {
    lua_State* L = luaL_newstate();
    if (!L) return NULL;

    luaL_openlibs(L);

    /* Register SCAR type metatables */
    scar_types_register(L);

    /* Store scar_root in registry */
    lua_pushstring(L, scar_root);
    lua_setfield(L, LUA_REGISTRYINDEX, SCAR_ROOT_REGISTRY_KEY);

    /* Store scar_data in registry (if provided) */
    if (scar_data) {
        lua_pushstring(L, scar_data);
        lua_setfield(L, LUA_REGISTRYINDEX, SCAR_DATA_REGISTRY_KEY);
    }

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
    mock_core_register(L);
    mock_blueprint_register(L);
    mock_territory_register(L);
    mock_ui_register(L);

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
