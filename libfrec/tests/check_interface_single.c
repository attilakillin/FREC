
#include <check.h>
#include <frec.h>

static void
compile_and_run(
	frec_match_t *matches, size_t match_cnt,
	const char *pattern, const char *text, int flags
) {
	frec_t prep;
	int ret = frec_regcomp(&prep, pattern, flags);

	ck_assert_msg(ret == REG_OK,
		"Compilation failed: returned '%d' for pattern '%s'",
		ret, pattern
	);

	ret = frec_regexec(&prep, text, match_cnt, matches, flags);

	ck_assert_msg(ret == REG_OK || ret == REG_NOMATCH,
		"Matching failed: returned '%d' for pattern '%s' and text '%s'",
		ret, pattern, text
	);
}


typedef struct match_tuple {
	const char *pattern;
	const char *text;
	int flags;
	size_t match_cnt;
	frec_match_t matches[10];
} match_tuple;

#define INPUT_LEN 5
static match_tuple inputs[INPUT_LEN] = {
	/* Literal matching */
	{"pattern", "text with pattern", 0, 2, {{10,17}, {-1,-1}}},
	{"many", "many many many many", 0, 5, {{0,4}, {5,9}, {10,14}, {15,19}, {-1,-1}}},
	
	/* Literal matching with escapes */
	{"\\$()\\$", "text with $()$ chars", 0, 2, {{10,14}, {-1,-1}}},

	/* Longest matching */
	{"p..ce", "piece peace pounce", 0, 3, {{0,5}, {6,11}, {-1,-1}}},
	{"[ai][cx]e", "words with the letter e but only axe matches", 0, 2, {{33,36}, {-1,-1}}}
};


START_TEST(loop_test_interface__comp_and_match__offsets_ok)
{
    frec_match_t actuals[10];
	match_tuple curr = inputs[_i];

	compile_and_run(actuals, 10, curr.pattern, curr.text, curr.flags);

	for (size_t i = 0; i < curr.match_cnt; i++) {
		frec_match_t actual = actuals[i];
		frec_match_t expect = curr.matches[i];

		ck_assert_msg(expect.soffset == actual.soffset,
			"Matching returned incorrect soffset: expected '%d', got '%d' for pattern '%s' and text '%s' with flags '%d'",
			expect.soffset, actual.soffset, curr.pattern, curr.text, curr.flags
		);

		ck_assert_msg(expect.eoffset == actual.eoffset,
			"Matching returned incorrect eoffset: expected '%d', got '%d' for pattern '%s' and text '%s' with flags '%d'",
			expect.eoffset, actual.eoffset, curr.pattern, curr.text, curr.flags
		);
	}
}
END_TEST

Suite *create_interface_single_suite()
{
	Suite *suite = suite_create("Interface / Single patterns");

	TCase *tc_offsets = tcase_create("Match offsets");

    tcase_add_loop_test(tc_offsets, loop_test_interface__comp_and_match__offsets_ok, 0, INPUT_LEN);

	suite_add_tcase(suite, tc_offsets);

	return suite;
}

int main(void)
{
	Suite *suite = create_interface_single_suite();
	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_VERBOSE);
	int failed = srunner_ntests_failed(runner);
	srunner_free(runner);

	return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

