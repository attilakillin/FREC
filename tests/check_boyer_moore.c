
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

START_TEST(test_boyer_moore_compile)
{
	fastmatch_t fm;
	wchar_t *wpat = L"something";
	char *pat = "something";
	int ret;

	ret = frec_proc_literal(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_square_brackets_basic)
{
	fastmatch_t fm;
	char *pat = "[ap]rint";
	wchar_t *wpat = L"[ap]rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret==REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_square_brackets_ext)
{
	fastmatch_t fm;
	char *pat = "[ap]rint";
	wchar_t *wpat = L"[ap]rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_asterix_basic)
{
	fastmatch_t fm;
	char *pat = "p*int";
	wchar_t *wpat= L"p*int";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret==REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_asterix_ext)
{
	fastmatch_t fm;
	char *pat = "*rint";
	wchar_t *wpat = L"*rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret==REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_round_brackets_basic)
{
	fastmatch_t fm;
	char *pat = "print\\(ln\\)";
	wchar_t *wpat = L"print\\(ln\\)";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret==REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_round_brackets_ext)
{
	fastmatch_t fm;
	char *pat = "print(ln)";
	wchar_t *wpat = L"print(ln)";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_dot_basic)
{
	fastmatch_t fm;
	char *pat= "prin.";
	wchar_t *wpat =L"prin.";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_dot_ext)
{
	fastmatch_t fm;
	char *pat = "prin.";
	wchar_t *wpat = L"prin.";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_vertical_bar_ext)
{
	fastmatch_t fm;
	char *pat = "print|prnt";
	wchar_t *wpat = L"print|prnt";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_plus_sign_ext)
{
	fastmatch_t fm;
	char *pat = "er+or";
	wchar_t *wpat = L"er+or";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_BADPAT);
}
END_TEST

/*
START_TEST(test_boyer_moore_badpat_7_ext)
{
	fastmatch_t fm;
	char *pat = "[pP]rint";
	wchar_t *wpat = L"[pP]rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_BADPAT);
}
END_TEST

START_TEST(test_boyer_moore_badpat_7_basic)
{
	fastmatch_t fm;
	char *pat = "[pP]rint";
	wchar_t *wpat = L"[pP]rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_BADPAT);
}
END_TEST
*/

START_TEST(test_proc_fast_compile_fails_when_pattern_has_question_mark_ext)
{
	fastmatch_t fm;
	char *pat = "pri?nt";
	wchar_t *wpat = L"pri?nt";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_braces_ext)
{
	fastmatch_t fm;
	char *pat = "print{1,2}";
	wchar_t *wpat = L"print{1,2}";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_fails_when_pattern_has_braces_basic)
{
	fastmatch_t fm;
	char *pat = "print\\{1,2\\}";
	wchar_t *wpat = L"print\\{1,2\\}";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_BADPAT);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_square_brackets_basic)
{
	fastmatch_t fm;
	char *pat = "\\[pP]rint";
	wchar_t *wpat = L"\\[pP]rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_square_brackets_ext)
{
	fastmatch_t fm;
	char *pat = "\\[pP]rint";
	wchar_t *wpat = L"\\[pP]rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);
	
	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_dot_ext)
{
	fastmatch_t fm;
	char *pat = "\\.rint";
	wchar_t *wpat = L"\\.rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_dot_basic)
{
	fastmatch_t fm;
	char *pat = "\\.rint";
	wchar_t *wpat = L"\\.rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_asterix_basic)
{
	fastmatch_t fm;
	char *pat = "\\*rint";
	wchar_t *wpat = L"\\*rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_asterix_ext)
{
	fastmatch_t fm;
	char *pat = "\\*rint";
	wchar_t *wpat = L"\\*rint";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_question_mark_ext)
{
	fastmatch_t fm;
	char *pat = "prin\\?t";
	wchar_t *wpat = L"prin\\?t";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_plus_sign_ext)
{
	fastmatch_t fm;
	char *pat = "pri\\+nt";
	wchar_t *wpat = L"pri\\+nt";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_vertical_bar_ext)
{
	fastmatch_t fm;
	char *pat = "print\\|prnt";
	wchar_t *wpat = L"print\\|prnt";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_braces_ext)
{
	fastmatch_t fm;
	char *pat = "print\\{1,2}";
	wchar_t *wpat = L"print\\{1,2}";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_braces_basic)
{
	fastmatch_t fm;
	char *pat = "print{1,2}";
	wchar_t *wpat = L"print{1,2}";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_round_brackets_basic)
{
	fastmatch_t fm;
	char *pat = "print(ln)";
	wchar_t *wpat = L"print(ln)";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_proc_fast_compile_ok_when_pattern_has_escaped_round_brackets_ext)
{
	fastmatch_t fm;
	char *pat = "print\\(ln)";
	wchar_t *wpat = L"print\\(ln)";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);
}
END_TEST

START_TEST(test_frec_match_fast)
{
	fastmatch_t fm;
	wchar_t *wpat = L"Wednesday that";
	char *pat = "Wednesday that";
	char *text = "Astronomers announced on Wednesday that at last they had captured an image of the unobservable: a black hole";
	int ret;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret==REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 0, NULL, 0);

	ck_assert(ret==REG_OK); //ck_assert(pmatch offset ? )
}
END_TEST

START_TEST(test_frec_match_fast_ok_when_pattern_has_escaped_brackets_basic)
{
	fastmatch_t fm;
	char *pat = "\\[pP]rint";
	wchar_t *wpat = L"\\[pP]rint";
	char *text ="To [pP]rint";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, 0);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==11);
}
END_TEST

START_TEST(test_frec_match_fast_ok_when_pattern_has_escaped_brackets_ext)
{
	fastmatch_t fm;
	char *pat = "\\[pP]rint";
	wchar_t *wpat = L"\\[pP]rint";
	char *text ="To [pP]rint";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, REG_EXTENDED);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==11);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_dot_ext)
{
	fastmatch_t fm;
	char *pat = "\\.rint";
	wchar_t *wpat = L"\\.rint";
	char *text ="To .rint";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, REG_EXTENDED);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==8);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_dot_basic)
{
	fastmatch_t fm;
	char *pat = "\\.rint";
	wchar_t *wpat = L"\\.rint";
	char *text ="To .rint";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, 0);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==8);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_asterix_basic)
{
	fastmatch_t fm;
	char *pat = "\\*rint";
	wchar_t *wpat = L"\\*rint";
	char *text ="To *rint";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, 0);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==8);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_asterix_ext)
{
	fastmatch_t fm;
	char *pat = "\\*rint";
	wchar_t *wpat = L"\\*rint";
	char *text ="To *rint";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, REG_EXTENDED);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==8);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_question_mark_ext)
{
	fastmatch_t fm;
	char *pat = "prin\\?t";
	wchar_t *wpat = L"prin\\?t";
	char *text = "To prin?t";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, REG_EXTENDED);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==9);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_plus_sign_ext)
{
	fastmatch_t fm;
	char *pat = "pri\\+nt";
	wchar_t *wpat = L"pri\\+nt";
	char *text = "To pri+nt";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, REG_EXTENDED);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==9);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_vertical_bar_ext)
{
	fastmatch_t fm;
	char *pat = "print\\|prnt";
	wchar_t *wpat = L"print\\|prnt";
	char *text = "To print|prnt";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, REG_EXTENDED);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==13);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_braces_ext)
{
	fastmatch_t fm;
	char *pat = "print\\{1,2}";
	wchar_t *wpat = L"print\\{1,2}";
	char *text = "To print{1,2}";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, REG_EXTENDED);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==13);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_braces_basic)
{
	fastmatch_t fm;
	char *pat = "print{1,2}";
	wchar_t *wpat = L"print{1,2}";
	char *text = "To print{1,2}";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, 0);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==13);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_round_brackets_basic)
{
	fastmatch_t fm;
	char *pat = "print(ln)";
	wchar_t *wpat = L"print(ln)";
	char *text = "To print(ln)";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), 0);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, 0);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==12);
}
END_TEST

START_TEST(test_frec_match_ok_when_pattern_has_escaped_round_brackets_ext)
{
	fastmatch_t fm;
	char *pat = "print\\(ln)";
	wchar_t *wpat = L"print\\(ln)";
	char *text = "To print(ln)";
	int ret;
	frec_match_t pmatch;

	ret = frec_proc_fast(&fm, wpat, wcslen(wpat), pat, strlen(pat), REG_EXTENDED);

	ck_assert(ret == REG_OK);

	ret = frec_match_fast(&fm, text, strlen(text), STR_BYTE, 1, &pmatch, REG_EXTENDED);

	ck_assert(pmatch.m.rm_so ==3);
	ck_assert(pmatch.m.rm_eo ==12);
}
END_TEST

Suite *boyer_moore_suite(void)
{
	Suite *s = suite_create("Boyer-Moore");
	TCase *tc_comp = tcase_create("Compilation");

	tcase_add_test(tc_comp, test_boyer_moore_compile);
	tcase_add_test(tc_comp,	test_proc_fast_compile_fails_when_pattern_has_square_brackets_basic);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_square_brackets_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_asterix_basic);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_asterix_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_round_brackets_basic);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_round_brackets_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_dot_basic);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_dot_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_vertical_bar_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_plus_sign_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_question_mark_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_braces_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_fails_when_pattern_has_braces_basic);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_square_brackets_basic);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_square_brackets_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_dot_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_dot_basic);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_asterix_basic);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_asterix_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_question_mark_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_plus_sign_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_vertical_bar_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_braces_ext);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_braces_basic);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_round_brackets_basic);
	tcase_add_test(tc_comp, test_proc_fast_compile_ok_when_pattern_has_escaped_round_brackets_ext);
	tcase_add_test(tc_comp, test_frec_match_fast);
	tcase_add_test(tc_comp, test_frec_match_fast_ok_when_pattern_has_escaped_brackets_basic);
	tcase_add_test(tc_comp, test_frec_match_fast_ok_when_pattern_has_escaped_brackets_ext);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_dot_ext);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_dot_basic);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_asterix_basic);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_asterix_ext);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_question_mark_ext);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_plus_sign_ext);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_vertical_bar_ext);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_braces_basic);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_braces_ext);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_round_brackets_ext);
	tcase_add_test(tc_comp, test_frec_match_ok_when_pattern_has_escaped_round_brackets_basic);
	suite_add_tcase(s, tc_comp);

	return s;
}

int main(void)
{
	Suite *s = boyer_moore_suite();
	SRunner *sr = srunner_create(s);
	int number_failed;

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
