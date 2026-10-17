#!/usr/bin/env python3
"""Regression tests for Ryu discovery and forbidden-pattern checks."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


CBASE = Path(__file__).resolve().parent
CHECKER = CBASE / "functions_forbidden_patterns.pl"
SHELL_CHECKER = CBASE / "functions_forbidden.sh"


class ForbiddenPatternsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.environment = dict(os.environ)
        self.environment["CBASE_FORBIDDEN_PATTERNS_CACHE"] = str(
            self.root / "patterns-cache.json"
        )
        self.environment["CBASE_STATUS_TEMPORARY_PATTERNS_CACHE"] = str(
            self.root / "status-cache.json"
        )

    def write(self, name, contents):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")
        return path

    def run_perl(self, name, contents, *, file_list=False):
        path = self.write(name, contents)
        args = ["perl", str(CHECKER)]
        if file_list:
            paths = self.write("sources.list", str(path) + "\n")
            args.extend(["--file-list", str(paths)])
        else:
            args.append(str(path))
        result = subprocess.run(
            args, cwd=self.root, env=self.environment,
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout

    def prepare_shell_checker(self):
        cbase = self.root / "cbase"
        cbase.mkdir(exist_ok=True)
        (cbase / "functions_forbidden_patterns.pl").symlink_to(CHECKER)
        self.write("cbase/functions_never_use.txt", "")
        self.write("cbase/functions_allowed_cbase_only.txt", "")
        self.write("cbase/status_temporary_patterns.py", "")

    def run_shell(self, **settings):
        env = dict(self.environment)
        for name in ("TEST_FILTER", "TEST_MAXDEPTH", "TEST_EXCLUDE_PATTERN"):
            env.pop(name, None)
        env.update(settings)
        result = subprocess.run(
            ["sh", "-c", 'error() { printf "$@" >&2; }; . "$1"',
             "check", str(SHELL_CHECKER)],
            cwd=self.root, env=env, capture_output=True, text=True,
            check=False,
        )
        return result

    def test_ryu_ternary_and_fast_math_are_permitted(self):
        output = self.run_perl(
            "cbase/ryu/sample.c",
            "#include <stdint.h>\n"
            "static uint64_t\n"
            "value(uint64_t n) {\n"
            "    return n ? ((__uint128_t)n >> 1) : sizeof(n);\n"
            "}\n",
        )
        self.assertEqual(output, "")

    # def test_non_ryu_ternary_is_rejected_but_macro_is_not(self):
    #     output = self.run_perl(
    #         "cbase/sample.c",
    #         "#define CHOOSE(a, b) ((a) ? (a) : (b))\n"
    #         "static int\n"
    #         "choose(int x) {\n"
    #         "    return x ? 1 : 0;\n"
    #         "}\n"
    #         "static char\n"
    #         "question(void) {\n"
    #         "    return '?'; // ? in a comment\n"
    #         "}\n",
    #     )
    #     self.assertEqual(output.count("ternary operator is forbidden"), 1)
    #     self.assertIn("sample.c:4:ternary operator", output)

    def test_ryu_whitespace_and_preprocessor_violations(self):
        output = self.run_perl(
            "cbase/ryu/sample.h",
            "#ifndef RYU_SAMPLE_H\n"
            "#define RYU_SAMPLE_H\n"
            "\tint32 value;  \n"
            "// " + "x" * 79 + "\n"
            "#endif // RYU_SAMPLE_H\n",
            file_list=True,
        )
        for rule in (
            "use #if defined or #if !defined",
            "tab character is forbidden",
            "trailing whitespace",
            "line exceeds 80 columns",
            "use a block comment after #endif",
        ):
            self.assertIn(rule, output)

    def test_project_source_whitespace_violations(self):
        output = self.run_perl(
            "src/sample.c",
            "\tint32 value;  \n" +
            "x" * 80 + "\n" +
            "x" * 81 + "\n",
        )
        for rule in (
            "tab character is forbidden",
            "trailing whitespace",
            "line exceeds 80 columns",
        ):
            self.assertIn(rule, output)
        self.assertEqual(output.count("line exceeds 80 columns"), 1)
        self.assertIn("sample.c:3:line exceeds 80 columns", output)

    def test_multiplication_rejects_spaces_on_either_side(self):
        output = self.run_perl(
            "src/sample.c",
            "int32 product = first * second;\n"
            "product = first* second;\n"
            "product = first *second;\n"
            "product = one * (two + three);\n"
            "product = values[0] * -second;\n"
            "product = (left + right) * function(x);\n"
            "product = 2.0 * 5;\n"
            "#define SQUARE(x) ((x) * (x))\n"
            "product = left*right;\n"
            "product *= amount;\n",
        )
        self.assertEqual(output.count("space around multiplication"), 8)
        for line in range(1, 9):
            self.assertIn(
                f"sample.c:{line}:space around multiplication", output,
            )
        self.assertNotIn("sample.c:9:space around multiplication", output)
        self.assertNotIn("sample.c:10:space around multiplication", output)

    def test_multiplication_ignores_pointers_comments_and_strings(self):
        output = self.run_perl(
            "src/sample.c",
            "char *pointer;\n"
            "int32 * value;\n"
            "CySystemRuntime *sys;\n"
            "CustomType * object;\n"
            "struct CustomType *member;\n"
            "double * restrict data;\n"
            "char **argv;\n"
            "void (*callback)(void);\n"
            "typedef uint32 *Counter;\n"
            "int32 n = SIZEOF(*pointer);\n"
            "int32 result = * pointer;\n"
            "int32 cast = (int32 *)pointer != NULL;\n"
            "#define PTR(T) T *value\n"
            "// x * y\n"
            'char *message = "x * y";\n'
            "/* x * y */\n"
            "int32 product = left*right;\n"
            "product *= result;\n",
        )
        self.assertNotIn("space around multiplication", output)

    def test_multiplication_checks_expression_even_with_type_like_name(self):
        output = self.run_perl(
            "src/sample.c",
            "value = CustomType * other;\n"
            "value = ptr->CustomType * other;\n"
            "value = table[0] * other;\n"
            "value = SIZEOF(value) * other;\n"
            "value = left /* explanation */ * other;\n",
        )
        self.assertEqual(output.count("space around multiplication"), 5)

    def test_multiplication_accepts_declarator_macros_and_unary_star(self):
        output = self.run_perl(
            "src/sample.c",
            "HASH_KEY_TYPE *key;\n"
            "CONDITION_VARIABLE *condition;\n"
            "FILETIME *filetime;\n"
            "const char *const *names;\n"
            "CAT(SYS_NAME, _Data) *pid = data;\n"
            "#define TRAP(...) *(int *)0 = 0\n"
            "*pointer = value;\n"
            "return *pointer;\n",
        )
        self.assertNotIn("space around multiplication", output)

    def test_multiplication_not_confused_by_constant_names(self):
        output = self.run_perl(
            "src/sample.c",
            "value = compute(N * x);\n"
            "value = N * x;\n",
        )
        self.assertEqual(output.count("space around multiplication"), 2)

    def test_shell_checker_reports_multiplication_spacing(self):
        self.prepare_shell_checker()
        self.write("src/sample.c", "int32 product = a * b;\n")
        result = self.run_shell(TEST_FILTER="sample")
        self.assertEqual(result.returncode, 1)
        self.assertIn("space around multiplication", result.stderr)

    def test_multiplication_ryu_is_exempt(self):
        output = self.run_perl(
            "cbase/ryu/sample.c",
            "int64 result = first * second;\n",
        )
        self.assertNotIn("space around multiplication", output)

    def test_multiplication_checker_cache_invalidates(self):
        path = "src/sample.c"
        self.assertEqual(self.run_perl(path, "int32 n = a*b;\n"), "")
        output = self.run_perl(path, "int32 n = a * b;\n")
        self.assertIn("sample.c:1:space around multiplication", output)
        self.assertEqual(self.run_perl(path, "int32 n = a*b;\n"), "")

    def test_project_source_unicode_column_limit(self):
        output = self.run_perl(
            "src/sample.c",
            "я" * 80 + "\n" +
            "я" * 81 + "\n",
        )
        self.assertEqual(output.count("line exceeds 80 columns"), 1)
        self.assertIn("sample.c:2:line exceeds 80 columns", output)

    def test_generated_meta_headers_are_ignored(self):
        contents = "#ifdef FEATURE\n\tint value = sizeof(int);  \n#endif\n"
        self.assertEqual(
            self.run_perl("src/sample.meta.h", contents), "",
        )
        self.assertEqual(
            self.run_perl("src/sample.meta.h", contents, file_list=True), "",
        )

    def test_cecup_source_accepts_up_to_100_columns(self):
        output = self.run_perl(
            "cecup/src/sample.c",
            "x" * 100 + "\n" +
            "x" * 101 + "\n",
        )
        self.assertEqual(output.count("line exceeds 100 columns"), 1)
        self.assertIn("sample.c:2:line exceeds 100 columns", output)

    def test_ifdef_ifndef_are_rejected_in_project_sources(self):
        output = self.run_perl(
            "src/sample.c",
            "#ifdef FEATURE\n"
            "#  ifndef OTHER_FEATURE\n"
            "#    ifdef NESTED_FEATURE\n"
            "#    endif\n"
            "#  endif\n"
            "#endif\n",
        )
        self.assertEqual(output.count("use #if defined or #if !defined"), 3)
        for line in (1, 2, 3):
            self.assertIn(f"sample.c:{line}:use #if defined", output)

    def test_ifdef_ifndef_are_rejected_in_headers_and_file_lists(self):
        output = self.run_perl(
            "src/sample.h",
            "# ifdef FIRST\n"
            "# endif\n"
            "#\tifndef SECOND\n"
            "# endif\n",
            file_list=True,
        )
        self.assertEqual(output.count("use #if defined or #if !defined"), 2)
        self.assertIn("sample.h:1:use #if defined", output)
        self.assertIn("sample.h:3:use #if defined", output)

    def test_preprocessor_conditional_valid_forms_are_ignored(self):
        output = self.run_perl(
            "src/sample.h",
            "#if defined(FEATURE)\n"
            "#elif defined(SECOND_FEATURE)\n"
            "#endif\n"
            "#if !defined(THIRD_FEATURE)\n"
            "#endif\n"
            "#if FLAG_VALUE\n"
            "#endif\n"
            "#define text_ifdef #ifdef\n",
        )
        self.assertNotIn("use #if defined or #if !defined", output)

    def test_preprocessor_conditional_comments_strings_are_ignored(self):
        output = self.run_perl(
            "src/sample.c",
            "// #ifdef COMMENT\n"
            "/* #ifndef OTHER_COMMENT */\n"
            'char *text = "#ifdef STRING";\n'
            "/*\n"
            "#ifdef MULTILINE_COMMENT\n"
            "*/\n"
            "# /* ignored comment */ ifdef REAL_DIRECTIVE\n",
        )
        self.assertEqual(output.count("use #if defined or #if !defined"), 1)
        self.assertIn("sample.c:7:use #if defined", output)

    def test_preprocessor_conditionals_ryu_not_reported_twice(self):
        output = self.run_perl(
            "cbase/ryu/sample.h",
            "#ifndef GUARD\n"
            "#define GUARD\n"
            "#endif\n",
        )
        self.assertEqual(output.count("use #if defined or #if !defined"), 1)

    def test_preprocessor_conditional_cache_invalidates_changes(self):
        path = "src/sample.c"
        self.assertEqual(self.run_perl(path, "#if defined(FOO)\n"), "")
        output = self.run_perl(path, "#ifdef FOO\n")
        self.assertIn("sample.c:1:use #if defined", output)
        self.assertEqual(self.run_perl(path, "#if defined(FOO)\n"), "")

    def test_builtin_sizeof_is_rejected_in_project_code(self):
        output = self.run_perl(
            "src/sample.c",
            "static int32\n"
            "example(int32 *pointer) {\n"
            "    int64 size = sizeof(*pointer);\n"
            "    size += sizeof pointer;\n"
            "    size += sizeof\n"
            "            (int32);\n"
            "    size += sizeof *pointer;\n"
            "    return (int32)size;\n"
            "}\n",
        )
        self.assertEqual(output.count("use SIZEOF instead of sizeof"), 4)
        for line in (3, 4, 5, 7):
            self.assertIn(f"sample.c:{line}:use SIZEOF", output)

    def test_builtin_sizeof_is_rejected_in_headers_and_macros(self):
        output = self.run_perl(
            "src/sample.h",
            "#define RAW_SIZE(X) sizeof(X)\n"
            "#define RAW_SIZE_MULTILINE(X) \\\n"
            "    sizeof(X)\n"
            "enum { VALUE_SIZE = sizeof(int32) };\n",
            file_list=True,
        )
        self.assertEqual(output.count("use SIZEOF instead of sizeof"), 3)
        for line in (1, 3, 4):
            self.assertIn(f"sample.h:{line}:use SIZEOF", output)

    def test_builtin_sizeof_ignores_wrapper_definition(self):
        output = self.run_perl(
            "cbase/base_macros.h",
            "#define SIZEOF(X) ((int64)sizeof(X))\n"
            "#  define SIZEOF(X) \\\n"
            "    ((int64)sizeof(X))\n"
            "#define LENGTH(X) (sizeof(X) / sizeof(*(X)))\n"
            "int64 a = SIZEOF(value);\n",
        )
        self.assertEqual(output.count("use SIZEOF instead of sizeof"), 2)
        self.assertIn("base_macros.h:4:use SIZEOF", output)
        self.assertNotIn("base_macros.h:1:use SIZEOF", output)
        self.assertNotIn("base_macros.h:3:use SIZEOF", output)

    def test_builtin_sizeof_ignores_literals_comments_and_names(self):
        output = self.run_perl(
            "src/sample.c",
            "// sizeof(value)\n"
            "/* sizeof(int32) */\n"
            'char *message = "sizeof(value)";\n'
            "char letter = 's';\n"
            "int64 my_sizeof = 3;\n"
            "int64 sizeof_value = SIZEOF(int32);\n"
            "XX(C_KEYWORD_SIZEOF, sizeof)\n",
        )
        self.assertNotIn("use SIZEOF instead of sizeof", output)

    def test_builtin_sizeof_is_exempt_in_ryu(self):
        output = self.run_perl(
            "cbase/ryu/sample.c",
            "int64 value = sizeof(value);\n",
        )
        self.assertNotIn("use SIZEOF instead of sizeof", output)

    def test_builtin_sizeof_checker_cache_invalidates(self):
        path = "src/sample.c"
        self.assertEqual(self.run_perl(path, "int64 n = SIZEOF(n);\n"), "")
        output = self.run_perl(path, "int64 n = sizeof(n);\n")
        self.assertIn("sample.c:1:use SIZEOF", output)
        self.assertEqual(self.run_perl(path, "int64 n = SIZEOF(n);\n"), "")

    def test_assignment_equals_cannot_begin_continuation_line(self):
        output = self.run_perl(
            "src/sample.c",
            "static void\n"
            "work(void) {\n"
            "    my_variable\n"
            "        = this_function_is_long_and_had_to_break_the_line(\n"
            "            100, 200, arg, other_arg);\n"
            "    options->state\n"
            "        = calculate(1);\n"
            "    int32 value\n"
            "= 42;\n"
            "}\n",
        )
        self.assertEqual(output.count("assignment = must end"), 3)
        for line in (4, 7, 9):
            self.assertIn(
                f"sample.c:{line}:assignment = must end", output,
            )

    def test_assignment_equals_may_end_line_even_when_expression_long(self):
        output = self.run_perl(
            "src/sample.c",
            "    my_variable =\n"
            "        this_function_is_long_and_had_to_break_the_line(\n"
            "            100, 200, arg, other_arg);\n",
        )
        self.assertNotIn("assignment = must end", output)

    def test_comparison_equals_may_begin_line(self):
        output = self.run_perl(
            "src/sample.c",
            "    same = first\n"
            "        == second;\n"
            "    same = (first\n"
            "            == second);\n"
            "    equals = (first ==\n"
            "              second);\n",
        )
        self.assertNotIn("assignment = must end", output)

    def test_leading_equals_ignores_comments_literals_and_macros(self):
        output = self.run_perl(
            "src/sample.c",
            "    // = not an assignment\n"
            "    /* comment\n"
            "    = not an assignment\n"
            "    */\n"
            "    char *text = \"= still a literal\";\n"
            "#define TEST(value) value \\\n"
            "    = value\n"
            "#define OTHER(value) \\\n"
            "    = value\n",
        )
        self.assertNotIn("assignment = must end", output)

    def test_leading_equals_still_checked_in_cecup_but_not_ryu_or_meta(self):
        source = "    value\n        = compute();\n"
        self.assertIn(
            "sample.c:2:assignment = must end",
            self.run_perl("cecup/src/sample.c", source),
        )
        for path in ("cbase/ryu/sample.c", "src/example.meta.h"):
            self.assertNotIn(
                "assignment = must end", self.run_perl(path, source),
            )

    def test_short_assignments_split_after_equals_are_rejected(self):
        output = self.run_perl(
            "src/sample.c",
            "static void\n"
            "work(void) {\n"
            "    my_variable =\n"
            "        this_is_a_function_call(args, other_arg);\n"
            "    int32 value =\n"
            "        calculate(1);\n"
            "    options->state =\n"
            "        flags;\n"
            "}\n",
        )
        self.assertEqual(output.count("assignment fits on one line"), 3)
        for line in (3, 5, 7):
            self.assertIn(
                f"sample.c:{line}:assignment fits on one line", output,
            )

    def test_assignment_wraps_check_complete_expression_length(self):
        prefix = "    result ="
        fits = "x" * (80 - len(prefix) - len(" ;"))
        too_long = fits + "x"
        output = self.run_perl(
            "src/sample.c",
            prefix + "\n        " + fits + ";\n"
            + "\n"
            + prefix + "\n        " + too_long + ";\n"
            + "\n"
            + "    result =\n"
            + "        calculate(first,\n"
            + "                  second);\n"
            + "\n"
            + "    result =\n"
            + "        some_function_with_an_extremely_long_name(\n"
            + "            first_really_long_argument, second_argument);\n",
        )
        self.assertEqual(output.count("assignment fits on one line"), 2)
        self.assertIn("sample.c:1:assignment fits on one line", output)
        self.assertIn("sample.c:7:assignment fits on one line", output)
        self.assertNotIn("sample.c:4:assignment fits on one line", output)
        self.assertNotIn("sample.c:11:assignment fits on one line", output)

    def test_assignment_wraps_preserve_longer_related_assignments(self):
        output = self.run_perl(
            "src/sample.c",
            "    first_value =\n"
            "        quick();\n"
            "    second_value =\n"
            "        some_function_with_a_long_name(first_long_argument,\n"
            "                                       second_long_argument);\n"
            "    third_value =\n"
            "        quick();\n"
            "\n"
            "    fourth_value =\n"
            "        quick();\n"
            "    fifth_value = quick();\n",
        )
        self.assertEqual(output.count("assignment fits on one line"), 1)
        self.assertIn("sample.c:9:assignment fits on one line", output)

    def test_assignment_wraps_group_long_assignments_broken_elsewhere(self):
        output = self.run_perl(
            "src/sample.c",
            "    first_value =\n"
            "        quick();\n"
            "    second_value = some_function_with_an_extremely_long_name(\n"
            "        first_really_long_argument, second_long_argument);\n"
            "    third_value =\n"
            "        quick();\n"
            "\n"
            "    fourth_value =\n"
            "        quick();\n",
        )
        self.assertEqual(output.count("assignment fits on one line"), 1)
        self.assertIn("sample.c:8:assignment fits on one line", output)

    def test_assignment_wraps_are_grouped_across_comments_not_blank_lines(self):
        output = self.run_perl(
            "src/sample.c",
            "    first_value =\n"
            "        quick();\n"
            "    // These values are related.\n"
            "    second_value =\n"
            "        long_named_function_with_many_arguments(\n"
            "            first_really_long_argument, second_long_argument);\n"
            "    third_value =\n"
            "        quick();\n"
            "    \n"
            "    fourth_value =\n"
            "        quick();\n",
        )
        self.assertEqual(output.count("assignment fits on one line"), 1)
        self.assertIn("sample.c:10:assignment fits on one line", output)

    def test_assignment_wraps_do_not_cross_unrelated_statements(self):
        output = self.run_perl(
            "src/sample.c",
            "    first_value =\n"
            "        quick();\n"
            "    unrelated_call();\n"
            "    second_value =\n"
            "        some_function_with_an_extremely_long_name(\n"
            "            first_really_long_argument, second_long_argument);\n",
        )
        self.assertEqual(output.count("assignment fits on one line"), 1)
        self.assertIn("sample.c:1:assignment fits on one line", output)

    def test_assignment_wraps_group_with_unwrapped_long_assignment(self):
        output = self.run_perl(
            "src/sample.c",
            "    first =\n"
            "        small();\n"
            "    long_value = " + "some_very_long_identifier_" * 3 + ";\n"
            "    third =\n"
            "        small();\n"
            "\n"
            "    fourth =\n"
            "        small();\n",
        )
        self.assertEqual(output.count("assignment fits on one line"), 1)
        self.assertIn("sample.c:7:assignment fits on one line", output)

    def test_assignment_wraps_group_obeys_column_limit(self):
        prefix = "    longer_value ="
        rhs = "x" * (90 - len(prefix) - len(" ;"))
        code = (
            "    short_value =\n"
            "        small();\n"
            + prefix + "\n        " + rhs + ";\n"
        )
        normal = self.run_perl("src/sample.c", code)
        cecup = self.run_perl("cecup/src/sample.c", code)
        self.assertNotIn("assignment fits on one line", normal)
        self.assertEqual(cecup.count("assignment fits on one line"), 2)

    def test_assignment_wraps_allow_braces_inside_string_literals(self):
        output = self.run_perl(
            "src/sample.c",
            '    pattern =\n'
            '        "{value}";\n',
        )
        self.assertIn("sample.c:1:assignment fits on one line", output)

    def test_only_assignment_lines_ending_with_bare_equals_are_checked(self):
        output = self.run_perl(
            "src/sample.c",
            "    value = function(\n"
            "        arg);\n"
            "    value = first +\n"
            "        second;\n"
            "    if (value ==\n"
            "        other) {}\n"
            "    value +=\n"
            "        amount;\n"
            "    value *=\n"
            "        factor;\n"
            "    value <=\n"
            "        maximum;\n"
            "    value = function(arg);\n",
        )
        self.assertNotIn("assignment fits on one line", output)

    def test_assignment_wraps_ignore_comments_directives_and_braces(self):
        output = self.run_perl(
            "src/sample.c",
            "    // fake =\n"
            "        call();\n"
            "    /* example =\n"
            "        value; */\n"
            "    value =\n"
            "        /* explanation */ call();\n"
            "    struct Item item =\n"
            "        { 1, 2 };\n"
            "#define COMPLEX_ASSIGNMENT() do { \\\n"
            "    value = \\\n"
            "        calculate(); \\\n"
            "} while (0)\n"
            "    value =\n"
            "#if defined(FEATURE)\n"
            "        foo;\n"
            "#else\n"
            "        bar;\n"
            "#endif\n",
        )
        self.assertNotIn("assignment fits on one line", output)

    def test_assignment_wrap_check_accepts_literal_and_cache_changes(self):
        path = "src/sample.c"
        wrapped = '    char *message =\n        "hello";\n'
        self.assertIn(
            "sample.c:1:assignment fits on one line",
            self.run_perl(path, wrapped, file_list=True),
        )
        self.assertIn(
            "sample.c:1:assignment fits on one line",
            self.run_perl(path, wrapped, file_list=True),
        )
        self.assertNotIn(
            "assignment fits on one line",
            self.run_perl(path, '    char *message = "hello";\n'),
        )

    def test_assignment_wrap_obeys_cecup_column_limit_and_ryu_exclusion(self):
        prefix = "    int32 variable ="
        rhs = "x" * (90 - len(prefix) - len(" ;"))
        wrapped = prefix + "\n        " + rhs + ";\n"
        regular = self.run_perl("src/sample.c", wrapped)
        cecup = self.run_perl("cecup/src/sample.c", wrapped)
        ryu = self.run_perl("cbase/ryu/sample.c", wrapped)
        self.assertNotIn("assignment fits on one line", regular)
        self.assertIn("assignment fits on one line", cecup)
        self.assertNotIn("assignment fits on one line", ryu)

    def test_ryu_missing_final_newline(self):
        output = self.run_perl("cbase/ryu/sample.c", "int x;")
        self.assertIn("missing final newline", output)

    def test_shell_discovers_ryu_below_depth_limit(self):
        self.prepare_shell_checker()
        self.write("cbase/ryu/extra.c", "// " + "x" * 79 + "\n")
        self.write("cbase/ryu/extra.h", "\tint x;\n")
        result = self.run_shell(TEST_MAXDEPTH="1")
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("extra.c:1:line exceeds 80 columns", result.stderr)
        self.assertIn("extra.h:1:tab character is forbidden", result.stderr)

    def test_shell_ryu_filter_checks_vendored_c_files(self):
        self.prepare_shell_checker()
        self.write("cbase/ryu/extra.c", "// " + "x" * 79 + "\n")
        result = self.run_shell(TEST_FILTER="ryu", TEST_MAXDEPTH="1")
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("extra.c:1:line exceeds 80 columns", result.stderr)

    def test_shell_ignores_generated_meta_headers(self):
        self.prepare_shell_checker()
        self.write("cbase/functions_never_use.txt", "malloc\n")
        self.write("src/generated.meta.h", "void *value = malloc(1);\n")
        result = self.run_shell()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_shell_disables_column_limit_for_piscou_config(self):
        self.prepare_shell_checker()
        self.write("piscou.1", "")
        self.write("config.h", "x" * 81 + "\n")
        self.write("src/extra.h", "x" * 81 + "\n")
        result = self.run_shell()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertNotIn("config.h:1:line exceeds", result.stderr)
        self.assertIn("extra.h:1:line exceeds 80 columns", result.stderr)

    def test_explicit_exclusion_still_applies(self):
        self.prepare_shell_checker()
        self.write("cbase/ryu/extra.c", "// " + "x" * 79 + "\n")
        result = self.run_shell(
            TEST_MAXDEPTH="1", TEST_EXCLUDE_PATTERN="(^|/)ryu/",
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_checker_cache_invalidates_changed_sources(self):
        path = "cbase/ryu/sample.c"
        self.assertEqual(self.run_perl(path, "int x;\n"), "")
        output = self.run_perl(path, "\tint x;\n")
        self.assertIn("tab character is forbidden", output)
        self.assertEqual(self.run_perl(path, "int x;\n"), "")

    def test_consecutive_bounds_use_assert_between(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "check(void) {\n"
            "    ASSERT_GE(index, 0);\n"
            "    ASSERT_LT(index, size);\n"
            "    ASSERT_GE(ctx->items[pos], 0);\n"
            "    ASSERT_LT(ctx->items[pos], LENGTH(items));\n"
            "}\n",
        )
        self.assertEqual(output.count("instead of consecutive"), 2)
        self.assertIn(
            "sample.c:3:use ASSERT_BETWEEN(index, 0, size - 1)",
            output,
        )
        self.assertIn(
            "sample.c:5:use ASSERT_BETWEEN(ctx->items[pos], 0, "
            "LENGTH(items) - 1)", output,
        )

    def test_consecutive_bounds_in_macro_continuations(self):
        output = self.run_perl(
            "sample.c",
            "#define CHECK_INDEX(i) do { \\\n"
            "    ASSERT_GE(i, 0);         \\\n"
            "    ASSERT_LT(i, LENGTH(data)); \\\n"
            "} while (0)\n",
        )
        self.assertIn(
            "sample.c:2:use ASSERT_BETWEEN(i, 0, LENGTH(data) - 1)",
            output,
        )

    def test_consecutive_bounds_ignores_unrelated_assertions(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "check(void) {\n"
            "    ASSERT_GE(i, 1);\n"
            "    ASSERT_LT(i, size);\n"
            "    ASSERT_GE(i, 0);\n"
            "    ASSERT_LT(j, size);\n"
            "    ASSERT_GE(i, 0);\n"
            "\n"
            "    ASSERT_LT(i, size);\n"
            "    ASSERT_GE(i, 0); other_call();\n"
            "    ASSERT_LT(i, size);\n"
            "    ASSERT_GE(i, 0); ASSERT_LT(i, size);\n"
            "    ASSERT_BETWEEN(i, 0, size - 1);\n"
            "}\n",
        )
        self.assertNotIn("instead of consecutive", output)

    def test_consecutive_bounds_ignore_comments_and_strings(self):
        output = self.run_perl(
            "sample.c",
            "// ASSERT_GE(i, 0);\n"
            "// ASSERT_LT(i, size);\n"
            "/* ASSERT_GE(i, 0);\n"
            "   ASSERT_LT(i, size); */\n"
            'const char *text = "ASSERT_GE(i, 0);\\nASSERT_LT(i, size);";\n'
            "ASSERT_GE(i, 0); // first comparison\n"
            "ASSERT_LT(i, size);\n",
        )
        self.assertEqual(output.count("instead of consecutive"), 1)
        self.assertIn("sample.c:6:use ASSERT_BETWEEN", output)

    def test_consecutive_bounds_uses_file_list_and_cache(self):
        path = "sample.c"
        pair = "ASSERT_GE(i, 0);\nASSERT_LT(i, limit);\n"
        self.assertIn(
            "ASSERT_BETWEEN(i, 0, limit - 1)",
            self.run_perl(path, pair, file_list=True),
        )
        self.assertIn(
            "ASSERT_BETWEEN(i, 0, limit - 1)",
            self.run_perl(path, pair, file_list=True),
        )
        self.assertNotIn(
            "instead of consecutive",
            self.run_perl(path, "ASSERT_GE(i, 0);\n", file_list=True),
        )

    def test_deep_artificial_blocks_are_reported(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "process(void) {\n"
            "    if (ready) {\n"
            "        for (int i = 0; i < 2; i += 1) {\n"
            "            {\n"
            "                int x = i;\n"
            "                use(x);\n"
            "            }\n"
            "            { use(i); }\n"
            "        }\n"
            "    }\n"
            "}\n",
        )
        self.assertEqual(output.count("artificial block at"), 2)
        self.assertIn("sample.c:5:artificial block at", output)
        self.assertIn("sample.c:9:artificial block at", output)

    def test_deep_inline_artificial_blocks_are_reported(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "process(void) {\n"
            "    if (ready) {\n"
            "        for (int i = 0; i < 2; i += 1) {\n"
            "            if (valid) { { use(i); } }\n"
            "            use(i); { use(i); }\n"
            "        }\n"
            "    }\n"
            "}\n",
        )
        self.assertEqual(output.count("artificial block at"), 2)
        self.assertIn("sample.c:5:artificial block at", output)
        self.assertIn("sample.c:6:artificial block at", output)

    def test_shallow_artificial_blocks_are_allowed(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "process(void) {\n"
            "    {\n"
            "        int x = 0;\n"
            "        { x += 1; }\n"
            "    }\n"
            "}\n",
        )
        self.assertNotIn("artificial block at", output)

    def test_deep_braces_with_control_headers_are_allowed(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "process(void) {\n"
            "    if (ready) {\n"
            "        while (running) {\n"
            "            if (valid)\n"
            "            {\n"
            "                use(1);\n"
            "            }\n"
            "            else\n"
            "            {\n"
            "                use(2);\n"
            "            }\n"
            "            do\n"
            "            {\n"
            "                use(3);\n"
            "            } while (again);\n"
            "            switch (value) {\n"
            "            case 1:\n"
            "            {\n"
            "                use(4);\n"
            "                break;\n"
            "            }\n"
            "            default:\n"
            "            {\n"
            "                use(5);\n"
            "                break;\n"
            "            }\n"
            "            }\n"
            "        }\n"
            "    }\n"
            "}\n",
        )
        self.assertNotIn("artificial block at", output)

    def test_deep_aggregate_initializers_are_allowed(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "process(void) {\n"
            "    if (ready) {\n"
            "        int items[][2] = {\n"
            "            { 1, 2 },\n"
            "            { 3, 4 },\n"
            "        };\n"
            "        int flags[] =\n"
            "            { 0, 1 };\n"
            "        struct Point point =\n"
            "            {\n"
            "                .x = 1,\n"
            "                .y = 2,\n"
            "            };\n"
            "    }\n"
            "}\n",
        )
        self.assertNotIn("artificial block at", output)

    def test_deep_preprocessor_control_braces_are_allowed(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "process(void) {\n"
            "    if (ready) {\n"
            "        while (running) {\n"
            "#if defined(USE_FAST)\n"
            "            if (fast)\n"
            "#else\n"
            "            if (slow)\n"
            "#endif\n"
            "            {\n"
            "                use(1);\n"
            "            }\n"
            "        }\n"
            "    }\n"
            "}\n",
        )
        self.assertNotIn("artificial block at", output)

    def test_deep_artificial_blocks_ignore_comments_and_strings(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "process(void) {\n"
            "    if (ready) {\n"
            "        while (running) {\n"
            "            // {\n"
            "            /* { */\n"
            '            const char *text = "{";\n'
            "            { // actual block\n"
            "                use(text);\n"
            "            }\n"
            "        }\n"
            "    }\n"
            "}\n",
        )
        self.assertEqual(output.count("artificial block at"), 1)
        self.assertIn("sample.c:8:artificial block at", output)

    def test_deep_artificial_blocks_with_directive_file_list(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "process(void) {\n"
            "    if (ready) {\n"
            "        while (running) {\n"
            "#define BLOCK_MACRO() do { \\\n"
            "            { \\\n"
            "            } \\\n"
            "        } while (0)\n"
            "            {\n"
            "                use(1);\n"
            "            }\n"
            "        }\n"
            "    }\n"
            "}\n",
            file_list=True,
        )
        self.assertEqual(output.count("artificial block at"), 1)
        self.assertIn("sample.c:9:artificial block at", output)

    def test_multiline_string_literals_must_align(self):
        output = self.run_perl(
            "sample.c",
            "static void\n"
            "function(void) {\n"
            '    fmt_printf("A long aligned format string with value %s "\n'
            '               "and enough text to require wrapping; "\n'
            '               "another value is %d\\n",\n'
            "               value, count);\n"
            '    fmt_printf("A long misaligned format string with value %s "\n'
            '              "and enough text to require wrapping; "\n'
            '              "another value is %d\\n",\n'
            "               value, count);\n"
            "}\n",
        )
        self.assertEqual(
            output.count("call string literal is not aligned"), 1,
        )
        self.assertIn("sample.c:8:fmt_printf call string literal", output)


if __name__ == "__main__":
    unittest.main()
