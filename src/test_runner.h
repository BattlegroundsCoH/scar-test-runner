#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

typedef struct {
    int total_files;
    int total_tests;
    int total_passed;
    int total_failed;
    int files_failed;
} RunnerResult;

/* Run a single test file. Returns 0 on success, >0 on failure count.
   scar_data may be NULL. */
int test_runner_run_file(const char* filepath, const char* scar_root, const char* scar_data);

/* Run all test_*.scar / *_test.scar files in a directory (recursive).
   scar_data may be NULL. Returns aggregate results. */
RunnerResult test_runner_run_dir(const char* dirpath, const char* scar_root, const char* scar_data);

/* Print final summary for multiple files */
void test_runner_print_summary(const RunnerResult* result);

#endif /* TEST_RUNNER_H */
