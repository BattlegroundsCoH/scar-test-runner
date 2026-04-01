#ifndef SCAR_STATE_H
#define SCAR_STATE_H

#include "lua.h"
#include "game_state.h"

/* Registry key for scar_root path */
#define SCAR_ROOT_REGISTRY_KEY "scar_test_scar_root"

/* Create a fully configured lua_State with all mocks registered.
   The caller must eventually call scar_state_destroy(). */
lua_State* scar_state_new(const char* scar_root, GameState* gs);

/* Close the lua_State */
void scar_state_destroy(lua_State* L);

/* Get the scar_root from the lua_State registry */
const char* scar_state_get_root(lua_State* L);

#endif /* SCAR_STATE_H */
