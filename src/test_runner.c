#include "test_runner.h"
#include "scar_state.h"
#include "game_state.h"
#include "test_api.h"
#include "lauxlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

/* ── File matching ───────────────────────────────────────────────── */

static int is_test_file(const char* name) {
    size_t len = strlen(name);
    /* Must end in .scar */
    if (len < 6) return 0;
    if (strcmp(name + len - 5, ".scar") != 0) return 0;

    /* Check test_*.scar or *_test.scar */
    if (strncmp(name, "test_", 5) == 0) return 1;
    if (len >= 10 && strcmp(name + len - 10, "_test.scar") == 0) return 1;

    return 0;
}

/* ── Run a single test file ──────────────────────────────────────── */

int test_runner_run_file(const char* filepath, const char* scar_root, const char* scar_data) {
    GameState* gs = game_state_create();
    if (!gs) {
        fprintf(stderr, "Failed to allocate game state\n");
        return -1;
    }

    lua_State* L = scar_state_new(scar_root, scar_data, gs);
    if (!L) {
        fprintf(stderr, "Failed to create Lua state\n");
        game_state_destroy(gs);
        return -1;
    }

    TestSuite* ts = test_suite_create();
    if (!ts) {
        fprintf(stderr, "Failed to allocate test suite\n");
        scar_state_destroy(L);
        game_state_destroy(gs);
        return -1;
    }

    test_api_register(L, ts);

    /* Store the test file's directory for resource() resolution */
    {
        char test_dir[1024];
        strncpy(test_dir, filepath, sizeof(test_dir) - 1);
        test_dir[sizeof(test_dir) - 1] = '\0';
        /* Normalize backslashes to forward slashes */
        for (char* p = test_dir; *p; p++) { if (*p == '\\') *p = '/'; }
        char* last_slash = strrchr(test_dir, '/');
        if (last_slash) *last_slash = '\0';
        else { test_dir[0] = '.'; test_dir[1] = '\0'; }
        lua_pushstring(L, test_dir);
        lua_setfield(L, LUA_REGISTRYINDEX, SCAR_RESOURCE_DIR_REGISTRY_KEY);
    }

    /* Load and execute the test file (collects describe/it blocks) */
    if (luaL_dofile(L, filepath) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        fprintf(stderr, "\033[31mError loading %s: %s\033[0m\n", filepath, err);
        lua_pop(L, 1);
        test_suite_destroy(ts);
        scar_state_destroy(L);
        game_state_destroy(gs);
        return -1;
    }

    /* Run the collected tests */
    int failures = test_suite_run(L, ts, filepath);

    test_suite_destroy(ts);
    scar_state_destroy(L);
    game_state_destroy(gs);

    return failures;
}

/* ── Directory traversal ─────────────────────────────────────────── */

#define MAX_FILES 1024

typedef struct {
    char paths[MAX_FILES][1024];
    int  count;
} FileList;

#ifdef _WIN32

static void find_test_files_recursive(const char* dir, FileList* fl) {
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (fd.cFileName[0] == '.') continue;

        char full[1024];
        snprintf(full, sizeof(full), "%s\\%s", dir, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            find_test_files_recursive(full, fl);
        } else if (is_test_file(fd.cFileName)) {
            if (fl->count < MAX_FILES) {
                strncpy(fl->paths[fl->count], full, sizeof(fl->paths[0]) - 1);
                fl->count++;
            }
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

#else /* POSIX */

static void find_test_files_recursive(const char* dir, FileList* fl) {
    DIR* dp = opendir(dir);
    if (!dp) return;

    struct dirent* entry;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            find_test_files_recursive(full, fl);
        } else if (is_test_file(entry->d_name)) {
            if (fl->count < MAX_FILES) {
                strncpy(fl->paths[fl->count], full, sizeof(fl->paths[0]) - 1);
                fl->count++;
            }
        }
    }
    closedir(dp);
}

#endif

/* Compare function for sorting file paths */
static int compare_paths(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}

RunnerResult test_runner_run_dir(const char* dirpath, const char* scar_root, const char* scar_data) {
    RunnerResult result = {0, 0, 0, 0, 0};

    FileList* fl = (FileList*)calloc(1, sizeof(FileList));
    if (!fl) {
        fprintf(stderr, "Failed to allocate file list\n");
        return result;
    }

    find_test_files_recursive(dirpath, fl);

    /* Sort files for deterministic order */
    qsort(fl->paths, fl->count, sizeof(fl->paths[0]), compare_paths);

    result.total_files = fl->count;

    if (fl->count == 0) {
        printf("No test files found in %s\n", dirpath);
        printf("(Looking for test_*.scar or *_test.scar files)\n");
        free(fl);
        return result;
    }

    printf("Found %d test file(s) in %s\n", fl->count, dirpath);

    for (int i = 0; i < fl->count; i++) {
        int failures = test_runner_run_file(fl->paths[i], scar_root, scar_data);
        if (failures < 0) {
            result.files_failed++;
        } else if (failures > 0) {
            result.total_failed += failures;
            result.files_failed++;
        }
    }

    free(fl);
    return result;
}

void test_runner_print_summary(const RunnerResult* result) {
    printf("\n");
    if (result->total_files == 0) {
        printf("No test files were executed.\n");
        return;
    }

    printf("========================================\n");
    printf("Total files:   %d\n", result->total_files);
    if (result->files_failed > 0) {
        printf("\033[31mFiles failed:  %d\033[0m\n", result->files_failed);
    } else {
        printf("\033[32mAll files passed.\033[0m\n");
    }
    printf("========================================\n");
}
