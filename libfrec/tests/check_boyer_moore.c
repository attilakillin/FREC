
#include <check.h>
#include <frec.h>
#include <stdlib.h>

#include "boyer-moore.h"
#include "type-unification.h"

static int
run_preprocess_literal(const wchar_t *pattern, int flags)
{
    bm_preproc_t *preg = bm_create_preproc();

    int ret = bm_preprocess_literal(preg, pattern, wcslen(pattern), flags);

    bm_free_preproc(preg);
    return ret;
}

static int
run_preprocess_full(const wchar_t *pattern, int flags)
{
    bm_preproc_t *preg = bm_create_preproc();

    int ret = bm_preprocess_full(preg, pattern, wcslen(pattern), flags);

    bm_free_preproc(preg);
    return ret;
}

static int
run_execute(
    frec_match_t *matches, size_t match_cnt,
    const wchar_t *pattern, const char *text, int flags
)
{
    bm_preproc_t *preg = bm_create_preproc();
    str_t *string = string_create_stnd(text, strlen(text));

    int ret = bm_preprocess_full(preg, pattern, wcslen(pattern), flags);
    ck_assert_msg(ret == REG_OK,
        "Execute failed because preprocessing failed: returned '%d' for '%ls'",
        ret, pattern
    );

    ret = bm_execute(matches, match_cnt, preg, string, flags);

    string_free(string);
    bm_free_preproc(preg);
    return ret;
}


START_TEST(test_bm__sanity__literal_prep_ok)
{
    int ret = run_preprocess_literal(L"pattern", 0);
    ck_assert_msg(ret == REG_OK, "Sanity literal preprocessing failed: returned '%d'", ret);
}
END_TEST

START_TEST(test_bm__sanity__full_prep_ok)
{
    int ret = run_preprocess_full(L"pattern", 0);
    ck_assert_msg(ret == REG_OK, "Sanity full preprocessing failed: returned '%d'", ret);
}
END_TEST

typedef struct prep_tuple {
    const wchar_t *pattern;
    int flags;
} prep_tuple;

#define PREP_FAIL_LEN 13
static prep_tuple prep_failures[PREP_FAIL_LEN] = {
    /* Both BRE and ERE */
    {L"p[r]int", 0},
    {L"p[r]int", REG_EXTENDED},
    {L"p*int", 0},
    {L"p*int", REG_EXTENDED},
    {L"p.int", 0},
    {L"p.int", REG_EXTENDED},
    /* BRE and ERE inverted */
    {L"print\\(ln\\)", 0},
    {L"print(ln)", REG_EXTENDED},
    {L"print\\{1,2\\}", 0},
    {L"print{1,2}", REG_EXTENDED},
    /* Only in ERE */
    {L"pr|int", REG_EXTENDED},
    {L"pr+nt", REG_EXTENDED},
    {L"pri?nt", REG_EXTENDED},
};

#define PREP_SUCC_LEN 13
static prep_tuple prep_successes[PREP_SUCC_LEN] = {
    /* Both BRE and ERE */
    {L"p\\[r]int", 0},
    {L"p\\[r]int", REG_EXTENDED},
    {L"p\\*int", 0},
    {L"p\\*int", REG_EXTENDED},
    {L"p\\.int", 0},
    {L"p\\.int", REG_EXTENDED},
    /* BRE and ERE inverted */
    {L"print(ln)", 0},
    {L"print\\(ln)", REG_EXTENDED},
    {L"print{1,2}", 0},
    {L"print\\{1,2}", REG_EXTENDED},
    /* Only in ERE */
    {L"pri\\|nt", REG_EXTENDED},
    {L"pr\\+nt", REG_EXTENDED},
    {L"pri\\?nt", REG_EXTENDED}
};

START_TEST(loop_test_bm__failures__full_prep_fails)
{
    prep_tuple current = prep_failures[_i];
    int ret = run_preprocess_full(current.pattern, current.flags);

    ck_assert_msg(ret == REG_BADPAT,
        "Full preprocessing did not fail on bad pattern: returned '%d' for '%ls' with flags '%d'",
        ret, current.pattern, current.flags
    );
}
END_TEST

START_TEST(loop_test_bm__successes__full_prep_succeeds)
{
    prep_tuple current = prep_successes[_i];
    int ret = run_preprocess_full(current.pattern, current.flags);

    ck_assert_msg(ret == REG_OK,
        "Full preprocessing did not succeed on pattern: returned '%d' for '%ls' with flags '%d'",
        ret, current.pattern, current.flags
    );
}
END_TEST


START_TEST(test_bm__sanity__execute_on_match_ok)
{
    int ret = run_execute(NULL, 0, L"something", "text that contains something here", 0);
    ck_assert_msg(ret == REG_OK, "Sanity execution failed on match: returned '%d'", ret);
}
END_TEST

START_TEST(test_bm__sanity__execute_on_nomatch_ok)
{
    int ret = run_execute(NULL, 0, L"something", "text that doesn't contain it", 0);
    ck_assert_msg(ret == REG_NOMATCH, "Sanity execution failed on nomatch: returned '%d'", ret);
}
END_TEST

typedef struct exec_tuple {
    const wchar_t *pattern;
    const char *text;
    int flags;
    frec_match_t expected;
} exec_tuple;

#define EXEC_SUCC_LEN 14
static exec_tuple exec_successes[EXEC_SUCC_LEN] = {
    /* Test on full match */
    {L"exactly the same", "exactly the same", 0, {0, 16}},
    /* Same in BRE and ERE */
    {L"p\\[r]int", "text that p[r]ints", 0, {10, 17}},
    {L"p\\[r]int", "text that p[r]ints", REG_EXTENDED, {10, 17}},
    {L"p\\*int", "text that p*ints", 0, {10, 15}},
    {L"p\\*int", "text that p*ints", REG_EXTENDED, {10, 15}},
    {L"p\\.int", "text that p.ints", 0, {10, 15}},
    {L"p\\.int", "text that p.ints", REG_EXTENDED, {10, 15}},
    /* BRE and ERE inverted */
    {L"print(ln)", "text that print(ln)s", 0, {10, 19}},
    {L"print\\(ln)", "text that print(ln)s", REG_EXTENDED, {10, 19}},
    {L"print{1,2}", "text that print{1,2}s", 0, {10, 20}},
    {L"print\\{1,2}", "text that print{1,2}s", REG_EXTENDED, {10, 20}},
    /* Only in ERE*/
    {L"p\\|int", "text that p|ints", REG_EXTENDED, {10, 15}},
    {L"p\\+int", "text that p+ints", REG_EXTENDED, {10, 15}},
    {L"p\\?int", "text that p?ints", REG_EXTENDED, {10, 15}},
};

START_TEST(loop_test_bm__successes__single_exec_succeeds)
{
    exec_tuple current = exec_successes[_i];

    frec_match_t match;
    int ret = run_execute(&match, 1, current.pattern, current.text, current.flags);

    ck_assert_msg(ret == REG_OK,
        "Execution did not succed: returned '%d' for pattern '%ls' and text '%s' with flags '%d'",
        ret, current.pattern, current.text, current.flags
    );

    ck_assert_msg(match.soffset == current.expected.soffset,
        "Execution succeeded but match soffset differs: Got '%d' instead of '%d' for pattern '%ls' and text '%s' with flags '%d'",
        match.soffset, current.expected.soffset, current.pattern, current.text, current.flags
    );

    ck_assert_msg(match.eoffset == current.expected.eoffset,
        "Execution succeeded but match eoffset differs: Got '%d' instead of '%d' for pattern '%ls' and text '%s' with flags '%d'",
        match.eoffset, current.expected.eoffset, current.pattern, current.text, current.flags
    );
}
END_TEST

static Suite *
create_suite()
{
	Suite *suite = suite_create("Boyer-Moore");

	TCase *tc_prep = tcase_create("Preprocessing");

    tcase_add_test(tc_prep, test_bm__sanity__literal_prep_ok);
    tcase_add_test(tc_prep, test_bm__sanity__full_prep_ok);

    tcase_add_loop_test(tc_prep, loop_test_bm__failures__full_prep_fails, 0, PREP_FAIL_LEN);
    tcase_add_loop_test(tc_prep, loop_test_bm__successes__full_prep_succeeds, 0, PREP_SUCC_LEN);

	TCase *tc_exec = tcase_create("Execution");

    tcase_add_test(tc_exec, test_bm__sanity__execute_on_match_ok);
    tcase_add_test(tc_exec, test_bm__sanity__execute_on_nomatch_ok);

	tcase_add_loop_test(tc_exec, loop_test_bm__successes__single_exec_succeeds, 0, EXEC_SUCC_LEN);
	
    suite_add_tcase(suite, tc_prep);
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
