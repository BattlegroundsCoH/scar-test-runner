#include "test_api.h"
#include "game_state.h"
#include "lauxlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ── Registry key for TestSuite ──────────────────────────────────── */

#define TEST_SUITE_REGISTRY_KEY "scar_test_suite"

static void test_suite_store(lua_State* L, TestSuite* ts) {
    lua_pushlightuserdata(L, (void*)ts);
    lua_setfield(L, LUA_REGISTRYINDEX, TEST_SUITE_REGISTRY_KEY);
}

static TestSuite* test_suite_from_lua(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, TEST_SUITE_REGISTRY_KEY);
    TestSuite* ts = (TestSuite*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return ts;
}

/* ── High-resolution timer ────────────────────────────────────────── */

static double get_time_ms(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart * 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

/* ── Color output helpers ────────────────────────────────────────── */

#ifdef _WIN32
static void enable_ansi_colors(void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */);
    SetConsoleOutputCP(65001); /* UTF-8 */
}
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN   "\033[36m"
#define COLOR_RESET  "\033[0m"
#define COLOR_DIM    "\033[2m"
#else
#include <time.h>
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN   "\033[36m"
#define COLOR_RESET  "\033[0m"
#define COLOR_DIM    "\033[2m"
static void enable_ansi_colors(void) {}
#endif

/* ── Stylized print() override ────────────────────────────────────── */

static int l_styled_print(lua_State* L) {
    int n = lua_gettop(L);
    printf(COLOR_CYAN "    \xe2\x94\x82 " COLOR_RESET);
    for (int i = 1; i <= n; i++) {
        if (i > 1) printf("\t");
        const char* s = luaL_tolstring(L, i, NULL);
        printf("%s", s);
        lua_pop(L, 1);
    }
    printf("\n");
    return 0;
}

/* ── Build full name from describe chain ─────────────────────────── */

static void build_full_name(TestSuite* ts, int describe_idx, const char* test_name, char* out, int out_size) {
    /* Collect describe names in reverse order */
    const char* parts[MAX_DESCRIBES];
    int part_count = 0;
    int idx = describe_idx;
    while (idx >= 0) {
        parts[part_count++] = ts->describes[idx].name;
        idx = ts->describes[idx].parent_idx;
    }
    /* Build forward: "outermost > ... > innermost > test_name" */
    out[0] = '\0';
    for (int i = part_count - 1; i >= 0; i--) {
        if (out[0] != '\0') {
            strncat(out, " > ", out_size - strlen(out) - 1);
        }
        strncat(out, parts[i], out_size - strlen(out) - 1);
    }
    if (test_name && test_name[0]) {
        strncat(out, " > ", out_size - strlen(out) - 1);
        strncat(out, test_name, out_size - strlen(out) - 1);
    }
}

/* ── BDD collection functions ────────────────────────────────────── */

static int l_describe(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    TestSuite* ts = test_suite_from_lua(L);
    if (ts->describe_count >= MAX_DESCRIBES) {
        return luaL_error(L, "describe: maximum nesting/count reached");
    }

    int idx = ts->describe_count++;
    DescribeBlock* db = &ts->describes[idx];
    strncpy(db->name, name, MAX_TEST_NAME - 1);
    db->name[MAX_TEST_NAME - 1] = '\0';
    db->parent_idx = ts->current_describe;
    db->before_each_count = 0;
    db->after_each_count = 0;

    int prev_describe = ts->current_describe;
    ts->current_describe = idx;

    /* Call the describe body to collect it/before_each/after_each */
    lua_pushvalue(L, 2);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        fprintf(stderr, "Error in describe '%s': %s\n", name, err);
        lua_pop(L, 1);
    }

    ts->current_describe = prev_describe;
    return 0;
}

static int l_it(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    TestSuite* ts = test_suite_from_lua(L);
    if (ts->test_count >= MAX_TESTS) {
        return luaL_error(L, "it: maximum test count reached");
    }

    TestCase* tc = &ts->tests[ts->test_count++];
    build_full_name(ts, ts->current_describe, name, tc->full_name, MAX_TEST_NAME);
    lua_pushvalue(L, 2);
    tc->lua_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    tc->describe_idx = ts->current_describe;
    tc->passed = false;
    tc->ran = false;
    tc->error_msg[0] = '\0';

    return 0;
}

static int l_before_each(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    TestSuite* ts = test_suite_from_lua(L);
    if (ts->current_describe < 0) {
        return luaL_error(L, "before_each: must be called inside describe()");
    }
    DescribeBlock* db = &ts->describes[ts->current_describe];
    if (db->before_each_count >= MAX_HOOKS) {
        return luaL_error(L, "before_each: maximum hooks reached");
    }
    lua_pushvalue(L, 1);
    db->before_each_refs[db->before_each_count++] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static int l_after_each(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    TestSuite* ts = test_suite_from_lua(L);
    if (ts->current_describe < 0) {
        return luaL_error(L, "after_each: must be called inside describe()");
    }
    DescribeBlock* db = &ts->describes[ts->current_describe];
    if (db->after_each_count >= MAX_HOOKS) {
        return luaL_error(L, "after_each: maximum hooks reached");
    }
    lua_pushvalue(L, 1);
    db->after_each_refs[db->after_each_count++] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

/* ── Assertion helpers ───────────────────────────────────────────── */

static int l_assert_eq(lua_State* L) {
    /* assert_eq(expected, actual, msg?) */
    int eq = 0;
    int t1 = lua_type(L, 1);
    int t2 = lua_type(L, 2);
    if (t1 != t2) {
        eq = 0;
    } else if (t1 == LUA_TNUMBER) {
        eq = (lua_tonumber(L, 1) == lua_tonumber(L, 2));
    } else if (t1 == LUA_TSTRING) {
        eq = (strcmp(lua_tostring(L, 1), lua_tostring(L, 2)) == 0);
    } else if (t1 == LUA_TBOOLEAN) {
        eq = (lua_toboolean(L, 1) == lua_toboolean(L, 2));
    } else if (t1 == LUA_TNIL) {
        eq = 1;
    } else {
        eq = lua_rawequal(L, 1, 2);
    }

    if (!eq) {
        const char* msg = luaL_optstring(L, 3, NULL);
        /* Format expected vs actual */
        const char* exp_s = luaL_tolstring(L, 1, NULL);
        const char* act_s = luaL_tolstring(L, 2, NULL);
        if (msg) {
            luaL_error(L, "assert_eq failed: %s\n  Expected: %s\n  Actual:   %s", msg, exp_s, act_s);
        } else {
            luaL_error(L, "assert_eq failed\n  Expected: %s\n  Actual:   %s", exp_s, act_s);
        }
    }
    return 0;
}

static int l_assert_ne(lua_State* L) {
    int t1 = lua_type(L, 1);
    int t2 = lua_type(L, 2);
    int eq = 0;
    if (t1 == t2) {
        if (t1 == LUA_TNUMBER)
            eq = (lua_tonumber(L, 1) == lua_tonumber(L, 2));
        else if (t1 == LUA_TSTRING)
            eq = (strcmp(lua_tostring(L, 1), lua_tostring(L, 2)) == 0);
        else if (t1 == LUA_TBOOLEAN)
            eq = (lua_toboolean(L, 1) == lua_toboolean(L, 2));
        else if (t1 == LUA_TNIL)
            eq = 1;
        else
            eq = lua_rawequal(L, 1, 2);
    }
    if (eq) {
        const char* msg = luaL_optstring(L, 3, "assert_ne failed: values are equal");
        return luaL_error(L, "%s", msg);
    }
    return 0;
}

static int l_assert_true(lua_State* L) {
    if (!lua_toboolean(L, 1)) {
        const char* msg = luaL_optstring(L, 2, "assert_true failed: value is falsy");
        return luaL_error(L, "%s", msg);
    }
    return 0;
}

static int l_assert_false(lua_State* L) {
    if (lua_toboolean(L, 1)) {
        const char* msg = luaL_optstring(L, 2, "assert_false failed: value is truthy");
        return luaL_error(L, "%s", msg);
    }
    return 0;
}

static int l_assert_nil(lua_State* L) {
    if (!lua_isnil(L, 1)) {
        const char* msg = luaL_optstring(L, 2, "assert_nil failed: value is not nil");
        return luaL_error(L, "%s", msg);
    }
    return 0;
}

static int l_assert_not_nil(lua_State* L) {
    if (lua_isnil(L, 1)) {
        const char* msg = luaL_optstring(L, 2, "assert_not_nil failed: value is nil");
        return luaL_error(L, "%s", msg);
    }
    return 0;
}

static int l_assert_error(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int status = lua_pcall(L, 0, 0, 0);
    if (status == LUA_OK) {
        const char* msg = luaL_optstring(L, 2, "assert_error failed: function did not raise an error");
        return luaL_error(L, "%s", msg);
    }
    lua_pop(L, 1); /* pop error message */
    return 0;
}

static int l_assert_gt(lua_State* L) {
    lua_Number a = luaL_checknumber(L, 1);
    lua_Number b = luaL_checknumber(L, 2);
    if (!(a > b)) {
        const char* msg = luaL_optstring(L, 3, NULL);
        if (msg) {
            return luaL_error(L, "assert_gt failed: %s (got %f <= %f)", msg, a, b);
        }
        return luaL_error(L, "assert_gt failed: %f is not > %f", a, b);
    }
    return 0;
}

static int l_assert_gte(lua_State* L) {
    lua_Number a = luaL_checknumber(L, 1);
    lua_Number b = luaL_checknumber(L, 2);
    if (!(a >= b)) {
        const char* msg = luaL_optstring(L, 3, NULL);
        if (msg) {
            return luaL_error(L, "assert_gte failed: %s (got %f < %f)", msg, a, b);
        }
        return luaL_error(L, "assert_gte failed: %f is not >= %f", a, b);
    }
    return 0;
}

static int l_assert_lt(lua_State* L) {
    lua_Number a = luaL_checknumber(L, 1);
    lua_Number b = luaL_checknumber(L, 2);
    if (!(a < b)) {
        const char* msg = luaL_optstring(L, 3, NULL);
        if (msg) {
            return luaL_error(L, "assert_lt failed: %s (got %f >= %f)", msg, a, b);
        }
        return luaL_error(L, "assert_lt failed: %f is not < %f", a, b);
    }
    return 0;
}

static int l_assert_lte(lua_State* L) {
    lua_Number a = luaL_checknumber(L, 1);
    lua_Number b = luaL_checknumber(L, 2);
    if (!(a <= b)) {
        const char* msg = luaL_optstring(L, 3, NULL);
        if (msg) {
            return luaL_error(L, "assert_lte failed: %s (got %f > %f)", msg, a, b);
        }
        return luaL_error(L, "assert_lte failed: %f is not <= %f", a, b);
    }
    return 0;
}

/* ── Value equality helper (used by spy assertions) ──────────────── */

static int values_equal(lua_State* L, int idx1, int idx2) {
    int t1 = lua_type(L, idx1);
    int t2 = lua_type(L, idx2);
    if (t1 != t2) return 0;
    switch (t1) {
        case LUA_TNIL:     return 1;
        case LUA_TBOOLEAN: return lua_toboolean(L, idx1) == lua_toboolean(L, idx2);
        case LUA_TNUMBER:  return lua_tonumber(L, idx1) == lua_tonumber(L, idx2);
        case LUA_TSTRING:  return strcmp(lua_tostring(L, idx1), lua_tostring(L, idx2)) == 0;
        default:           return lua_rawequal(L, idx1, idx2);
    }
}

/* ── Spy system ──────────────────────────────────────────────────── */

#define SPY_METATABLE "ScarSpy"

/* __call metamethod: record call args, then forward to wrapped fn */
static int l_spy_call(lua_State* L) {
    /* self (spy table) is at index 1, args start at 2 */
    int nargs = lua_gettop(L) - 1;

    /* Append a record to spy._calls */
    lua_getfield(L, 1, "_calls");
    int calls_idx = lua_gettop(L);
    int call_num = (int)lua_rawlen(L, calls_idx) + 1;

    /* Build {arg1, arg2, ..., n=nargs} */
    lua_createtable(L, nargs, 1);
    for (int i = 0; i < nargs; i++) {
        lua_pushvalue(L, 2 + i);
        lua_rawseti(L, -2, i + 1);
    }
    lua_pushinteger(L, nargs);
    lua_setfield(L, -2, "n");

    lua_rawseti(L, calls_idx, call_num);
    lua_pop(L, 1); /* pop _calls */

    /* Forward to wrapped function if present */
    lua_getfield(L, 1, "_fn");
    if (lua_isfunction(L, -1)) {
        int base = lua_gettop(L);
        for (int i = 0; i < nargs; i++) {
            lua_pushvalue(L, 2 + i);
        }
        lua_call(L, nargs, LUA_MULTRET);
        return lua_gettop(L) - base + 1;
    }
    lua_pop(L, 1);
    return 0;
}

/* spy([fn]) → spy object  (callable table that records calls) */
static int l_spy(lua_State* L) {
    int has_fn = lua_isfunction(L, 1);

    lua_createtable(L, 0, 2);

    /* spy._calls = {} */
    lua_newtable(L);
    lua_setfield(L, -2, "_calls");

    /* spy._fn = fn or false */
    if (has_fn) {
        lua_pushvalue(L, 1);
    } else {
        lua_pushboolean(L, 0);
    }
    lua_setfield(L, -2, "_fn");

    /* Set ScarSpy metatable (provides __call) */
    luaL_getmetatable(L, SPY_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

/* Helper: get call count from spy, or error if not a spy */
static int spy_call_count(lua_State* L, int spy_idx) {
    lua_getfield(L, spy_idx, "_calls");
    if (lua_isnil(L, -1)) {
        luaL_error(L, "expected a spy (created with spy())");
    }
    int n = (int)lua_rawlen(L, -1);
    lua_pop(L, 1);
    return n;
}

/* assert_called(spy, n [, msg]) — verify spy was called exactly n times */
static int l_assert_called(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int expected = (int)luaL_checkinteger(L, 2);
    int actual = spy_call_count(L, 1);

    if (actual != expected) {
        const char* msg = luaL_optstring(L, 3, NULL);
        if (msg)
            return luaL_error(L, "assert_called failed: %s (expected %d calls, got %d)", msg, expected, actual);
        return luaL_error(L, "assert_called failed: expected %d calls, got %d", expected, actual);
    }
    return 0;
}

/* assert_not_called(spy [, msg]) — verify spy was never called */
static int l_assert_not_called(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int actual = spy_call_count(L, 1);

    if (actual != 0) {
        const char* msg = luaL_optstring(L, 2, NULL);
        if (msg)
            return luaL_error(L, "assert_not_called failed: %s (called %d times)", msg, actual);
        return luaL_error(L, "assert_not_called failed: spy was called %d times", actual);
    }
    return 0;
}

/* assert_called_with(spy, arg1, arg2, ...) — at least one call matched */
static int l_assert_called_with(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int expected_nargs = lua_gettop(L) - 1;

    lua_getfield(L, 1, "_calls");
    if (lua_isnil(L, -1)) {
        return luaL_error(L, "assert_called_with: expected a spy (created with spy())");
    }
    int calls_idx = lua_gettop(L);
    int ncalls = (int)lua_rawlen(L, calls_idx);

    if (ncalls == 0) {
        lua_pop(L, 1);
        return luaL_error(L, "assert_called_with failed: spy was never called");
    }

    for (int c = 1; c <= ncalls; c++) {
        lua_rawgeti(L, calls_idx, c);
        int rec_idx = lua_gettop(L);

        lua_getfield(L, rec_idx, "n");
        int stored_n = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        if (stored_n != expected_nargs) {
            lua_pop(L, 1); /* pop record */
            continue;
        }

        int match = 1;
        for (int a = 1; a <= expected_nargs; a++) {
            lua_rawgeti(L, rec_idx, a);   /* stored arg */
            if (!values_equal(L, -1, 1 + a)) {
                match = 0;
                lua_pop(L, 1);
                break;
            }
            lua_pop(L, 1);
        }

        lua_pop(L, 1); /* pop record */
        if (match) {
            lua_pop(L, 1); /* pop _calls */
            return 0;       /* success */
        }
    }

    lua_pop(L, 1); /* pop _calls */
    return luaL_error(L, "assert_called_with failed: no call matched the expected arguments");
}

/* ── Mock state helpers ──────────────────────────────────────────── */

static int l_Mock_CreateEntity(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    const char* bp = luaL_optstring(L, 2, "");
    float x = (float)luaL_optnumber(L, 3, 0.0);
    float y = (float)luaL_optnumber(L, 4, 0.0);
    float z = (float)luaL_optnumber(L, 5, 0.0);

    GameState* gs = game_state_from_lua(L);
    if (game_state_get_entity(gs, id)) {
        return luaL_error(L, "Mock_CreateEntity: entity %d already exists", id);
    }
    MockEntity* e = game_state_add_entity(gs, id);
    if (!e) {
        return luaL_error(L, "Mock_CreateEntity: maximum entity count reached");
    }
    strncpy(e->blueprint, bp, MAX_BLUEPRINT_LEN - 1);
    e->position.x = x;
    e->position.y = y;
    e->position.z = z;

    /* Optional: health_max as 6th arg */
    if (lua_gettop(L) >= 6) {
        e->health_max = (float)lua_tonumber(L, 6);
        e->health = e->health_max;
    }

    push_entity(L, id);
    return 1;
}

static int l_Mock_CreateSquad(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    const char* bp = luaL_optstring(L, 2, "");

    GameState* gs = game_state_from_lua(L);
    if (game_state_get_squad(gs, id)) {
        return luaL_error(L, "Mock_CreateSquad: squad %d already exists", id);
    }
    MockSquad* s = game_state_add_squad(gs, id);
    if (!s) {
        return luaL_error(L, "Mock_CreateSquad: maximum squad count reached");
    }
    strncpy(s->blueprint, bp, MAX_BLUEPRINT_LEN - 1);

    /* Optional: entity_ids table as 3rd arg (Entity userdata handles) */
    if (lua_istable(L, 3)) {
        int n = (int)lua_rawlen(L, 3);
        if (n > MAX_SQUAD_ENTITIES) n = MAX_SQUAD_ENTITIES;
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, 3, i);
            s->entity_ids[s->entity_count++] = check_entity_id(L, -1);
            lua_pop(L, 1);
        }
    }

    push_squad(L, id);
    return 1;
}

static int l_Mock_CreatePlayer(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    int team_id = (int)luaL_optinteger(L, 2, 0);
    const char* name = luaL_optstring(L, 3, "");
    const char* race = luaL_optstring(L, 4, "");
    int is_human = 1;
    if (lua_gettop(L) >= 5) {
        is_human = lua_toboolean(L, 5);
    }

    GameState* gs = game_state_from_lua(L);
    if (game_state_get_player(gs, id)) {
        return luaL_error(L, "Mock_CreatePlayer: player %d already exists", id);
    }
    MockPlayer* p = game_state_add_player(gs, id);
    if (!p) {
        return luaL_error(L, "Mock_CreatePlayer: maximum player count reached");
    }
    p->team_id = team_id;
    strncpy(p->display_name, name, MAX_DISPLAY_NAME_LEN - 1);
    strncpy(p->race_name, race, MAX_BLUEPRINT_LEN - 1);
    p->is_human = is_human;

    push_player(L, id);
    return 1;
}

static int l_Mock_SetResources(lua_State* L) {
    int player_id = check_player_id(L, 1);
    float mp  = (float)luaL_checknumber(L, 2);
    float fuel = (float)luaL_checknumber(L, 3);
    float mun  = (float)luaL_checknumber(L, 4);

    GameState* gs = game_state_from_lua(L);
    MockPlayer* p = game_state_get_player(gs, player_id);
    if (!p) {
        return luaL_error(L, "Mock_SetResources: player %d not found", player_id);
    }
    p->resources.manpower = mp;
    p->resources.fuel = fuel;
    p->resources.munitions = mun;
    return 0;
}

static int l_Mock_ResetState(lua_State* L) {
    GameState* gs = game_state_from_lua(L);
    game_state_reset(gs, L);
    return 0;
}

static int l_Mock_SetEntityOwner(lua_State* L) {
    int entity_id = check_entity_id(L, 1);
    int player_id = check_player_id(L, 2);
    GameState* gs = game_state_from_lua(L);
    MockEntity* e = game_state_get_entity(gs, entity_id);
    if (!e) return luaL_error(L, "Mock_SetEntityOwner: entity %d not found", entity_id);
    e->player_owner_id = player_id;
    return 0;
}

static int l_Mock_SetSquadOwner(lua_State* L) {
    int squad_id = check_squad_id(L, 1);
    int player_id = check_player_id(L, 2);
    GameState* gs = game_state_from_lua(L);
    MockSquad* s = game_state_get_squad(gs, squad_id);
    if (!s) return luaL_error(L, "Mock_SetSquadOwner: squad %d not found", squad_id);
    s->player_owner_id = player_id;
    return 0;
}

/* ── SCAR lifecycle helpers ──────────────────────────────────────── */

static int l_Scar_ForceInit(lua_State* L) {
    GameState* gs = game_state_from_lua(L);
    for (int i = 0; i < gs->init_func_count; i++) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, gs->init_func_refs[i]);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            fprintf(stderr, "Error in init function %d: %s\n", i, err);
            lua_pop(L, 1);
        }
    }
    return 0;
}

static int l_Scar_ForceGameSetup(lua_State* L) {
    lua_getglobal(L, "OnGameSetup");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            fprintf(stderr, "Error in OnGameSetup: %s\n", err);
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
    return 0;
}

static int l_Scar_ForceStart(lua_State* L) {
    /* Calls Core_CallDelegateFunctions("OnGameSetup") then all init funcs,
       then Core_CallDelegateFunctions("OnInit") */
    GameState* gs = game_state_from_lua(L);

    /* OnGameSetup delegates */
    lua_getglobal(L, "Core_CallDelegateFunctions");
    if (lua_isfunction(L, -1)) {
        lua_pushstring(L, "OnGameSetup");
        lua_call(L, 1, 0);
    } else {
        lua_pop(L, 1);
    }

    /* Run registered init functions */
    for (int i = 0; i < gs->init_func_count; i++) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, gs->init_func_refs[i]);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            fprintf(stderr, "Error in init function %d: %s\n", i, err);
            lua_pop(L, 1);
        }
    }

    /* OnInit delegates */
    lua_getglobal(L, "Core_CallDelegateFunctions");
    if (lua_isfunction(L, -1)) {
        lua_pushstring(L, "OnInit");
        lua_call(L, 1, 0);
    } else {
        lua_pop(L, 1);
    }

    return 0;
}

/* ── Mock_FireGlobalEvent ────────────────────────────────────────── */

static int l_Mock_FireGlobalEvent(lua_State* L) {
    int event_type = (int)luaL_checkinteger(L, 1);
    int nargs = lua_gettop(L) - 1;
    GameState* gs = game_state_from_lua(L);

    /* Build a context table from remaining args */
    for (int i = 0; i < gs->global_event_count; i++) {
        GlobalEventRule* r = &gs->global_event_rules[i];
        if (!r->active || r->event_type != event_type) continue;

        lua_rawgeti(L, LUA_REGISTRYINDEX, r->lua_func_ref);
        /* Push context: a table with the extra args */
        lua_createtable(L, 0, nargs);
        for (int a = 0; a < nargs; a++) {
            lua_pushvalue(L, 2 + a);
            lua_rawseti(L, -2, a + 1);
        }
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            fprintf(stderr, "Mock_FireGlobalEvent: error: %s\n", err);
            lua_pop(L, 1);
        }
    }
    return 0;
}

/* ── Mock_CreateTerritory ────────────────────────────────────────── */

static int l_Mock_CreateTerritory(lua_State* L) {
    int sector_id = (int)luaL_checkinteger(L, 1);
    int owner_player_id = (int)luaL_optinteger(L, 2, -1);
    float x = (float)luaL_optnumber(L, 3, 0.0);
    float y = (float)luaL_optnumber(L, 4, 0.0);
    float z = (float)luaL_optnumber(L, 5, 0.0);

    GameState* gs = game_state_from_lua(L);
    if (gs->territory_count >= MAX_TERRITORIES) {
        return luaL_error(L, "Mock_CreateTerritory: max territories reached");
    }
    MockTerritory* t = &gs->territories[gs->territory_count++];
    t->sector_id = sector_id;
    t->owner_player_id = owner_player_id;
    t->position.x = x;
    t->position.y = y;
    t->position.z = z;
    return 0;
}

/* ── Mock_CreateStrategyPoint ────────────────────────────────────── */

static int l_Mock_CreateStrategyPoint(lua_State* L) {
    int entity_id = check_entity_id(L, 1);
    int sector_id = (int)luaL_checkinteger(L, 2);
    int is_vp = lua_toboolean(L, 3);

    GameState* gs = game_state_from_lua(L);
    if (gs->strategy_point_count >= MAX_STRATEGY_POINTS) {
        return luaL_error(L, "Mock_CreateStrategyPoint: max strategy points reached");
    }
    StrategyPoint* sp = &gs->strategy_points[gs->strategy_point_count++];
    sp->entity_id = entity_id;
    sp->sector_id = sector_id;
    sp->is_victory_point = is_vp;

    /* Also set the entity's is_victory_point flag */
    MockEntity* e = game_state_get_entity(gs, entity_id);
    if (e) {
        e->is_victory_point = is_vp;
    }

    return 0;
}

/* ── Mock_SetPlayerStartingPosition ──────────────────────────────── */

static int l_Mock_SetPlayerStartingPosition(lua_State* L) {
    int player_id = check_player_id(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_checknumber(L, 4);

    GameState* gs = game_state_from_lua(L);
    MockPlayer* p = game_state_get_player(gs, player_id);
    if (!p) return luaL_error(L, "Mock_SetPlayerStartingPosition: player %d not found", player_id);
    p->starting_position.x = x;
    p->starting_position.y = y;
    p->starting_position.z = z;
    p->has_starting_position = true;
    return 0;
}

static int l_Mock_AdvanceTime(lua_State* L) {
    float seconds = (float)luaL_checknumber(L, 1);
    GameState* gs = game_state_from_lua(L);
    gs->game_time += seconds;

    /* Fire due rules */
    for (int i = 0; i < gs->rule_count; i++) {
        MockRule* r = &gs->rules[i];
        if (!r->active) continue;

        r->elapsed += seconds;
        if (r->elapsed >= r->interval) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, r->lua_func_ref);
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                fprintf(stderr, "Error in rule callback: %s\n", err);
                lua_pop(L, 1);
            }

            if (r->type == RULE_ONESHOT) {
                r->active = false;
                luaL_unref(L, LUA_REGISTRYINDEX, r->lua_func_ref);
                r->lua_func_ref = LUA_NOREF;
            } else {
                /* Reset for next interval */
                r->elapsed -= r->interval;
            }
        }
    }
    return 0;
}

/* ── Test Suite lifecycle ────────────────────────────────────────── */

TestSuite* test_suite_create(void) {
    TestSuite* ts = (TestSuite*)calloc(1, sizeof(TestSuite));
    ts->current_describe = -1;
    return ts;
}

void test_suite_destroy(TestSuite* ts) {
    if (ts) free(ts);
}

/* ── Run hooks for a test's describe chain ───────────────────────── */

static void collect_hooks_chain(TestSuite* ts, int describe_idx,
                                int* refs_out, int* count_out, int is_before) {
    /* Collect describes from outermost to innermost */
    int chain[MAX_DESCRIBES];
    int chain_len = 0;
    int idx = describe_idx;
    while (idx >= 0) {
        chain[chain_len++] = idx;
        idx = ts->describes[idx].parent_idx;
    }
    /* Reverse: outermost first */
    *count_out = 0;
    for (int i = chain_len - 1; i >= 0; i--) {
        DescribeBlock* db = &ts->describes[chain[i]];
        int n = is_before ? db->before_each_count : db->after_each_count;
        int* src = is_before ? db->before_each_refs : db->after_each_refs;
        for (int j = 0; j < n; j++) {
            refs_out[(*count_out)++] = src[j];
        }
    }
}

/* ── Execute all collected tests ─────────────────────────────────── */

/* ── Format duration for display ─────────────────────────────────── */

static const char* format_duration(double ms, char* buf, size_t buf_sz) {
    if (ms < 1.0)
        snprintf(buf, buf_sz, "%.0f \xc2\xb5s", ms * 1000.0);
    else if (ms < 1000.0)
        snprintf(buf, buf_sz, "%.1f ms", ms);
    else
        snprintf(buf, buf_sz, "%.2f s", ms / 1000.0);
    return buf;
}

int test_suite_run(lua_State* L, TestSuite* ts, const char* file_label) {
    enable_ansi_colors();

    printf("\n" COLOR_DIM "# %s" COLOR_RESET "\n", file_label);

    ts->total = ts->test_count;
    ts->passed = 0;
    ts->failed = 0;

    double suite_start = get_time_ms();
    GameState* gs = game_state_from_lua(L);

    for (int i = 0; i < ts->test_count; i++) {
        TestCase* tc = &ts->tests[i];
        tc->ran = true;

        /* Reset game state between tests */
        game_state_reset(gs, L);

        /* Run before_each hooks (outermost to innermost) */
        int hook_refs[MAX_HOOKS * MAX_DESCRIBES];
        int hook_count = 0;
        if (tc->describe_idx >= 0) {
            collect_hooks_chain(ts, tc->describe_idx, hook_refs, &hook_count, 1);
        }
        int hook_failed = 0;
        for (int h = 0; h < hook_count; h++) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, hook_refs[h]);
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                snprintf(tc->error_msg, sizeof(tc->error_msg), "before_each error: %s", err);
                lua_pop(L, 1);
                hook_failed = 1;
                break;
            }
        }

        /* Run the test itself */
        double t_start = get_time_ms();
        if (!hook_failed) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, tc->lua_func_ref);
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                snprintf(tc->error_msg, sizeof(tc->error_msg), "%s", err);
                lua_pop(L, 1);
                tc->passed = false;
            } else {
                tc->passed = true;
            }
        }
        tc->duration_ms = get_time_ms() - t_start;

        /* Run after_each hooks (outermost to innermost, even if test failed) */
        if (tc->describe_idx >= 0) {
            collect_hooks_chain(ts, tc->describe_idx, hook_refs, &hook_count, 0);
            for (int h = 0; h < hook_count; h++) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, hook_refs[h]);
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    lua_pop(L, 1);
                }
            }
        }

        /* Print result */
        char tbuf[32];
        if (tc->passed) {
            ts->passed++;
            printf(COLOR_GREEN "  ok %d" COLOR_RESET " - %s" COLOR_DIM " (%s)" COLOR_RESET "\n",
                   i + 1, tc->full_name, format_duration(tc->duration_ms, tbuf, sizeof(tbuf)));
        } else {
            ts->failed++;
            printf(COLOR_RED "  not ok %d" COLOR_RESET " - %s" COLOR_DIM " (%s)" COLOR_RESET "\n",
                   i + 1, tc->full_name, format_duration(tc->duration_ms, tbuf, sizeof(tbuf)));
            if (tc->error_msg[0]) {
                printf(COLOR_RED "    --- %s" COLOR_RESET "\n", tc->error_msg);
            }
        }
    }

    /* Summary */
    double suite_ms = get_time_ms() - suite_start;
    char suite_tbuf[32];
    if (ts->failed > 0) {
        printf("\n" COLOR_RED "  Results: %d/%d passed, %d failed" COLOR_RESET
               COLOR_DIM " in %s" COLOR_RESET "\n",
               ts->passed, ts->total, ts->failed,
               format_duration(suite_ms, suite_tbuf, sizeof(suite_tbuf)));
    } else {
        printf("\n" COLOR_GREEN "  Results: %d/%d passed" COLOR_RESET
               COLOR_DIM " in %s" COLOR_RESET "\n",
               ts->passed, ts->total,
               format_duration(suite_ms, suite_tbuf, sizeof(suite_tbuf)));
    }

    return ts->failed;
}

/* ── Registration ────────────────────────────────────────────────── */

void test_api_register(lua_State* L, TestSuite* ts) {
    test_suite_store(L, ts);

    /* Spy metatable (provides __call) */
    luaL_newmetatable(L, SPY_METATABLE);
    lua_pushcfunction(L, l_spy_call);
    lua_setfield(L, -2, "__call");
    lua_pop(L, 1);

    /* BDD structure */
    lua_register(L, "describe",     l_describe);
    lua_register(L, "it",           l_it);
    lua_register(L, "before_each",  l_before_each);
    lua_register(L, "after_each",   l_after_each);

    /* Override print() for stylized output */
    lua_register(L, "print",        l_styled_print);

    /* Spy factory */
    lua_register(L, "spy",                l_spy);

    /* Assertions */
    lua_register(L, "assert_eq",          l_assert_eq);
    lua_register(L, "assert_ne",          l_assert_ne);
    lua_register(L, "assert_true",        l_assert_true);
    lua_register(L, "assert_false",       l_assert_false);
    lua_register(L, "assert_nil",         l_assert_nil);
    lua_register(L, "assert_not_nil",     l_assert_not_nil);
    lua_register(L, "assert_error",       l_assert_error);
    lua_register(L, "assert_gt",          l_assert_gt);
    lua_register(L, "assert_gte",         l_assert_gte);
    lua_register(L, "assert_lt",          l_assert_lt);
    lua_register(L, "assert_lte",         l_assert_lte);
    lua_register(L, "assert_called",      l_assert_called);
    lua_register(L, "assert_not_called",  l_assert_not_called);
    lua_register(L, "assert_called_with", l_assert_called_with);

    /* Mock state helpers */
    lua_register(L, "Mock_CreateEntity",   l_Mock_CreateEntity);
    lua_register(L, "Mock_CreateSquad",    l_Mock_CreateSquad);
    lua_register(L, "Mock_CreatePlayer",   l_Mock_CreatePlayer);
    lua_register(L, "Mock_SetResources",   l_Mock_SetResources);
    lua_register(L, "Mock_ResetState",     l_Mock_ResetState);
    lua_register(L, "Mock_SetEntityOwner", l_Mock_SetEntityOwner);
    lua_register(L, "Mock_SetSquadOwner",  l_Mock_SetSquadOwner);
    lua_register(L, "Mock_AdvanceTime",    l_Mock_AdvanceTime);
    lua_register(L, "Mock_FireGlobalEvent", l_Mock_FireGlobalEvent);
    lua_register(L, "Mock_CreateTerritory", l_Mock_CreateTerritory);
    lua_register(L, "Mock_CreateStrategyPoint", l_Mock_CreateStrategyPoint);
    lua_register(L, "Mock_SetPlayerStartingPosition", l_Mock_SetPlayerStartingPosition);

    /* SCAR lifecycle */
    lua_register(L, "Scar_ForceInit",      l_Scar_ForceInit);
    lua_register(L, "Scar_ForceGameSetup", l_Scar_ForceGameSetup);
    lua_register(L, "Scar_ForceStart",     l_Scar_ForceStart);
}
