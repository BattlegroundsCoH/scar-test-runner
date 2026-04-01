#include "test_api.h"
#include "game_state.h"
#include "lauxlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ── Color output helpers ────────────────────────────────────────── */

#ifdef _WIN32
#include <windows.h>
static void enable_ansi_colors(void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */);
}
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET  "\033[0m"
#define COLOR_DIM    "\033[2m"
#else
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET  "\033[0m"
#define COLOR_DIM    "\033[2m"
static void enable_ansi_colors(void) {}
#endif

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

    lua_pushinteger(L, id);
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

    /* Optional: entity_ids table as 3rd arg */
    if (lua_istable(L, 3)) {
        int n = (int)lua_rawlen(L, 3);
        if (n > MAX_SQUAD_ENTITIES) n = MAX_SQUAD_ENTITIES;
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, 3, i);
            s->entity_ids[s->entity_count++] = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);
        }
    }

    lua_pushinteger(L, id);
    return 1;
}

static int l_Mock_CreatePlayer(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    int team_id = (int)luaL_optinteger(L, 2, 0);

    GameState* gs = game_state_from_lua(L);
    if (game_state_get_player(gs, id)) {
        return luaL_error(L, "Mock_CreatePlayer: player %d already exists", id);
    }
    MockPlayer* p = game_state_add_player(gs, id);
    if (!p) {
        return luaL_error(L, "Mock_CreatePlayer: maximum player count reached");
    }
    p->team_id = team_id;

    lua_pushinteger(L, id);
    return 1;
}

static int l_Mock_SetResources(lua_State* L) {
    int player_id = (int)luaL_checkinteger(L, 1);
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
    int entity_id = (int)luaL_checkinteger(L, 1);
    int player_id = (int)luaL_checkinteger(L, 2);
    GameState* gs = game_state_from_lua(L);
    MockEntity* e = game_state_get_entity(gs, entity_id);
    if (!e) return luaL_error(L, "Mock_SetEntityOwner: entity %d not found", entity_id);
    e->player_owner_id = player_id;
    return 0;
}

static int l_Mock_SetSquadOwner(lua_State* L) {
    int squad_id = (int)luaL_checkinteger(L, 1);
    int player_id = (int)luaL_checkinteger(L, 2);
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

int test_suite_run(lua_State* L, TestSuite* ts, const char* file_label) {
    enable_ansi_colors();

    printf("\n" COLOR_DIM "# %s" COLOR_RESET "\n", file_label);

    ts->total = ts->test_count;
    ts->passed = 0;
    ts->failed = 0;

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
        if (tc->passed) {
            ts->passed++;
            printf(COLOR_GREEN "  ok %d" COLOR_RESET " - %s\n", i + 1, tc->full_name);
        } else {
            ts->failed++;
            printf(COLOR_RED "  not ok %d" COLOR_RESET " - %s\n", i + 1, tc->full_name);
            if (tc->error_msg[0]) {
                printf(COLOR_RED "    --- %s" COLOR_RESET "\n", tc->error_msg);
            }
        }
    }

    /* Summary */
    if (ts->failed > 0) {
        printf("\n" COLOR_RED "  Results: %d/%d passed, %d failed" COLOR_RESET "\n",
               ts->passed, ts->total, ts->failed);
    } else {
        printf("\n" COLOR_GREEN "  Results: %d/%d passed" COLOR_RESET "\n",
               ts->passed, ts->total);
    }

    return ts->failed;
}

/* ── Registration ────────────────────────────────────────────────── */

void test_api_register(lua_State* L, TestSuite* ts) {
    test_suite_store(L, ts);

    /* BDD structure */
    lua_register(L, "describe",     l_describe);
    lua_register(L, "it",           l_it);
    lua_register(L, "before_each",  l_before_each);
    lua_register(L, "after_each",   l_after_each);

    /* Assertions */
    lua_register(L, "assert_eq",       l_assert_eq);
    lua_register(L, "assert_ne",       l_assert_ne);
    lua_register(L, "assert_true",     l_assert_true);
    lua_register(L, "assert_false",    l_assert_false);
    lua_register(L, "assert_nil",      l_assert_nil);
    lua_register(L, "assert_not_nil",  l_assert_not_nil);
    lua_register(L, "assert_error",    l_assert_error);
    lua_register(L, "assert_gt",       l_assert_gt);
    lua_register(L, "assert_gte",      l_assert_gte);
    lua_register(L, "assert_lt",       l_assert_lt);
    lua_register(L, "assert_lte",      l_assert_lte);

    /* Mock state helpers */
    lua_register(L, "Mock_CreateEntity",   l_Mock_CreateEntity);
    lua_register(L, "Mock_CreateSquad",    l_Mock_CreateSquad);
    lua_register(L, "Mock_CreatePlayer",   l_Mock_CreatePlayer);
    lua_register(L, "Mock_SetResources",   l_Mock_SetResources);
    lua_register(L, "Mock_ResetState",     l_Mock_ResetState);
    lua_register(L, "Mock_SetEntityOwner", l_Mock_SetEntityOwner);
    lua_register(L, "Mock_SetSquadOwner",  l_Mock_SetSquadOwner);
    lua_register(L, "Mock_AdvanceTime",    l_Mock_AdvanceTime);

    /* SCAR lifecycle */
    lua_register(L, "Scar_ForceInit",      l_Scar_ForceInit);
    lua_register(L, "Scar_ForceGameSetup", l_Scar_ForceGameSetup);
}
