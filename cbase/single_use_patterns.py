#!/usr/bin/env python3
"""Report potentially redundant C functions and file-scope struct types.

This is a conservative lexical check, not a preprocessor or a C compiler.
It deliberately omits uncertain cases instead of proposing unsafe refactors.
See --help for usage and limitations.
"""

import argparse
from collections import defaultdict
from dataclasses import dataclass
import hashlib
import json
import os
from fnmatch import fnmatch
from pathlib import Path
import re
import subprocess
import sys
import tempfile


IDENTIFIER = re.compile(r"[A-Za-z_]\w*\Z", re.ASCII)
LEXEMES = re.compile(r"[A-Za-z_]\w*|[^\s]", re.ASCII)
COMMENTS_AND_LITERALS = re.compile(
    r"/\*.*?\*/|//[^\n]*|(?:u8|[uUL])?\"(?:\\.|[^\"\\])*\""
    r"|(?:u8|[uUL])?'(?:\\.|[^'\\])*'",
    re.DOTALL,
)
DIRECTIVE = re.compile(r"^[ \t]*#\s*(\w+)(.*)$")
INCLUDE_C = re.compile(r'^\s*[<"]([^>"]+\.c)[>"]')
GUARD_IF = re.compile(
    r"^\s*!\s*defined\s*(?:\(\s*([A-Za-z_]\w*)\s*\)"
    r"|([A-Za-z_]\w*))\s*$", re.ASCII)
GUARD_IFNDEF = re.compile(r"^\s*([A-Za-z_]\w*)\s*$", re.ASCII)
GUARD_DEFINE = re.compile(r"^\s*([A-Za-z_]\w*)\s*$", re.ASCII)
CONTROL = {"if", "while", "for", "switch", "sizeof", "_Alignof",
           "_Generic", "return", "typedef", "__attribute__"}
CACHE_VERSION = 3
CACHE_ENV = "CBASE_SINGLE_USE_PATTERNS_CACHE"
DEFAULT_CACHE = ".cache/single_use_patterns.json"


def mask_non_newlines(match):
    return re.sub(r"[^\n]", " ", match.group())


def whole_file_guard(original, code=None):
    """Return opening/closing lines for a conventional whole-file guard."""
    if code is None:
        code = COMMENTS_AND_LITERALS.sub(mask_non_newlines, original)
    lines = code.splitlines()
    stack = []
    pairs = {}
    branched = set()
    top_level = []

    for number, line in enumerate(lines, 1):
        match = DIRECTIVE.match(line)
        if not match:
            continue
        kind = match.group(1)
        if kind in ("if", "ifdef", "ifndef"):
            if not stack:
                top_level.append((number, kind, match.group(2)))
            stack.append(number)
        elif kind in ("else", "elif"):
            if stack:
                branched.add(stack[-1])
        elif kind == "endif" and stack:
            opening = stack.pop()
            pairs[opening] = number

    for opening, kind, argument in top_level:
        if opening in branched or opening not in pairs:
            continue
        macro = None
        if kind == "ifndef":
            match = GUARD_IFNDEF.fullmatch(argument)
            if match:
                macro = match.group(1)
        elif kind == "if":
            match = GUARD_IF.fullmatch(argument)
            if match:
                macro = match.group(1) or match.group(2)
        if macro is None:
            continue

        closing = pairs[opening]
        first_body_line = None
        for number in range(opening + 1, closing):
            if lines[number - 1].strip():
                first_body_line = number
                break
        if first_body_line is None:
            continue
        define = DIRECTIVE.match(lines[first_body_line - 1])
        if define is None or define.group(1) != "define":
            continue
        defined = GUARD_DEFINE.fullmatch(define.group(2))
        if defined is None or defined.group(1) != macro:
            continue

        before = lines[:opening - 1]
        if any(line.strip() and DIRECTIVE.match(line) is None
               for line in before):
            continue
        if any(line.strip() for line in lines[closing:]):
            continue
        return opening, closing
    return None


@dataclass(frozen=True, slots=True)
class Token:
    value: str
    line: int


@dataclass
class Function:
    name: str
    line: int
    name_index: int
    start: int
    end: int
    is_static: bool
    conditional: bool


@dataclass
class Struct:
    name: str
    line: int
    names: tuple
    tag: str
    alias: str
    start: int
    end: int
    conditional: bool


class Source:
    def __init__(self, path):
        self.path = path
        original = path.read_text(encoding="utf-8", errors="replace")
        lines = original.splitlines(keepends=True)
        directives = set()
        conditional = set()
        self.included_c = set()
        self.prototype_indices = set()
        condition_depth = 0
        continued = False
        code = COMMENTS_AND_LITERALS.sub(mask_non_newlines, original)
        guard = whole_file_guard(original, code)
        guard_open = guard[0] if guard is not None else -1
        guard_close = guard[1] if guard is not None else -1

        for number, line in enumerate(lines, 1):
            is_continuation = continued
            match = DIRECTIVE.match(line) if not continued else None
            if match:
                kind = match.group(1)
                if kind in ("if", "ifdef", "ifndef"):
                    condition_depth += 1
                elif kind == "endif":
                    condition_depth = max(0, condition_depth - 1)
                if kind == "include":
                    included = INCLUDE_C.match(match.group(2))
                    if included:
                        self.included_c.add(Path(included.group(1)).name)
                continued = line.rstrip("\r\n").endswith("\\")
            elif continued:
                continued = line.rstrip("\r\n").endswith("\\")
            else:
                continued = False
            if match or is_continuation:
                directives.add(number)
            guard_depth = 1 if guard_open <= number < guard_close else 0
            if condition_depth > guard_depth:
                conditional.add(number)

        self.tokens = []
        self.directive_uses = set()
        for number, line in enumerate(code.splitlines(), 1):
            for item in LEXEMES.finditer(line):
                value = item.group()
                if number in directives:
                    if IDENTIFIER.fullmatch(value):
                        self.directive_uses.add(value)
                else:
                    self.tokens.append(Token(value, number))
        self.conditional = conditional
        self.functions = []
        self.structs = []
        self.owners = [-1] * len(self.tokens)
        self.pairs = {}
        self._parse()

    def to_cache(self):
        """Persist parser results, not final cross-file diagnostics."""
        return {
            "tokens": [[token.value, token.line] for token in self.tokens],
            "included_c": sorted(self.included_c),
            "prototype_indices": sorted(self.prototype_indices),
            "directive_uses": sorted(self.directive_uses),
            "conditional": sorted(self.conditional),
            "owners": self.owners,
            "pairs": [[index, match] for index, match in self.pairs.items()],
            "functions": [
                [f.name, f.line, f.name_index, f.start, f.end,
                 f.is_static, f.conditional] for f in self.functions
            ],
            "structs": [
                [s.name, s.line, s.names, s.tag, s.alias, s.start, s.end,
                 s.conditional] for s in self.structs
            ],
        }

    @classmethod
    def from_cache(cls, path, data):
        """Reconstitute a source without reopening or parsing its C file."""
        source = cls.__new__(cls)
        source.path = path
        source.tokens = [Token(*values) for values in data["tokens"]]
        source.included_c = set(data["included_c"])
        source.prototype_indices = set(data["prototype_indices"])
        source.directive_uses = set(data["directive_uses"])
        source.conditional = set(data["conditional"])
        source.owners = data["owners"]
        source.pairs = dict(data["pairs"])
        source.functions = [Function(*values) for values in data["functions"]]
        source.structs = [
            Struct(values[0], values[1], tuple(values[2]), *values[3:])
            for values in data["structs"]
        ]
        if len(source.tokens) != len(source.owners):
            raise ValueError("invalid cached owner list")
        return source

    def _parse(self):
        tokens = self.tokens
        pairs = {}
        stack = []
        for index, token in enumerate(tokens):
            if token.value in "({[" and len(token.value) == 1:
                stack.append((token.value, index))
            elif token.value in ")}]" and len(token.value) == 1:
                opening = {")": "(", "]": "[", "}": "{"}[token.value]
                if stack and stack[-1][0] == opening:
                    _, start = stack.pop()
                    pairs[start] = index
                    pairs[index] = start
                else:
                    # Unbalanced input is unsuitable for scope analysis.
                    return
        if stack:
            return
        self.pairs = pairs

        start = 0
        index = 0
        while index < len(tokens):
            value = tokens[index].value
            if value == ";":
                self._prototype(start, index, pairs)
                start = index + 1
            elif value == "{":
                end = pairs[index]
                head = tokens[start:index]
                function = self._function(head, start, index, end, pairs)
                if function is not None:
                    owner = len(self.functions)
                    self.functions.append(function)
                    for item in range(index + 1, end):
                        self.owners[item] = owner
                    index = end
                else:
                    struct = self._struct(head, start, index, end)
                    if struct is not None:
                        self.structs.append(struct)
                    index = end
                start = end + 1
            index += 1

    def _prototype(self, start, end, pairs):
        tokens = self.tokens
        if end <= start or tokens[end - 1].value != ")":
            return
        paren = pairs.get(end - 1)
        if paren is None or paren <= start:
            return
        name = tokens[paren - 1].value
        prefix = [token.value for token in tokens[start:paren - 1]]
        if (IDENTIFIER.fullmatch(name) and name not in CONTROL and prefix
                and "typedef" not in prefix and "=" not in prefix
                and "(" not in prefix and ")" not in prefix):
            self.prototype_indices.add(paren - 1)

    def _function(self, head, start, brace, end, pairs):
        tokens = self.tokens
        if not head or tokens[brace - 1].value != ")":
            return None
        paren = pairs.get(brace - 1)
        if paren is None or paren <= start:
            return None
        name_index = paren - 1
        name = tokens[name_index].value
        if not IDENTIFIER.fullmatch(name) or name in CONTROL:
            return None
        prefix = tokens[start:name_index]
        if not prefix or any(token.value in ("=", "typedef", ":")
                             for token in prefix):
            return None
        # A function definition must have a declarator, not an initializer.
        if any(token.value in ("{", "}", ";") for token in prefix):
            return None
        return Function(name, tokens[name_index].line, name_index,
                        brace + 1, end,
                        any(token.value == "static" for token in prefix),
                        tokens[name_index].line in self.conditional)

    def _struct(self, head, start, brace, end):
        tokens = self.tokens
        if not head:
            return None
        # Only simple file-scope struct definitions, with a single declarator.
        # Do not mistake a compound literal or initialized object for a type.
        if any(token.value == "=" for token in head):
            return None
        values = [token.value for token in head]
        if "struct" not in values:
            return None
        struct_pos = len(values) - 1 - values[::-1].index("struct")
        after_struct = values[struct_pos + 1:]
        if len(after_struct) > 1:
            return None
        if after_struct and not IDENTIFIER.fullmatch(after_struct[0]):
            return None
        tag = after_struct[0] if after_struct else None
        typedef = "typedef" in values[:struct_pos]
        if not typedef and not tag:
            return None

        stop = end + 1
        while stop < len(tokens) and tokens[stop].value != ";":
            # Avoid swallowing the next declaration if this isn't simple.
            if tokens[stop].value in ("{", "}"):
                return None
            stop += 1
        if stop == len(tokens):
            return None
        trailing = [token.value for token in tokens[end + 1:stop]]
        alias = None
        if typedef:
            if len(trailing) != 1 or not IDENTIFIER.fullmatch(trailing[0]):
                return None
            alias = trailing[0]
            names = tuple(dict.fromkeys(filter(None, (tag, alias))))
        else:
            if trailing:
                return None
            names = (tag,)
        return Struct(names[-1], tokens[struct_pos + start].line, names,
                      tag, alias, start, stop + 1,
                      tokens[struct_pos + start].line in self.conditional)



def excluded_dir(path, directory_names, directory_paths):
    """Exclude whole subtrees, whether the input is a file or directory."""
    if any(part in directory_names for part in path.parts):
        return True
    return any(path == excluded or excluded in path.parents
               for excluded in directory_paths)


def directory_exclusions(values):
    """A simple name matches anywhere; a path is relative to the cwd."""
    names = set()
    paths = set()
    for value in values:
        cleaned = value.rstrip("/\\")
        if not cleaned or cleaned in (".", ".."):
            raise ValueError(f"invalid --exclude-dir value: {value!r}")
        path = Path(cleaned)
        if path.is_absolute() or "/" in cleaned or len(path.parts) > 1:
            paths.add(path.resolve())
        else:
            names.add(path.name)
    return names, paths


def excluded_file(path, exclusions):
    return any(fnmatch(path.as_posix(), glob) or fnmatch(path.name, glob)
               for glob in exclusions)


def files_to_check(inputs, exclusions, excluded_directories=()):
    """Collect only candidate C files selected by the caller."""
    names, directories = directory_exclusions(excluded_directories)
    found = set()
    for path_str in inputs:
        path = Path(path_str)
        if not path.exists():
            raise ValueError(f"not a C file or directory: {path}")
        path = path.resolve()
        if excluded_dir(path, names, directories):
            continue
        if path.is_file():
            if (path.suffix.lower() in (".c", ".h") and
                    not excluded_file(path, exclusions)):
                found.add(path)
            continue
        if not path.is_dir():
            raise ValueError(f"not a C file or directory: {path}")

        for root, dirnames, filenames in os.walk(path):
            root_path = Path(root).resolve()
            dirnames[:] = [
                name for name in dirnames
                if not excluded_dir((root_path / name).resolve(),
                                    names, directories)
            ]
            for filename in filenames:
                candidate = (root_path / filename).resolve()
                if (candidate.suffix.lower() in (".c", ".h") and
                        not excluded_file(candidate, exclusions)):
                    found.add(candidate)
    return sorted(found)


def cwd_reference_files():
    """Return non-ignored C sources/headers below the current directory."""
    root = Path.cwd().resolve()
    temporary = None
    try:
        try:
            probe = subprocess.run(
                ["git", "-C", str(root), "rev-parse", "--show-toplevel"],
                capture_output=True, check=False)
        except OSError as error:
            raise ValueError(f"Git is required for ignore filtering: {error}")

        if probe.returncode == 0:
            command = ["git", "-C", str(root)]
        else:
            temporary = tempfile.TemporaryDirectory(
                prefix="single-use-git-")
            created = subprocess.run(
                ["git", "init", "--quiet", "--bare", temporary.name],
                capture_output=True, check=False)
            if created.returncode != 0:
                message = os.fsdecode(created.stderr).strip()
                raise ValueError(
                    f"cannot initialize temporary Git ignores: {message}")
            command = ["git", "-C", str(root),
                       f"--git-dir={temporary.name}",
                       f"--work-tree={root}"]

        result = subprocess.run(
            command + ["ls-files", "--cached", "--others",
                       "--exclude-standard", "-z", "--", "."],
            capture_output=True, check=False)
        if result.returncode != 0:
            message = os.fsdecode(result.stderr).strip()
            raise ValueError(f"Git ignore filtering failed: {message}")

        found = set()
        for item in result.stdout.split(b"\0"):
            if not item:
                continue
            path = (root / os.fsdecode(item)).resolve()
            if path.suffix.lower() in (".c", ".h") and path.is_file():
                found.add(path)
        return sorted(found)
    finally:
        if temporary is not None:
            temporary.cleanup()

def file_signature(path):
    """Use inode and nanosecond metadata without reading the source file."""
    info = path.stat()
    return [info.st_dev, info.st_ino, info.st_size,
            info.st_mtime_ns, info.st_ctime_ns]


def read_cache(path, fingerprint):
    try:
        with path.open(encoding="utf-8") as stream:
            cache = json.load(stream)
    except (OSError, ValueError, UnicodeError):
        return {}
    if (not isinstance(cache, dict) or
            cache.get("version") != CACHE_VERSION or
            cache.get("checker") != fingerprint or
            not isinstance(cache.get("files"), dict) or
            not isinstance(cache.get("runs"), dict)):
        return {}
    return cache


def write_cache(path, fingerprint, cache):
    """Best-effort atomic write; cache failures must not fail the check."""
    temporary = None
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
                mode="w", encoding="utf-8", dir=path.parent,
                prefix="single-use-", delete=False) as stream:
            temporary = Path(stream.name)
            # dumps uses the C JSON encoder in one pass; dump repeatedly
            # writes millions of tiny fragments on large source caches.
            stream.write(json.dumps(
                {"version": CACHE_VERSION, "checker": fingerprint,
                 "files": cache["files"], "runs": cache["runs"]},
                separators=(",", ":")))
        os.replace(temporary, path)
    except OSError:
        pass
    finally:
        if temporary is not None:
            try:
                temporary.unlink(missing_ok=True)
            except OSError:
                pass



def finding_rows(findings):
    return [[str(path), line, kind, name, explanation]
            for path, line, kind, name, explanation in findings]


def decode_findings(rows):
    findings = []
    for item in rows:
        if (not isinstance(item, list) or len(item) != 5 or
                not isinstance(item[0], str) or
                not isinstance(item[1], int) or
                not all(isinstance(value, str) for value in item[2:])):
            raise ValueError("invalid cached finding")
        findings.append((Path(item[0]), *item[1:]))
    return findings


def fast_cache_path(cache_path):
    return cache_path.with_name(cache_path.name + ".fast")


def read_fast_run(cache_path, fingerprint, run_key, signatures):
    # The large, parsed-source JSON cache is expensive to load just to
    # retrieve a previous run's findings.  A small sidecar can safely skip
    # it when the checker, source set, source metadata and main cache match.
    try:
        signature = file_signature(cache_path)
        with fast_cache_path(cache_path).open(encoding="utf-8") as stream:
            run = json.load(stream)
        if (run.get("checker") == fingerprint and
                run.get("cache_signature") == signature and
                run.get("run_key") == run_key and
                run.get("signatures") == signatures):
            return decode_findings(run["findings"])
    except (OSError, ValueError, KeyError, TypeError, IndexError):
        pass
    return None


def write_fast_run(cache_path, fingerprint, run_key, signatures, findings):
    temporary = None
    try:
        cache_signature = file_signature(cache_path)
        path = fast_cache_path(cache_path)
        with tempfile.NamedTemporaryFile(
                mode="w", encoding="utf-8", dir=path.parent,
                prefix="single-use-fast-", delete=False) as stream:
            temporary = Path(stream.name)
            json.dump({"checker": fingerprint,
                       "cache_signature": cache_signature,
                       "run_key": run_key, "signatures": signatures,
                       "findings": finding_rows(findings)}, stream,
                      separators=(",", ":"))
        os.replace(temporary, path)
    except OSError:
        pass
    finally:
        if temporary is not None:
            try:
                temporary.unlink(missing_ok=True)
            except OSError:
                pass


def cached_findings(candidate_paths, reference_paths, global_functions,
                    cache_path):
    """Analyze candidates against the complete reference-file universe."""
    fingerprint = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    all_paths = sorted(set(candidate_paths) | set(reference_paths))
    signatures = {str(path): file_signature(path) for path in all_paths}
    run_options = [[str(path) for path in candidate_paths],
                   [str(path) for path in reference_paths], global_functions]
    run_key = hashlib.sha256(json.dumps(run_options).encode()).hexdigest()
    findings = read_fast_run(cache_path, fingerprint, run_key, signatures)
    if findings is not None:
        return findings

    cache = read_cache(cache_path, fingerprint)
    if not cache:
        cache = {"files": {}, "runs": {}}
    entries = cache["files"]
    runs = cache["runs"]
    run = runs.get(run_key)
    if (isinstance(run, dict) and run.get("signatures") == signatures and
            isinstance(run.get("findings"), list)):
        try:
            findings = decode_findings(run["findings"])
            write_fast_run(cache_path, fingerprint, run_key, signatures,
                           findings)
            return findings
        except (TypeError, ValueError, IndexError):
            pass

    dirty = False
    sources = {}
    for path in all_paths:
        key = str(path)
        before = signatures[key]
        entry = entries.get(key)
        source = None
        if isinstance(entry, dict) and entry.get("signature") == before:
            try:
                source = Source.from_cache(path, entry["parsed"])
            except (KeyError, TypeError, ValueError, IndexError,
                    AttributeError):
                pass
        if source is None:
            source = Source(path)
            after = file_signature(path)
            if before == after:
                entries[key] = {"signature": after,
                                "parsed": source.to_cache()}
                dirty = True
            elif key in entries:
                del entries[key]
                dirty = True
        sources[path] = source

    candidate_sources = [sources[path] for path in candidate_paths]
    findings = analyze(candidate_sources, list(sources.values()),
                       global_functions, reference_paths)
    if all(file_signature(path) == signatures[str(path)] for path in all_paths):
        runs[run_key] = {
            "signatures": signatures,
            "findings": finding_rows(findings),
        }
        dirty = True
    if dirty:
        write_cache(cache_path, fingerprint, cache)
    write_fast_run(cache_path, fingerprint, run_key, signatures, findings)
    return findings

def clear_type_use(source, index, struct):
    """Require C type syntax; a coincidentally named value is not a use."""
    tokens = source.tokens
    value = tokens[index].value
    before = tokens[index - 1].value if index else ""
    after = tokens[index + 1].value if index + 1 < len(tokens) else ""
    before2 = tokens[index - 2].value if index >= 2 else ""
    after2 = tokens[index + 2].value if index + 2 < len(tokens) else ""

    if value == struct.tag and before == "struct":
        return after not in ("{", ";")
    if value != struct.alias:
        return False
    if before == "struct":
        return False
    if before == "(" and before2 in ("sizeof", "_Alignof", "_Generic"):
        return True
    if before == "(" and after == ")" and after2 == "{":
        return True
    # A declaration whose declarator begins with a name, pointer or array.
    if before not in ("{", "}", ";", "(", ",", "const", "volatile",
                      "restrict", "_Atomic", "static", "extern", "register"):
        return False
    if IDENTIFIER.fullmatch(after) and after not in CONTROL:
        return True
    if after == "*":
        cursor = index + 1
        while cursor < len(tokens) and tokens[cursor].value in (
                "*", "const", "volatile", "restrict"):
            cursor += 1
        if cursor < len(tokens) and IDENTIFIER.fullmatch(
                tokens[cursor].value):
            return True
    return False


def analyze(candidates, sources, global_functions=False,
            reference_paths=None):
    # Index only names that can produce findings.  Most source identifiers
    # can never be queried, so indexing all of them wastes time and memory.
    wanted = set()
    for source in candidates:
        wanted.update(function.name for function in source.functions)
        for struct in source.structs:
            wanted.update(struct.names)

    uses = defaultdict(list)
    for source in sources:
        for index, token in enumerate(source.tokens):
            if token.value in wanted:
                uses[token.value].append((source, index))

    if reference_paths is None:
        reference_paths = {source.path for source in sources}
    else:
        reference_paths = set(reference_paths)

    reference_included_c = set()
    reference_directives = set()
    for source in sources:
        if source.path not in reference_paths:
            continue
        reference_included_c.update(source.included_c)
        reference_directives.update(source.directive_uses)

    findings = []
    for source in candidates:
        included_c = reference_included_c | source.included_c
        directives = reference_directives | source.directive_uses
        if source.path.suffix != ".c" or source.path.name in included_c:
            continue
        for function in source.functions:
            if function.conditional or (not function.is_static and
                                        not global_functions):
                continue
            if function.name in directives:
                continue
            seen_calls = []
            valid = True
            for other, index in uses[function.name]:
                if other is not source and other.path not in reference_paths:
                    continue
                if other is not source:
                    # Even a mention in a header might be a macro or an API.
                    if other.path.suffix == ".h" or not function.is_static:
                        valid = False
                        break
                    # Distinct static functions can have the same name.
                    continue
                token = other.tokens[index]
                if index == function.name_index or \
                        index in other.prototype_indices:
                    continue
                # A prototype isn't a call; conservatively suppress the
                # result whenever any such declaration exists.
                if other.owners[index] < 0:
                    valid = False
                    break
                if token.line in source.conditional:
                    valid = False
                    break
                if index + 1 >= len(other.tokens) or \
                        other.tokens[index + 1].value != "(":
                    valid = False
                    break
                close = other.pairs.get(index + 1)
                if (close is None or index > 0 and
                        IDENTIFIER.fullmatch(other.tokens[index - 1].value)
                        and other.tokens[index - 1].value not in (
                            "return", "sizeof", "_Alignof", "if", "while")
                        and close + 1 < len(other.tokens)
                        and other.tokens[close + 1].value == ";"):
                    # A declaration inside a function is not a call.
                    valid = False
                    break
                if index > 0 and other.tokens[index - 1].value in (".", ">"):
                    valid = False
                    break
                caller = other.functions[other.owners[index]]
                if caller.name == function.name:
                    valid = False
                    break
                seen_calls.append(index)
                if len(seen_calls) > 1:
                    break
            if valid and len(seen_calls) == 1:
                findings.append((source.path, function.line, "function",
                                 function.name,
                                 "called once, in its defining file"))

        for struct in source.structs:
            if struct.conditional or any(name in directives
                                         for name in struct.names):
                continue
            owners = set()
            valid = True
            found_use = False
            for name in struct.names:
                for other, index in uses[name]:
                    if other is not source and other.path not in reference_paths:
                        continue
                    if other is source and struct.start <= index < struct.end:
                        continue
                    if other is not source:
                        valid = False
                        break
                    if other.tokens[index].line in source.conditional:
                        valid = False
                        break
                    owner = other.owners[index]
                    if owner < 0:
                        valid = False
                        break
                    if not clear_type_use(other, index, struct):
                        valid = False
                        break
                    owners.add(owner)
                    found_use = True
                    if len(owners) > 1:
                        valid = False
                        break
                if not valid:
                    break
            if valid and found_use and len(owners) == 1:
                findings.append((source.path, struct.line, "struct",
                                 struct.name,
                                 "used only within one function"))
    return sorted(findings, key=lambda row: (str(row[0]), row[1], row[2]))



def main():
    parser = argparse.ArgumentParser(
        description="Find single-use functions and function-local struct types "
                    "in selected C source files. Non-static functions are "
                    "skipped by default because external users cannot be "
                    "ruled out.",
        epilog="Candidate findings come only from files/directories supplied "
               "on the command line. For cross-file validation, every .c and "
               ".h file below the current working directory is checked. "
               "Conservative limitations: does not resolve macro expansions, "
               "function pointers, conditional compilation, or complex "
               "declarators. Findings are suggestions, not proof.")
    parser.add_argument("paths", nargs="*",
                        help="C files/directories to inspect; no default paths")
    parser.add_argument("--file-list", metavar="PATH",
                        help="newline-separated list of candidate paths")
    parser.add_argument("--exclude", action="append", default=[],
                        metavar="GLOB",
                        help="exclude matching candidate paths or basenames")
    parser.add_argument("--exclude-dir", "--ignore-dir", action="append",
                        default=[], metavar="DIR",
                        help="skip a candidate directory tree (repeatable); a "
                             "bare name matches anywhere and a path is "
                             "relative to the current directory")
    parser.add_argument("--no-cache", action="store_true",
                        help="parse all sources without reading or writing "
                             "the cache")
    parser.add_argument("--include-global", action="store_true",
                        help="also consider non-static functions (closed-world "
                             "assumption; external consumers may exist)")
    args = parser.parse_args()
    paths = list(args.paths)
    try:
        if args.file_list:
            paths += Path(args.file_list).read_text().splitlines()
        candidate_paths = files_to_check(paths, args.exclude,
                                         args.exclude_dir)
        if not candidate_paths:
            return 0
        reference_paths = cwd_reference_files()
        if args.no_cache:
            all_paths = sorted(set(candidate_paths) | set(reference_paths))
            sources = {path: Source(path) for path in all_paths}
            candidates = [sources[path] for path in candidate_paths]
            findings = analyze(candidates, list(sources.values()),
                               args.include_global, reference_paths)
        else:
            cache_path = Path(os.environ.get(CACHE_ENV, DEFAULT_CACHE))
            findings = cached_findings(candidate_paths, reference_paths,
                                       args.include_global, cache_path)
    except (OSError, ValueError) as error:
        parser.exit(2, f"{error}\n")
    for path, line, kind, name, explanation in findings:
        try:
            shown = path.relative_to(Path.cwd())
        except ValueError:
            shown = path
        print(f"{shown}:{line}: {kind} {name}: {explanation}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
