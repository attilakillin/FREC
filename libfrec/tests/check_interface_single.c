
#include <check.h>
#include <frec.h>

START_TEST(test_exec__n_literal_finds_one_match)
{
    frec_t preg;
    char *patt = "literal";
    frec_regncomp(&preg, patt, strlen(patt), 0);

    frec_match_t results[10];
    char *t = "literal text";
    frec_regnexec(&preg, t, strlen(t), 1, results, 0);

    ck_assert_int_eq(0, results[0].soffset);
    ck_assert_int_eq(7, results[0].eoffset);
}
END_TEST

Suite *create_interface_single_suite()
{
	Suite *suite = suite_create("Interface / Single patterns");

	TCase *tc_lit = tcase_create("Literal tests");

    tcase_add_test(tc_lit, test_exec__n_literal_finds_one_match);

	suite_add_tcase(suite, tc_lit);

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

