#include "test_runner.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

static void print_usage(const char* prog) {
    printf("Usage: %s [options] <file_or_directory>\n", prog);
    printf("\n");
    printf("SCAR Test Framework for Company of Heroes 3 mods.\n");
    printf("Runs .scar test files with a BDD-style API (describe/it).\n");
    printf("\n");
    printf("Options:\n");
    printf("  --scar-root <path>  Base path for resolving import() calls\n");
    printf("                      (default: current directory)\n");
    printf("  --scar-data <path>  Extra fallback path for resolving imports\n");
    printf("                      (optional; engine .scar files are handled\n");
    printf("                      automatically via built-in stubs)\n");
    printf("  --help              Show this help message\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  <file>              Run a single .scar test file\n");
    printf("  <directory>         Discover and run all test_*.scar / *_test.scar files\n");
    printf("\n");
    printf("Test file naming:\n");
    printf("  test_*.scar         Files starting with 'test_'\n");
    printf("  *_test.scar         Files ending with '_test'\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s --scar-root my-mod/assets/scar test/\n", prog);
    printf("  %s --scar-root my-mod/assets/scar test/test_my_wc.scar\n", prog);
}

static int is_directory(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
#endif
}

static int is_file(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
#endif
}

int main(int argc, char* argv[]) {
    const char* scar_root = ".";
    const char* scar_data = NULL;
    const char* target = NULL;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--scar-root") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --scar-root requires a path argument\n");
                return 1;
            }
            scar_root = argv[++i];
        } else if (strcmp(argv[i], "--scar-data") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --scar-data requires a path argument\n");
                return 1;
            }
            scar_data = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            if (target) {
                fprintf(stderr, "Error: only one file/directory argument is supported\n");
                return 1;
            }
            target = argv[i];
        }
    }

    if (!target) {
        fprintf(stderr, "Error: no test file or directory specified\n\n");
        print_usage(argv[0]);
        return 1;
    }

    if (is_file(target)) {
        int failures = test_runner_run_file(target, scar_root, scar_data);
        return (failures > 0 || failures < 0) ? 1 : 0;
    } else if (is_directory(target)) {
        RunnerResult result = test_runner_run_dir(target, scar_root, scar_data);
        test_runner_print_summary(&result);
        return (result.files_failed > 0) ? 1 : 0;
    } else {
        fprintf(stderr, "Error: '%s' is not a file or directory\n", target);
        return 1;
    }
}
