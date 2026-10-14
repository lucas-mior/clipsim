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
