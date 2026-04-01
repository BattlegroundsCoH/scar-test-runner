#ifndef TEST_API_H
#define TEST_API_H

#include "lua.h"

#include <stdbool.h>

/* ── Test tree structures ────────────────────────────────────────── */

#define MAX_TEST_NAME    256
#define MAX_TESTS         512
#define MAX_DESCRIBES     128
#define MAX_HOOKS          32

typedef struct {
    char  full_name[MAX_TEST_NAME]; /* "describe > it" */
    int   lua_func_ref;
    int   describe_idx;             /* index into describes array */
    bool  passed;
    bool  ran;
    char  error_msg[512];
    double duration_ms;             /* wall-clock time for this test */
} TestCase;

typedef struct {
    char  name[MAX_TEST_NAME];
    int   parent_idx;               /* -1 for root */
    int   before_each_refs[MAX_HOOKS];
    int   before_each_count;
    int   after_each_refs[MAX_HOOKS];
    int   after_each_count;
} DescribeBlock;

typedef struct {
    DescribeBlock describes[MAX_DESCRIBES];
    int           describe_count;
    int           current_describe; /* stack: index of current describe during collection */

    TestCase      tests[MAX_TESTS];
    int           test_count;

    int           passed;
    int           failed;
    int           total;
} TestSuite;

/* ── API ─────────────────────────────────────────────────────────── */

/* Initialize a fresh test suite */
TestSuite* test_suite_create(void);
void       test_suite_destroy(TestSuite* ts);

/* Register the BDD functions (describe, it, before_each, etc.) and
   assertion functions into the Lua state.
   TestSuite* is stored in the Lua registry. */
void test_api_register(lua_State* L, TestSuite* ts);

/* Execute all collected tests. Returns number of failures. */
int  test_suite_run(lua_State* L, TestSuite* ts, const char* file_label);

#endif /* TEST_API_H */
