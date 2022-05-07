#include <check.h>
#include <frec.h>
#include <stdlib.h>
#include "wm-comp.h"
#include "string-type.h"

/*
 * Runs the Boyer-Moore execution phase. Asserts that the preprocessing
 * succeeded, and returns the final execution return value as well as any
 * potential matches in the matches input variable.
 */
static int
run_execute(
    frec_match_t *match,
    const char **patterns, ssize_t count, const char *text
) {
    wm_comp comp;

    string *strs = malloc(sizeof(string) * count);

    for (int i = 0; i < count; i++) {
        const char *pattern = patterns[i];
        string_borrow(&strs[i], pattern, (ssize_t) strlen(pattern), false);
    }

    int ret = wm_compile(&comp, strs, count, 0);
    ck_assert_msg(ret == REG_OK,
                  "Execute failed because preprocessing failed: returned '%d'",
                  ret
    );

    string txt;
    string_borrow(&txt, text, (ssize_t) strlen(text), true);

    ret = wm_execute(match, &comp, txt, 0);

    wm_comp_free(&comp);
    return ret;
}

typedef struct exec_tuple {
    const char *patterns[5];
    ssize_t count;
    const char *text;
    int flags;
    frec_match_t expected;
} exec_tuple;

#define EXEC_SUCC_LEN 5
static exec_tuple exec_successes[EXEC_SUCC_LEN] = {
    /* Test with single patterns */
    {{"exactly the same"}, 1, "exactly the same", 0, {0, 16}},
    {{"alpha"}, 1, "alpha beta gamma delta", 0, {0, 5}},
    /* Test with two patterns that match */
    {{"alfa", "beta"}, 2, "alpha beta gamma delta", 0, {0, 5}},
    {{"beta", "delta"}, 2, "alpha beta gamma delta", 0, {6, 10}},
    {{"delta", "gamma"}, 2, "alpha beta gamma delta", 0, {11, 16}}
};

START_TEST(loop_test_wm__successes__single_exec_succeeds)
    {
        exec_tuple curr = exec_successes[_i];

        frec_match_t match;
        int ret = run_execute(&match, curr.patterns, curr.count, curr.text);

        ck_assert_msg(ret == REG_OK,
            "Execution did not succed: returned '%d' for element %d with text '%s' with flags '%d'",
            ret, _i, curr.text, curr.flags
        );

        ck_assert_msg(match.soffset == curr.expected.soffset,
            "Execution succeeded but match soffset differs: Got '%d' instead of '%d' for element %d with text '%s' with flags '%d'",
            match.soffset, curr.expected.soffset, _i, curr.text, curr.flags
        );

        ck_assert_msg(match.eoffset == curr.expected.eoffset,
            "Execution succeeded but match eoffset differs: Got '%d' instead of '%d' for element %d with text '%s' with flags '%d'",
            match.eoffset, curr.expected.eoffset, _i, curr.text, curr.flags
        );
    }
END_TEST

static Suite *
create_suite()
{
    Suite *suite = suite_create("Wu-Manber");

    TCase *tc_exec = tcase_create("Execution");

    tcase_add_loop_test(tc_exec, loop_test_wm__successes__single_exec_succeeds, 0, EXEC_SUCC_LEN);

    suite_add_tcase(suite, tc_exec);

    return suite;
}

int
main(void)
{
    Suite *suite = create_suite();
    SRunner *runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    int failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
