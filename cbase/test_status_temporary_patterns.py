#!/usr/bin/env python3
"""Regression tests for status_temporary_patterns.py."""

from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("status_temporary_patterns.py")


class StatusTemporaryPatternsTests(unittest.TestCase):
    def run_checker(self, source, *, file_list=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "sample.c"
            path.write_text(source)
            command = ["python3", str(SCRIPT)]
            if file_list:
                paths = root / "sources.txt"
                paths.write_text(f"{path}\n")
                command.extend(["--file-list", str(paths)])
            else:
                command.append(str(path))
            result = subprocess.run(
                command,
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            return result.stdout

    def test_reports_immediate_one_use_status(self):
        output = self.run_checker(
            """\
static void
example(void) {
    int32 status;

    status = operation();
    ASSERT_LE(status, 0);
    return;
}
"""
        )
        self.assertIn("sample.c:3:inline the one-use int32 status", output)
        self.assertIn("in example", output)

    def test_accepts_inlined_form(self):
        output = self.run_checker(
            """\
static void
example(void) {
    ASSERT_LE(operation(), 0);
    return;
}
"""
        )
        self.assertEqual(output, "")

    def test_requires_exactly_three_variable_references(self):
        output = self.run_checker(
            """\
static void
example(void) {
    int32 status;

    status = operation();
    log_status(status);
    ASSERT_LE(status, 0);
    return;
}
"""
        )
        self.assertEqual(output, "")

    def test_requires_immediately_following_source_line(self):
        output = self.run_checker(
            """\
static void
example(void) {
    int32 status;

    status = operation();

    ASSERT_LE(status, 0);
    return;
}
"""
        )
        self.assertEqual(output, "")

    def test_out_parameter_is_not_assignment_pattern(self):
        output = self.run_checker(
            """\
static void
example(void) {
    int32 status;

    operation(&status);
    ASSERT_LE(status, 0);
    return;
}
"""
        )
        self.assertEqual(output, "")

    def test_member_name_does_not_count_as_status_variable(self):
        output = self.run_checker(
            """\
static void
example(void) {
    int32 status;

    status = operation();
    ASSERT_TRUE(result.status);
    return_if_needed(status);
    return;
}
"""
        )
        self.assertEqual(output, "")

    def test_comments_and_literals_do_not_count(self):
        output = self.run_checker(
            """\
static void
example(void) {
    int32 status; /* status */
    char *text = "status";

    status = operation();
    ASSERT_LE(status, 0);
    (void)text;
    return;
}
"""
        )
        self.assertIn("inline the one-use int32 status", output)

    def test_initialized_status_declaration_is_not_targeted(self):
        output = self.run_checker(
            """\
static void
example(void) {
    int32 status = operation();
    ASSERT_LE(status, 0);
    return;
}
"""
        )
        self.assertEqual(output, "")

    def test_shadowed_status_is_conservative(self):
        output = self.run_checker(
            """\
static void
example(void) {
    int32 status;

    status = operation();
    {
        int32 status;
    }
    return;
}
"""
        )
        self.assertEqual(output, "")

    def test_file_list_input(self):
        output = self.run_checker(
            """\
static int32
example(void) {
    int32 status;

    status = operation();
    return status;
}
""",
            file_list=True,
        )
        self.assertIn("inline the one-use int32 status", output)


if __name__ == "__main__":
    unittest.main()
