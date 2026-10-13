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

    def test_non_ryu_ternary_is_rejected_but_macro_is_not(self):
        output = self.run_perl(
            "cbase/sample.c",
            "#define CHOOSE(a, b) ((a) ? (a) : (b))\n"
            "static int\n"
            "choose(int x) {\n"
            "    return x ? 1 : 0;\n"
            "}\n"
            "static char\n"
            "question(void) {\n"
            "    return '?'; // ? in a comment\n"
            "}\n",
        )
        self.assertEqual(output.count("ternary operator is forbidden"), 1)
        self.assertIn("sample.c:4:ternary operator", output)

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


if __name__ == "__main__":
    unittest.main()
