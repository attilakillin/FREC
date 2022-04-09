
#include <check.h>
#include <frec.h>
#include <stdlib.h>

#include "heuristic.h"

/* 
 * Runs the heuristics preprocessing phase with full regex parsing.
 * Returns the result of the preprocessing call.
 */
static int
run_preprocess(const wchar_t *pattern, int flags)
{
    heur *prep = frec_create_heur();

    int ret = frec_preprocess_heur(prep, pattern, wcslen(pattern), flags);

    frec_free_heur(prep);
    return ret;
}

/* 
 * Runs the heuristics preprocessing phase with full regex parsing.
 * Returns the created heur struct. Asserts that compilation succeeded.
 */
static heur *
run_and_return_prep(const wchar_t *pattern, int flags)
{
    heur *prep = frec_create_heur();

    int ret = frec_preprocess_heur(prep, pattern, wcslen(pattern), flags);
    ck_assert_msg(ret == REG_OK,
        "Preprocessing failed: returned '%d' for pattern '%ls' with flags '%d'",
        ret, pattern, flags
    );

    return prep;
}


START_TEST(test_heur__sanity__literal_ok)
{
    int ret = run_preprocess(L"pattern", 0);
    ck_assert_msg(ret == REG_OK, "Sanity literal preprocessing failed: returned '%d'", ret);
}
END_TEST

START_TEST(test_heur__sanity__prefix_ok)
{
    int ret = run_preprocess(L"pattern(other){1,2}", 0);
    ck_assert_msg(ret == REG_OK, "Sanity prefix preprocessing failed: returned '%d'", ret);
}
END_TEST

START_TEST(test_heur__sanity__longest_ok)
{
    int ret = run_preprocess(L"(other){1,2}pattern", 0);
    ck_assert_msg(ret == REG_OK, "Sanity longest preprocessing failed: returned '%d'", ret);
}
END_TEST


typedef struct heur_tuple {
    const wchar_t *pattern;
    int flags;
    const wchar_t *expected_segment;
} heur_tuple;

/* Prefix heuristics is only used when the pattern may contain
 * newlines AND we don't know its length AND REG_NEWLINE is not set.
 * Also, obviously, it has to have a literal prefix. */
#define PREF_SUCC_LEN 12
static heur_tuple prefix_successes[PREF_SUCC_LEN] = {
    /* Explicit newlines with unknown max length */
    {L"\nliteralx*", 0, L"\nliteral"},
    {L"\nliteralx*", REG_EXTENDED, L"\nliteral"},
    {L"\nliteral+", REG_EXTENDED, L"\nliteral"},

    /* Literal newlines with unknown max length */
    {L"\\nliteralx*", 0, L"\nliteral"},
    {L"\\nliteralx*", REG_EXTENDED, L"\nliteral"},
    {L"\\nliteral+", REG_EXTENDED, L"\nliteral"},

    /* Specific syntaxes which can include newlines with unknown max length */
    {L"literal[^x]x*", REG_EXTENDED, L"literal"},
    {L"literal[\n]x*", REG_EXTENDED, L"literal"},
    {L"literal(.)", REG_EXTENDED, L"literal"},
    {L"literal(\n)", REG_EXTENDED, L"literal"},
    {L"literal.x*", REG_EXTENDED, L"literal"},
    {L"literal.+", REG_EXTENDED, L"literal"},
};

/* In general scenarios, longest heuristics is used. */
#define LONG_SUCC_LEN 27
static heur_tuple longest_successes[LONG_SUCC_LEN] = {
    /** Literal at beginning of text **/
    /* Literal pattern */
    {L"pattern", 0, L"pattern"},
    /* Both BRE and ERE */
    {L"literal[opt]", 0, L"literal"},
    {L"literal[opt]", REG_EXTENDED, L"literal"},
    {L"literal[^opt]", REG_EXTENDED, L"literal"},
    {L"literalx*", 0, L"literal"},
    {L"literalx*", REG_EXTENDED, L"literal"},
    {L"literal.", 0, L"literal"},
    {L"literal.", REG_EXTENDED, L"literal"},
    /* BRE and ERE inverted */
    {L"literal\\(grp\\)", 0, L"literal"},
    {L"literal(grp)", REG_EXTENDED, L"literal"},
    {L"literalx\\{1,2\\}", 0, L"literal"},
    {L"literalx{1,2}", REG_EXTENDED, L"literal"},
    /* Only in ERE */
    {L"literal+", REG_EXTENDED, L"literal"},
    {L"literalx?", REG_EXTENDED, L"literal"},

    /** Literal at end of text **/
    /* Both BRE and ERE */
    {L"[opt]literal", 0, L"literal"},
    {L"[opt]literal", REG_EXTENDED, L"literal"},
    {L"[^opt]literal", REG_EXTENDED, L"literal"},
    {L"x*literal", 0, L"literal"},
    {L"x*literal", REG_EXTENDED, L"literal"},
    {L".literal", 0, L"literal"},
    {L".literal", REG_EXTENDED, L"literal"},
    /* BRE and ERE inverted */
    {L"\\(grp\\)literal", 0, L"literal"},
    {L"(grp)literal", REG_EXTENDED, L"literal"},
    {L"x\\{1,2\\}literal", 0, L"literal"},
    {L"x{1,2}literal", REG_EXTENDED, L"literal"},
    /* Only in ERE */
    {L"x+literal", REG_EXTENDED, L"literal"},
    {L"x?literal", REG_EXTENDED, L"literal"}
};

START_TEST(loop_test_heur__successes__prefix_succeeds)
{
    heur_tuple current = prefix_successes[_i];
    heur *heur = run_and_return_prep(current.pattern, current.flags);

    ck_assert_msg(heur != NULL, "Preprocessing returned NULL heuristic!");

    int cmp = wcscmp(current.expected_segment, heur->literal_comp->wide.pattern);
    ck_assert_msg(cmp == 0,
                  "Preprocessing returned incorrect heuristic segment: returned '%ls', expected '%ls' for pattern '%ls' with flags '%d'",
                  heur->literal_comp->wide.pattern, current.expected_segment, current.pattern, current.flags
    );

    ck_assert_msg(heur->heur_type == HEUR_PREFIX,
        "Preprocessing incorrectly did not create prefix heuristics: returned '%d' for pattern '%ls' with flags '%d'",
        heur->heur_type, current.pattern, current.flags
    );
}
END_TEST

START_TEST(loop_test_heur__successes__longest_succeeds)
{
    heur_tuple current = longest_successes[_i];
    heur *heur = run_and_return_prep(current.pattern, current.flags);

    ck_assert_msg(heur != NULL, "Preprocessing returned NULL heuristic!");

    int cmp = wcscmp(current.expected_segment, heur->literal_comp->wide.pattern);
    ck_assert_msg(cmp == 0,
                  "Preprocessing returned incorrect heuristic segment: returned '%ls', expected '%ls' for pattern '%ls' with flags '%d'",
                  heur->literal_comp->wide.pattern, current.expected_segment, current.pattern, current.flags
    );

    ck_assert_msg(heur->heur_type == HEUR_LONGEST,
        "Preprocessing incorrectly did not create prefix heuristics: returned '%d' for pattern '%ls' with flags '%d'",
        heur->heur_type, current.pattern, current.flags
    );
}
END_TEST


static Suite *
create_suite()
{
	Suite *suite = suite_create("Heuristics");

	TCase *tc_prep = tcase_create("Preprocessing");

    tcase_add_test(tc_prep, test_heur__sanity__literal_ok);
    tcase_add_loop_test(tc_prep, loop_test_heur__successes__prefix_succeeds, 0, PREF_SUCC_LEN);
    tcase_add_loop_test(tc_prep, loop_test_heur__successes__longest_succeeds, 0, LONG_SUCC_LEN);

	suite_add_tcase(suite, tc_prep);

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
