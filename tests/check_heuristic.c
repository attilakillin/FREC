/*-
 * Copyright (C) 2012, 2018 Gabor Kovesdan <gabor@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <check.h>
#include <stdlib.h>
#include <wchar.h>

#include "heuristic.h"

#define HEUR_TEST_RETURN(name, pattern, flags, expected_return)			\
    START_TEST(name)								\
	{												\
		heur_t prep;                            	\
		wchar_t *patt = pattern;					\
		int ret = frec_preprocess_heur(&prep, patt, wcslen(patt), flags); 	\
		ck_assert_int_eq(expected_return, ret);		\
	}												\
	END_TEST

#define HEUR_TEST_SELECTED_PATTERN(name, in_pattern, flags, ex_pattern)	\
    START_TEST(name)								\
	{												\
		heur_t prep;                            	\
		wchar_t *patt = in_pattern;					\
		int ret = frec_preprocess_heur(&prep, patt, wcslen(patt), flags); 	\
		ck_assert_int_eq(REG_OK, ret);      		\
        wchar_t *expected = ex_pattern;             \
        ret = wcscmp(ex_pattern, prep.heur->wide.pattern);              \
        ck_assert_int_eq(0, ret);                   \
	}												\
	END_TEST

HEUR_TEST_RETURN(
    test_heur_prep__literal_ok,
	L"literal pattern", 0, REG_OK
)

HEUR_TEST_RETURN(
    test_heur_prep__prefix_ok,
	L"literal(pattern){1,2}", REG_EXTENDED, REG_OK
)

HEUR_TEST_RETURN(
    test_heur_prep__longest_ok,
	L"(prefix)? with pattern", REG_EXTENDED, REG_OK
)


HEUR_TEST_SELECTED_PATTERN(
    test_heur_prep__literal_has_correct_bm_pattern,
    L"literal pattern", 0, L"literal pattern"
)

Suite *create_heur_suite()
{
	Suite *suite = suite_create("Heuristics");

	TCase *tc_prep = tcase_create("Preprocessing");

	/* Sanity checks. */
	tcase_add_test(tc_prep, test_heur_prep__literal_ok);
	tcase_add_test(tc_prep, test_heur_prep__prefix_ok);
	tcase_add_test(tc_prep, test_heur_prep__longest_ok);

    /* Literal tests. */
    tcase_add_test(tc_prep, test_heur_prep__literal_has_correct_bm_pattern);

	suite_add_tcase(suite, tc_prep);

	return suite;
}

int main(void)
{
	Suite *suite = create_heur_suite();
	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_VERBOSE);
	int failed = srunner_ntests_failed(runner);
	srunner_free(runner);

	return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
