
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
#include <tre/regex.h>
#include <stdlib.h>
#include <frec.h>

#include "mregex.h"
#include "boyer-moore.h"

/* 
 * Macro that can be used to test whether a literal Boyer-Moore preprocessing
 * on the given pattern returns the expected value or not.
 * - name: the name of the test case.
 * - pattern: the wide string pattern to run the preprocessing on.
 * - flags: regex compilation flags that are passed on to preprocessing.
 * - expected_return: the expected return value of the preprocessing.
 */
#define BM_TEST_PREP_LIT(name, pattern, flags, expected_return)				\
	START_TEST(name)								\
	{												\
		bm_preproc_t* prep = bm_create_preproc();	\
		wchar_t *patt = pattern;					\
		int ret = bm_preprocess_literal(prep, patt, wcslen(patt), flags); 	\
		bm_free_preproc(prep);						\
		ck_assert_int_eq(expected_return, ret);		\
	}												\
	END_TEST

/* 
 * Macro that can be used to test whether a full Boyer-Moore preprocessing
 * on the given pattern returns the expected value or not.
 * - name: the name of the test case.
 * - pattern: the wide string pattern to run the preprocessing on.
 * - flags: regex compilation flags that are passed on to preprocessing.
 * - expected_return: the expected return value of the preprocessing.
 */
#define BM_TEST_PREP_FULL(name, pattern, flags, expected_return)		\
	START_TEST(name)								\
	{												\
		bm_preproc_t* prep = bm_create_preproc();	\
		wchar_t *patt = pattern;					\
		int ret = bm_preprocess_full(prep, patt, wcslen(patt), flags);	\
		bm_free_preproc(prep);						\
		ck_assert_int_eq(expected_return, ret);		\
	}												\
	END_TEST

/* 
 * Macro that can be used to test whether a full Boyer-Moore execution
 * on the given text finds the match at the given position, or not.
 * - name: the name of the test case.
 * - pattern: the wide string pattern to test for.
 * - text: the text that is searched.
 * - cflags: regex compilation flags that are passed on to preprocessing.
 * - soff: the expected start offset of the match.
 * - eoff: the expected end offset of the match.
 */
#define BM_TEST_EXEC_MATCH(name, pattern, text, cflags, soff, eoff)			\
	START_TEST(name)								\
	{												\
		bm_preproc_t* prep = bm_create_preproc();	\
		wchar_t *patt = pattern;					\
		int ret = bm_preprocess_full(prep, patt, wcslen(patt), cflags);		\
		ck_assert(ret == REG_OK);					\
													\
		frec_match_t match;							\
		char *t = text;								\
		ret = bm_execute_stnd(&match, 1, prep, t, strlen(t), 0);			\
		bm_free_preproc(prep);						\
													\
		ck_assert_int_eq(REG_OK, ret);				\
		ck_assert_int_eq(soff, match.soffset);		\
		ck_assert_int_eq(eoff, match.eoffset);		\
	}												\
	END_TEST

/* 
 * Macro that can be used to test whether a full Boyer-Moore execution
 * on the given text correctly checks whether the text contains the pattern.
 * - name: the name of the test case.
 * - pattern: the wide string pattern to test for.
 * - text: the text that is searched.
 * - cflags: regex compilation flags that are passed on to preprocessing.
 * - expected_return: The expected return value of the execution.
 */
#define BM_TEST_EXEC_CHECK(name, pattern, text, cflags, expected_return)	\
	START_TEST(name)								\
	{												\
		bm_preproc_t* prep = bm_create_preproc();	\
		wchar_t *patt = pattern;					\
		int ret = bm_preprocess_full(prep, patt, wcslen(patt), cflags);	\
		ck_assert(ret == REG_OK);					\
													\
		char *t = text;								\
		ret = bm_execute_stnd(NULL, 0, prep, t, strlen(t), 0);				\
		bm_free_preproc(prep);						\
													\
		ck_assert_int_eq(expected_return, ret);		\
	}												\
	END_TEST

/* Preprocessing tests. */

BM_TEST_PREP_LIT(
	test_bm_prep_literal__ok,
	L"something", 0, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok,
	L"something", 0, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_brackets_basic,
	L"p[r]int", 0, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_brackets_ext,
	L"p[r]int", REG_EXTENDED, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_asterisk_basic,
	L"p*int", 0, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_asterisk_ext,
	L"p*int", REG_EXTENDED, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_parens_basic,
	L"print\\(ln\\)", 0, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_parens_ext,
	L"print(ln)", REG_EXTENDED, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_dot_basic,
	L"p.int", 0, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_dot_ext,
	L"p.int", REG_EXTENDED, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_pipe_ext,
	L"pr|int", REG_EXTENDED, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_plus_sign_ext,
	L"pr+nt", REG_EXTENDED, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_question_mark_ext,
	L"pri?nt", REG_EXTENDED, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_braces_basic,
	L"print\\{1,2\\}", 0, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__fail_when_pattern_has_braces_ext,
	L"print{1,2}", REG_EXTENDED, REG_BADPAT
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_brackets_basic,
	L"p\\[r]int", 0, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_brackets_ext,
	L"p\\[r]int", REG_EXTENDED, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_dot_basic,
	L"p\\.int", 0, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_dot_ext,
	L"p\\.int", REG_EXTENDED, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_asterisk_basic,
	L"p\\*int", 0, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_asterisk_ext,
	L"p\\*int", REG_EXTENDED, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_question_mark_ext,
	L"pri\\?nt", REG_EXTENDED, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_plus_sign_ext,
	L"pr\\+nt", REG_EXTENDED, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_pipe_ext,
	L"pri\\|nt", REG_EXTENDED, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_braces_ext,
	L"print\\{1,2}", REG_EXTENDED, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_braces_basic,
	L"print{1,2}", 0, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_parens_basic,
	L"print(ln)", 0, REG_OK
)

BM_TEST_PREP_FULL(
	test_bm_prep_full__ok_when_pattern_has_escaped_parens_ext,
	L"print\\(ln)", REG_EXTENDED, REG_OK
)


/* Execution tests. */

BM_TEST_EXEC_CHECK(
	test_bm_exec_stnd__ok_when_pattern_present,
	L"Wednesday that",
	"Astronomers announced on Wednesday that at last they had captured an image of the unobservable: a black hole",
	0, REG_OK
)

BM_TEST_EXEC_CHECK(
	test_bm_exec_stnd__ok_when_pattern_not_present,
	L"This string is not present",
	"Astronomers announced on Wednesday that at last they had captured an image of the unobservable: a black hole",
	0, REG_NOMATCH
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_is_same_as_text,
	L"the same text", "the same text", 0, 0, 13
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_brackets_basic,
	L"p\\[r]int", "Text that p[r]ints", 0, 10, 17
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_brackets_ext,
	L"p\\[r]int", "Text that p[r]ints", REG_EXTENDED, 10, 17
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_dot_basic,
	L"p\\.int", "Text that p.ints", 0, 10, 15
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_dot_ext,
	L"p\\.int", "Text that p.ints", REG_EXTENDED, 10, 15
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_asterisk_basic,
	L"p\\*int", "Text that p*ints", 0, 10, 15
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_asterisk_ext,
	L"p\\*int", "Text that p*ints", REG_EXTENDED, 10, 15
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_question_mark_ext,
	L"p\\?int", "Text that p?ints", REG_EXTENDED, 10, 15
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_plus_sign_ext,
	L"p\\+int", "Text that p+ints", REG_EXTENDED, 10, 15
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_pipe_ext,
	L"p\\|int", "Text that p|ints", REG_EXTENDED, 10, 15
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_braces_basic,
	L"print{1,2}", "Text that print{1,2}s", 0, 10, 20
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_braces_ext,
	L"print\\{1,2}", "Text that print{1,2}s", REG_EXTENDED, 10, 20
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_parens_basic,
	L"print(ln)", "Text that print(ln)s", 0, 10, 19
)

BM_TEST_EXEC_MATCH(
	test_bm_exec_stnd__match_when_pattern_has_escaped_parens_ext,
	L"print\\(ln)", "Text that print(ln)s", REG_EXTENDED, 10, 19
)


Suite *create_bm_suite()
{
	Suite *suite = suite_create("Boyer-Moore");

	TCase *tc_prep = tcase_create("Preprocessing");

	/* Sanity checks. */
	tcase_add_test(tc_prep, test_bm_prep_literal__ok);
	tcase_add_test(tc_prep, test_bm_prep_full__ok);

	/* Full prep failing cases. */
	tcase_add_test(tc_prep,	test_bm_prep_full__fail_when_pattern_has_brackets_basic);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_brackets_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_asterisk_basic);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_asterisk_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_parens_basic);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_parens_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_dot_basic);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_dot_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_pipe_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_plus_sign_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_question_mark_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_braces_basic);
	tcase_add_test(tc_prep, test_bm_prep_full__fail_when_pattern_has_braces_ext);

	/* Full prep passing cases. */
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_brackets_basic);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_brackets_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_dot_basic);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_dot_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_asterisk_basic);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_asterisk_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_question_mark_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_plus_sign_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_pipe_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_braces_ext);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_braces_basic);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_parens_basic);
	tcase_add_test(tc_prep, test_bm_prep_full__ok_when_pattern_has_escaped_parens_ext);

	TCase *tc_exec = tcase_create("Execution");

	/* Sanity checks */
	tcase_add_test(tc_exec, test_bm_exec_stnd__ok_when_pattern_present);
	tcase_add_test(tc_exec, test_bm_exec_stnd__ok_when_pattern_not_present);

	/* Execution passing cases */
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_is_same_as_text);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_brackets_basic);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_brackets_ext);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_dot_basic);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_dot_ext);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_asterisk_basic);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_asterisk_ext);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_question_mark_ext);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_plus_sign_ext);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_pipe_ext);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_braces_basic);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_braces_ext);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_parens_basic);
	tcase_add_test(tc_exec, test_bm_exec_stnd__match_when_pattern_has_escaped_parens_ext);
	
	suite_add_tcase(suite, tc_prep);
	suite_add_tcase(suite, tc_exec);

	return suite;
}

int main(void)
{
	Suite *suite = create_bm_suite();
	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_NORMAL);
	int failed = srunner_ntests_failed(runner);
	srunner_free(runner);

	return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
