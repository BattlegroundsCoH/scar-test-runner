#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "lua.h"

#include <stdbool.h>

/* Registry key to store GameState pointer */
#define GAME_STATE_REGISTRY_KEY "scar_test_game_state"

/* Maximum capacities for dynamic arrays */
#define MAX_ENTITIES   1024
#define MAX_SQUADS     256
#define MAX_PLAYERS    16
#define MAX_EGROUPS    128
#define MAX_SGROUPS    128
#define MAX_INIT_FUNCS 64
#define MAX_RULES      128
#define MAX_GROUP_ITEMS 256
#define MAX_SQUAD_ENTITIES 32
#define MAX_BLUEPRINT_LEN 128

/* ── Position ────────────────────────────────────────────────────── */

typedef struct {
    float x, y, z;
} Position;

/* ── Mock Entity ─────────────────────────────────────────────────── */

typedef struct {
    int   id;
    float health;
    float health_max;
    Position position;
    int   player_owner_id;
    char  blueprint[MAX_BLUEPRINT_LEN];
    bool  invulnerable;
    bool  alive;
} MockEntity;

/* ── Mock Squad ──────────────────────────────────────────────────── */

typedef struct {
    int   id;
    int   entity_ids[MAX_SQUAD_ENTITIES];
    int   entity_count;
    Position position;
    int   player_owner_id;
    char  blueprint[MAX_BLUEPRINT_LEN];
    bool  invulnerable;
    bool  alive;
} MockSquad;

/* ── Mock Player ─────────────────────────────────────────────────── */

typedef struct {
    float manpower;
    float fuel;
    float munitions;
} PlayerResources;

typedef struct {
    int   id;
    int   team_id;
    PlayerResources resources;
    int   population_cap;
    int   population_current;
} MockPlayer;

/* ── Mock EGroup ─────────────────────────────────────────────────── */

typedef struct {
    char  name[MAX_BLUEPRINT_LEN];
    int   entity_ids[MAX_GROUP_ITEMS];
    int   count;
} MockEGroup;

/* ── Mock SGroup ─────────────────────────────────────────────────── */

typedef struct {
    char  name[MAX_BLUEPRINT_LEN];
    int   squad_ids[MAX_GROUP_ITEMS];
    int   count;
} MockSGroup;

/* ── Rule types ──────────────────────────────────────────────────── */

typedef enum {
    RULE_INTERVAL,
    RULE_ONESHOT
} RuleType;

typedef struct {
    int      lua_func_ref;
    RuleType type;
    float    interval;
    float    elapsed;
    bool     active;
} MockRule;

/* ── Game State ──────────────────────────────────────────────────── */

typedef struct {
    MockEntity  entities[MAX_ENTITIES];
    int         entity_count;

    MockSquad   squads[MAX_SQUADS];
    int         squad_count;

    MockPlayer  players[MAX_PLAYERS];
    int         player_count;

    MockEGroup  egroups[MAX_EGROUPS];
    int         egroup_count;

    MockSGroup  sgroups[MAX_SGROUPS];
    int         sgroup_count;

    /* Game lifecycle */
    int         init_func_refs[MAX_INIT_FUNCS];
    int         init_func_count;

    MockRule    rules[MAX_RULES];
    int         rule_count;

    float       game_time;
} GameState;

/* ── API ─────────────────────────────────────────────────────────── */

GameState*  game_state_create(void);
void        game_state_destroy(GameState* gs);
void        game_state_reset(GameState* gs, lua_State* L);

/* Store/retrieve from Lua registry */
void        game_state_store(lua_State* L, GameState* gs);
GameState*  game_state_from_lua(lua_State* L);

/* Lookups (return NULL if not found) */
MockEntity* game_state_get_entity(GameState* gs, int id);
MockSquad*  game_state_get_squad(GameState* gs, int id);
MockPlayer* game_state_get_player(GameState* gs, int id);
MockEGroup* game_state_get_egroup(GameState* gs, const char* name);
MockSGroup* game_state_get_sgroup(GameState* gs, const char* name);

/* Additions (return pointer to new item, or NULL if full) */
MockEntity* game_state_add_entity(GameState* gs, int id);
MockSquad*  game_state_add_squad(GameState* gs, int id);
MockPlayer* game_state_add_player(GameState* gs, int id);
MockEGroup* game_state_add_egroup(GameState* gs, const char* name);
MockSGroup* game_state_add_sgroup(GameState* gs, const char* name);

/* Helper: push Position table onto Lua stack */
void        push_position(lua_State* L, Position pos);
/* Helper: read Position from Lua table at given stack index */
Position    read_position(lua_State* L, int idx);

/* ── Typed userdata for SCAR handles ─────────────────────────────── */

#define SCAR_ENTITY_MT "ScarEntity"
#define SCAR_SQUAD_MT  "ScarSquad"
#define SCAR_PLAYER_MT "ScarPlayer"
#define SCAR_EGROUP_MT "ScarEGroup"
#define SCAR_SGROUP_MT "ScarSGroup"

typedef struct { int id; } EntityUD;
typedef struct { int id; } SquadUD;
typedef struct { int id; } PlayerUD;
typedef struct { char name[MAX_BLUEPRINT_LEN]; } EGroupUD;
typedef struct { char name[MAX_BLUEPRINT_LEN]; } SGroupUD;

/* Register metatables for all SCAR types (call once per lua_State) */
void scar_types_register(lua_State* L);

/* Push typed userdata onto the Lua stack */
void push_entity(lua_State* L, int id);
void push_squad(lua_State* L, int id);
void push_player(lua_State* L, int id);
void push_egroup(lua_State* L, const char* name);
void push_sgroup(lua_State* L, const char* name);

/* Check and extract typed userdata from the Lua stack (raises error on type mismatch) */
int         check_entity_id(lua_State* L, int idx);
int         check_squad_id(lua_State* L, int idx);
int         check_player_id(lua_State* L, int idx);
const char* check_egroup_name(lua_State* L, int idx);
const char* check_sgroup_name(lua_State* L, int idx);

#endif /* GAME_STATE_H */
