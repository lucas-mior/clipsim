#!/usr/bin/env python3
"""Regression tests for single_use_patterns.py (stdlib only)."""

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


CHECKER = Path(__file__).with_name("single_use_patterns.py")


class SingleUseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def write(self, name, contents):
        path = self.directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")
        return path

    def run_check(self, *args, env=None):
        result = subprocess.run(
            [sys.executable, str(CHECKER), *map(str, args)],
            cwd=self.directory, capture_output=True, text=True,
            check=False, env=env)
        self.assertFalse(result.stderr, result.stderr)
        self.assertIn(result.returncode, (0, 1))
        return result.stdout

    def cache_data(self):
        path = self.directory / ".cache/single_use_patterns.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def test_both_patterns_and_prototype(self):
        self.write("a.c", """
            typedef struct Data { int x; } Data;
            static int twice(int x);
            static int twice(int x) { return x * 2; }
            int run(void) {
                Data data = { 1 };
                return twice(data.x);
            }
        """)
        output = self.run_check(".")
        self.assertIn("struct Data: used only within one function", output)
        self.assertIn("function twice: called once", output)
        self.assertEqual(output.count("\n"), 2)

    def test_multiple_calls_and_functions(self):
        self.write("a.c", """
            typedef struct State { int v; } State;
            static int plus(int x) { return x + 1; }
            int first(void) {
                State s = { 2 };
                return plus(s.v) + plus(s.v);
            }
            int second(void) {
                State s = { 3 };
                return s.v;
            }
        """)
        self.assertEqual(self.run_check("."), "")

    def test_external_uses_and_header_decl(self):
        self.write("a.c", """
            typedef struct Shared { int v; } Shared;
            int public_function(void) { return 0; }
            void use(void) { Shared s; public_function(); }
        """)
        self.write("b.h", """
            typedef struct Shared Shared;
            int public_function(void);
        """)
        self.assertEqual(self.run_check("a.c", "--include-global"), "")

    def test_static_name_in_unrelated_file(self):
        self.write("a.c", """
            static int helper(void) { return 1; }
            int a(void) { return helper(); }
        """)
        self.write("b.c", """
            static int helper(void) { return 2; }
            int b(void) { return helper(); }
        """)
        output = self.run_check(".")
        self.assertEqual(output.count("function helper:"), 2)

    def test_header_inline_code_is_a_possible_caller(self):
        self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.write("b.h", """
            static inline int other(void) { return helper(); }
        """)
        self.assertEqual(self.run_check("a.c"), "")

    def test_unrelated_identifier_does_not_prove_struct_use(self):
        self.write("a.c", """
            typedef struct Data { int x; } Data;
            int run(void) { int Data = 1; return Data; }
        """)
        self.assertEqual(self.run_check("."), "")

    def test_tagged_struct_without_typedef(self):
        self.write("a.c", """
            struct Data { int x; };
            int run(void) { struct Data data; return data.x; }
        """)
        self.assertIn("struct Data:", self.run_check("."))

    def test_nonstatic_opt_in(self):
        self.write("a.c", """
            int helper(void) { return 1; }
            int caller(void) { return helper(); }
        """)
        self.assertEqual(self.run_check("."), "")
        output = self.run_check("--include-global", ".")
        self.assertIn("function helper:", output)

    def test_local_function_declaration_is_not_a_call(self):
        self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) {
                int helper(void);
                return 0;
            }
        """)
        self.assertEqual(self.run_check("."), "")

    def test_function_pointer_and_macro(self):
        self.write("a.c", """
            static int pointed(void) { return 1; }
            static int macros(void) { return 2; }
            int run(void) {
                int (*ptr)(void) = pointed;
                return ptr() + macros();
            }
        """)
        self.write("defs.h", "#define CALL_MACROS() macros()\n")
        self.assertEqual(self.run_check("."), "")

    def test_conditionals_not_safe(self):
        self.write("a.c", """
            typedef struct Maybe { int x; } Maybe;
            static int helper(void) { return 1; }
            int run(void) {
            #if FEATURE
                Maybe s = { 1 };
                return helper() + s.x;
            #endif
                return 0;
            }
        """)
        self.assertEqual(self.run_check("a.c"), "")

    def test_whole_file_if_not_defined_guard_is_not_conditional(self):
        self.write("a.c", """
            #define BEFORE_GUARD 1
            #include "a.h"
            #if !defined(A_C)
            #define A_C
            typedef struct Data { int x; } Data;
            static int helper(void) { return 1; }
            int run(void) {
                Data data = { 1 };
                return helper() + data.x;
            }
            #endif /* A_C */
        """)
        self.write("a.h", "/* header */\n")
        output = self.run_check("a.c")
        self.assertIn("struct Data: used only within one function", output)
        self.assertIn("function helper: called once", output)

    def test_whole_file_ifndef_guard_is_not_conditional(self):
        self.write("a.c", """
            #ifndef A_C
            #define A_C
            typedef struct Data { int x; } Data;
            int run(void) { Data data = { 1 }; return data.x; }
            #endif
        """)
        self.assertIn("struct Data: used only within one function",
                      self.run_check("a.c"))

    def test_nested_conditional_inside_whole_file_guard_is_still_unsafe(self):
        self.write("a.c", """
            #if !defined(A_C)
            #define A_C
            typedef struct Maybe { int x; } Maybe;
            static int helper(void) { return 1; }
            int run(void) {
            #if FEATURE
                Maybe data = { 1 };
                return helper() + data.x;
            #endif
                return 0;
            }
            #endif
        """)
        self.assertEqual(self.run_check("a.c"), "")

    def test_guard_like_block_with_trailing_code_remains_conditional(self):
        self.write("a.c", """
            #if !defined(A_C)
            #define A_C
            typedef struct Data { int x; } Data;
            int run(void) { Data data = { 1 }; return data.x; }
            #endif
            int trailing(void) { return 0; }
        """)
        self.assertEqual(self.run_check("a.c"), "")

    def test_strings_comments_and_identifiers(self):
        self.write("a.c", r'''
            typedef struct Box { int value; } Box;
            static int work(void) { return 4; }
            int main(void) {
                Box box;
                char *unused = "work() Box { }";
                // work(); Box
                /* work(); Box */
                return work() + box.value;
            }
        ''')
        output = self.run_check(".")
        self.assertIn("function work:", output)
        self.assertIn("struct Box:", output)

    def test_tag_and_alias_both_count(self):
        self.write("a.c", """
            typedef struct Node { struct Node *next; } Node;
            int run(void) {
                struct Node n;
                Node other;
                return n.next == &other;
            }
        """)
        output = self.run_check(".")
        self.assertIn("struct Node:", output)
        self.assertEqual(output.count("struct Node:"), 1)

    def test_shadowed_struct_tag_is_not_a_use(self):
        self.write("a.c", """
            struct Data { int x; };
            int run(void) {
                struct Data { int y; };
                struct Data local;
                return local.y;
            }
        """)
        self.assertEqual(self.run_check("."), "")

    def test_multiple_pointer_qualifiers(self):
        self.write("a.c", """
            typedef struct Data { int x; } Data;
            int run(void) {
                Data **volatile pointer = 0;
                return pointer == 0;
            }
        """)
        self.assertIn("struct Data:", self.run_check("."))

    def test_global_use_disqualifies_type(self):
        self.write("a.c", """
            struct Box { int x; };
            struct Box shared;
            int run(void) { struct Box local; return local.x; }
        """)
        self.assertEqual(self.run_check("."), "")

    def test_local_struct_definition_not_reported(self):
        self.write("a.c", """
            int run(void) {
                typedef struct Local { int x; } Local;
                Local x = { 0 };
                return x.x;
            }
        """)
        self.assertEqual(self.run_check("."), "")

    def test_included_c_is_not_standalone(self):
        self.write("inc.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.write("main.c", '#include "inc.c"\n')
        self.assertEqual(self.run_check("."), "")

    def test_multiline_directive(self):
        self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.write("b.h", "#define USE() \\\n helper()\n")
        self.assertEqual(self.run_check("a.c"), "")

    def test_file_list_and_exclude(self):
        a = self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.write("b.c", """
            static int other(void) { return 2; }
            int use(void) { return other(); }
        """)
        paths = self.write("sources.txt", f"{a}\n")
        output = self.run_check("--file-list", paths)
        self.assertIn("function helper:", output)
        self.assertNotIn("function other:", output)
        self.assertEqual(self.run_check("--exclude", "a.c", a), "")

    def test_no_paths_means_no_candidates(self):
        self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.assertEqual(self.run_check(), "")
        self.assertFalse((self.directory / ".cache").exists())

    def test_gitignore_is_not_applied(self):
        self.write(".gitignore", "generated/\n*.tmp.c\n")
        ignored = self.write("generated/ignored.c", """
            static int discarded(void) { return 1; }
            int run(void) { return discarded(); }
        """)
        temporary = self.write("other.tmp.c", """
            static int skipped(void) { return 1; }
            int run(void) { return skipped(); }
        """)
        self.assertIn("function discarded:", self.run_check(ignored))
        self.assertIn("function skipped:", self.run_check(temporary))
        output = self.run_check(".")
        self.assertIn("function discarded:", output)
        self.assertIn("function skipped:", output)

    def test_cbase_is_not_excluded_by_default(self):
        source = self.write("cbase/detection.c", """
            static int candidate(void) { return 1; }
            int run(void) { return candidate(); }
        """)
        self.assertIn("function candidate:", self.run_check(source))
        self.assertIn("function candidate:", self.run_check("cbase"))

    def test_gitignored_header_is_not_a_reference(self):
        source = self.write("src/a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.write(".gitignore", "generated/\n")
        self.write("generated/use.h", "#define USE_HELPER() helper()\n")
        self.assertIn("function helper:", self.run_check(source))

    def test_gitignored_c_file_is_not_a_reference(self):
        source = self.write("src/a.c", """
            int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.write(".gitignore", "generated/\n")
        ignored = self.write("generated/use.c", """
            int helper(void);
            int other(void) { return helper(); }
        """)
        output = self.run_check("--include-global", source)
        self.assertIn("function helper:", output)
        # Explicit candidate selection remains independent of .gitignore.
        self.assertEqual(
            self.run_check("--include-global", source, ignored), output)

    def test_gitignore_negation_keeps_reference(self):
        source = self.write("src/a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.write(".gitignore", "generated/*.h\n!generated/keep.h\n")
        self.write("generated/drop.h", "#define DROP() helper()\n")
        keep = self.write("generated/keep.h", "#define KEEP() helper()\n")
        self.assertEqual(self.run_check(source), "")
        keep.unlink()
        self.assertIn("function helper:", self.run_check(source))

    def test_gitignore_change_invalidates_reference_cache(self):
        source = self.write("src/a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        ignore = self.write(".gitignore", "generated/\n")
        self.write("generated/use.h", "#define USE_HELPER() helper()\n")
        expected = self.run_check(source)
        self.assertIn("function helper:", expected)
        self.assertEqual(self.run_check(source), expected)

        ignore.write_text("# no ignores\n", encoding="utf-8")
        self.assertEqual(self.run_check(source), "")
        ignore.write_text("generated/\n", encoding="utf-8")
        self.assertEqual(self.run_check(source), expected)

    def test_unsupplied_header_is_still_a_reference(self):
        source = self.write("src/a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.write("include/extra.h", "#define EXTRA() helper()\n")
        self.assertEqual(self.run_check(source), "")
        self.assertEqual(self.run_check("--exclude-dir", "include", source),
                         "")

    def test_unsupplied_c_file_is_still_a_reference(self):
        source = self.write("src/a.c", """
            int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.write("other/use.c", """
            int helper(void);
            int other(void) { return helper(); }
        """)
        self.assertEqual(self.run_check("--include-global", source), "")

    def test_unsupplied_file_can_disqualify_struct(self):
        source = self.write("src/a.c", """
            typedef struct Data { int x; } Data;
            int run(void) { Data data = { 1 }; return data.x; }
        """)
        self.write("other/use.c", """
            typedef struct Data Data;
            Data *shared;
        """)
        self.assertEqual(self.run_check(source), "")

    def test_exclude_dir_name_relative_and_absolute_paths(self):
        self.write("main.c", """
            static int included(void) { return 1; }
            int run(void) { return included(); }
        """)
        self.write("build/skip.c", """
            static int in_build(void) { return 1; }
            int run(void) { return in_build(); }
        """)
        self.write("src/vendor/skip.c", """
            static int in_vendor(void) { return 1; }
            int run(void) { return in_vendor(); }
        """)
        self.write("src/extras/skip.c", """
            static int in_extras(void) { return 1; }
            int run(void) { return in_extras(); }
        """)
        all_output = self.run_check(".")
        self.assertIn("function in_build:", all_output)
        self.assertIn("function in_vendor:", all_output)
        self.assertIn("function in_extras:", all_output)
        output = self.run_check("--exclude-dir", "build", "--exclude-dir",
                                "src/vendor", "--ignore-dir",
                                self.directory / "src/extras", ".")
        self.assertIn("function included:", output)
        self.assertNotIn("in_build", output)
        self.assertNotIn("in_vendor", output)
        self.assertNotIn("in_extras", output)
        self.assertEqual(output, self.run_check("--no-cache", "--exclude-dir",
                                                "build", "--exclude-dir",
                                                "src/vendor", "--ignore-dir",
                                                self.directory / "src/extras",
                                                "."))

    def test_exclude_dir_file_list_and_cache_file_set(self):
        main = self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        other = self.write("generated/b.c", """
            static int extra(void) { return 1; }
            int run(void) { return extra(); }
        """)
        list_path = self.write("sources.txt", f"{main}\n{other}\n")
        first = self.run_check("--file-list", list_path)
        self.assertIn("function extra:", first)
        self.assertEqual(len(self.cache_data()["files"]), 2)
        filtered = self.run_check("--file-list", list_path,
                                  "--exclude-dir", "generated")
        self.assertIn("function helper:", filtered)
        self.assertNotIn("extra", filtered)
        self.assertEqual(self.run_check("--file-list", list_path), first)
        # Exclusions must be reflected in the final-result cache's run key.
        self.assertEqual(len(self.cache_data()["runs"]), 2)

    def test_new_reference_file_invalidates_cached_result(self):
        source = self.write("a.c", """
            int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        expected = self.run_check("--include-global", source)
        self.assertIn("function helper:", expected)

        other = self.write("other/use.c", """
            int helper(void);
            int use(void) { return helper(); }
        """)
        self.assertEqual(self.run_check("--include-global", source), "")
        other.unlink()
        self.assertEqual(self.run_check("--include-global", source), expected)

    def test_cache_reuses_unchanged_results_without_writing(self):
        self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        output = self.run_check(".")
        path = self.directory / ".cache/single_use_patterns.json"
        fast_path = path.with_name(path.name + ".fast")
        self.assertTrue(fast_path.exists())
        first = self.cache_data()
        self.assertEqual(first["version"], 3)
        self.assertEqual(len(first["files"]), 1)
        self.assertEqual(len(first["runs"]), 1)
        before = path.stat().st_mtime_ns
        fast_before = fast_path.stat().st_mtime_ns

        self.assertEqual(self.run_check("."), output)
        self.assertEqual(path.stat().st_mtime_ns, before)
        self.assertEqual(fast_path.stat().st_mtime_ns, fast_before)
        self.assertEqual(self.run_check("--no-cache", "."), output)
        self.assertEqual(path.stat().st_mtime_ns, before)

    def test_cache_invalidation_for_edited_source_and_header(self):
        source = self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        self.assertIn("function helper:", self.run_check(source))

        # A new header or an edited header can suppress a finding even though
        # the defining C file did not change.
        header = self.write("b.h", "#define EXTRA() helper()\n")
        self.assertEqual(self.run_check(source), "")
        self.assertEqual(self.run_check(source), "")
        header.write_text("/* no reference */\n", encoding="utf-8")
        self.assertIn("function helper:", self.run_check(source))

        # Also detect an edit with unchanged size and modification time.
        before = source.stat()
        code = source.read_text(encoding="utf-8")
        source.write_text(code.replace("helper();", "other_();"),
                          encoding="utf-8")
        os.utime(source, ns=(before.st_atime_ns, before.st_mtime_ns))
        self.assertEqual(source.stat().st_size, before.st_size)
        self.assertEqual(source.stat().st_mtime_ns, before.st_mtime_ns)
        self.assertEqual(self.run_check(source), "")

    def test_cache_respects_options_and_candidate_set(self):
        source = self.write("a.c", """
            int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        header = self.write("other.h", "#define EXTRA() helper()\n")
        self.assertEqual(self.run_check("--include-global", source), "")
        self.assertEqual(self.run_check("--include-global", "--exclude",
                                       "*.h", source), "")

        header.unlink()
        output = self.run_check("--include-global", source)
        self.assertIn("function helper:", output)
        self.assertEqual(self.run_check("--include-global", "."), output)

    def test_cache_corruption_and_fingerprint_change(self):
        self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        cache_path = self.directory / ".cache/single_use_patterns.json"
        expected = self.run_check(".")

        cache_path.write_text("{invalid", encoding="utf-8")
        self.assertEqual(self.run_check("."), expected)

        cache = self.cache_data()
        cache["checker"] = "not-the-checker-hash"
        cache_path.write_text(json.dumps(cache), encoding="utf-8")
        self.assertEqual(self.run_check("."), expected)

        cache = self.cache_data()
        cache["files"][str(self.directory / "a.c")]["parsed"] = {}
        next(iter(cache["runs"].values()))["findings"] = ["bad row"]
        cache_path.write_text(json.dumps(cache), encoding="utf-8")
        self.assertEqual(self.run_check("."), expected)

    def test_fast_cache_corruption_falls_back_to_main_cache(self):
        self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        expected = self.run_check(".")
        cache_path = self.directory / ".cache/single_use_patterns.json"
        fast_path = cache_path.with_name(cache_path.name + ".fast")
        cache_before = cache_path.stat().st_mtime_ns

        fast_path.write_text("{invalid", encoding="utf-8")
        self.assertEqual(self.run_check("."), expected)
        self.assertEqual(cache_path.stat().st_mtime_ns, cache_before)
        self.assertEqual(self.run_check("."), expected)

    def test_cache_path_override_and_unwritable_destination(self):
        self.write("a.c", """
            static int helper(void) { return 1; }
            int run(void) { return helper(); }
        """)
        env = dict(os.environ)
        env["CBASE_SINGLE_USE_PATTERNS_CACHE"] = "elsewhere/checker.json"
        expected = self.run_check(".", env=env)
        self.assertIn("function helper:", expected)
        self.assertTrue((self.directory / "elsewhere/checker.json").exists())
        self.assertFalse((self.directory / ".cache").exists())
        self.assertEqual(self.run_check(".", env=env), expected)

        env["CBASE_SINGLE_USE_PATTERNS_CACHE"] = "a.c/unavailable.json"
        self.assertEqual(self.run_check(".", env=env), expected)


if __name__ == "__main__":
    unittest.main()
