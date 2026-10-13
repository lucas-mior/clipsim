#!/usr/bin/env python3
"""Report int32 status temporaries whose assignment/use pairs should be inlined.

The narrow pattern is a function-local declaration ``int32 status;`` followed
only by one or more pairs in which ``status`` is assigned and then used on the
immediately following source line.  The declaration plus complete pairs means
that the variable has an odd number of references.  Comments, literals, member
names, and preprocessor directives do not count as uses.
"""

import argparse
import hashlib
import json
from dataclasses import dataclass
import os
from pathlib import Path
import re
import sys
import tempfile


IDENTIFIER = re.compile(r"[A-Za-z_]\w*\Z", re.ASCII)
LEXEMES = re.compile(r"[A-Za-z_]\w*|[^\s]", re.ASCII)
COMMENTS_AND_LITERALS = re.compile(
    r"/\*.*?\*/|//[^\n]*|(?:u8|[uUL])?\"(?:\\.|[^\"\\])*\""
    r"|(?:u8|[uUL])?'(?:\\.|[^'\\])*'",
    re.DOTALL,
)
DIRECTIVE = re.compile(r"^[ \t]*#")
STATUS_DECLARATION_CANDIDATE = re.compile(r"\bint32\s+status\s*;")
CACHE_ENV = "CBASE_STATUS_TEMPORARY_PATTERNS_CACHE"
DEFAULT_CACHE = ".cache/status_temporary_patterns.json"
CACHE_VERSION = 1
CONTROL = {
    "if", "while", "for", "switch", "sizeof", "_Alignof", "_Generic",
    "return", "typedef", "__attribute__",
}


@dataclass(frozen=True, slots=True)
class Token:
    value: str
    line: int


@dataclass(frozen=True)
class Function:
    name: str
    start: int
    end: int


def mask_non_newlines(match):
    return re.sub(r"[^\n]", " ", match.group())


def mask_directives(code):
    lines = code.splitlines(keepends=True)
    continued = False

    for index, line in enumerate(lines):
        directive = DIRECTIVE.match(line) is not None if not continued else True
        if directive:
            continued = line.rstrip("\r\n").endswith("\\")
            lines[index] = re.sub(r"[^\r\n]", " ", line)
        else:
            continued = False

    return "".join(lines)


def tokenize(source):
    code = COMMENTS_AND_LITERALS.sub(mask_non_newlines, source)
    code = mask_directives(code)
    tokens = []

    for line_number, line in enumerate(code.splitlines(), 1):
        for match in LEXEMES.finditer(line):
            tokens.append(Token(match.group(), line_number))

    return tokens


def matching_delimiters(tokens):
    pairs = {}
    stack = []
    closing_for = {"(": ")", "[": "]", "{": "}"}

    for index, token in enumerate(tokens):
        if token.value in closing_for:
            stack.append((token.value, index))
            continue
        if token.value not in closing_for.values():
            continue
        if not stack or closing_for[stack[-1][0]] != token.value:
            return None
        _, opening = stack.pop()
        pairs[opening] = index
        pairs[index] = opening

    if stack:
        return None
    return pairs


def function_from_head(tokens, start, brace, end, pairs):
    if brace <= start or tokens[brace - 1].value != ")":
        return None
    paren = pairs.get(brace - 1)
    if paren is None or paren <= start:
        return None

    name_index = paren - 1
    name = tokens[name_index].value
    if not IDENTIFIER.fullmatch(name) or name in CONTROL:
        return None

    prefix = tokens[start:name_index]
    if not prefix:
        return None
    if any(token.value in ("=", "typedef", ":") for token in prefix):
        return None
    if any(token.value in ("{", "}", ";") for token in prefix):
        return None

    return Function(name, brace + 1, end)


def functions_in(tokens):
    pairs = matching_delimiters(tokens)
    if pairs is None:
        return []

    functions = []
    start = 0
    index = 0
    while index < len(tokens):
        value = tokens[index].value
        if value == ";":
            start = index + 1
        elif value == "{":
            end = pairs[index]
            function = function_from_head(tokens, start, index, end, pairs)
            if function is not None:
                functions.append(function)
            index = end
            start = end + 1
        index += 1

    return functions


def is_member_name(tokens, index):
    if index > 0 and tokens[index - 1].value == ".":
        return True
    return (index > 1 and tokens[index - 2].value == "-"
            and tokens[index - 1].value == ">")


def status_references(tokens, function):
    references = []
    for index in range(function.start, function.end):
        if tokens[index].value != "status":
            continue
        if is_member_name(tokens, index):
            continue
        if index + 1 < function.end and tokens[index + 1].value == ":":
            continue
        references.append(index)
    return references


def status_declarations(tokens, function, references):
    declarations = []
    reference_set = set(references)

    for index in references:
        if index <= function.start or index + 1 >= function.end:
            continue
        if (tokens[index - 1].value == "int32"
                and tokens[index + 1].value == ";"):
            declarations.append(index)

    return [index for index in declarations if index in reference_set]


def is_simple_assignment_before_use(tokens, assignment, use):
    if assignment + 1 >= use or tokens[assignment + 1].value != "=":
        return False

    for index in range(assignment + 2, use):
        if tokens[index].value == ";":
            return True
    return False


def diagnostics_for(path):
    try:
        source = path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        raise ValueError(f"{path}: {error}") from error

    if STATUS_DECLARATION_CANDIDATE.search(source) is None:
        return []

    tokens = tokenize(source)
    diagnostics = []

    for function in functions_in(tokens):
        references = status_references(tokens, function)
        if len(references) < 3 or len(references)%2 == 0:
            continue

        declarations = status_declarations(tokens, function, references)
        if len(declarations) != 1 or declarations[0] != references[0]:
            continue

        pairs_inlineable = True
        for offset in range(1, len(references), 2):
            assignment = references[offset]
            use = references[offset + 1]

            if tokens[use].line != tokens[assignment].line + 1:
                pairs_inlineable = False
                break
            if not is_simple_assignment_before_use(tokens, assignment, use):
                pairs_inlineable = False
                break

        if not pairs_inlineable:
            continue

        line = tokens[declarations[0]].line
        diagnostics.append(
            f"{path}:{line}:inline the int32 status assignment/use pairs "
            f"into their immediately following uses in {function.name}\n"
        )

    return diagnostics


def collect_paths(inputs):
    paths = set()
    for value in inputs:
        path = Path(value)
        if not path.exists():
            raise ValueError(f"not a C file or directory: {path}")
        if path.is_file():
            if path.suffix.lower() in (".c", ".h"):
                paths.add(path)
            continue
        if not path.is_dir():
            raise ValueError(f"not a C file or directory: {path}")

        for root, _, filenames in os.walk(path):
            for name in filenames:
                file_path = Path(root) / name
                if file_path.suffix.lower() in (".c", ".h"):
                    paths.add(file_path)
    return sorted(paths, key=lambda item: item.as_posix())


def listed_paths(path):
    values = []
    try:
        with path.open(encoding="utf-8", errors="replace") as file_handle:
            for line in file_handle:
                value = line.rstrip("\r\n")
                if value:
                    values.append(value)
    except OSError as error:
        raise ValueError(f"{path}: {error}") from error
    return values


def file_signature(path):
    info = path.stat()
    return [info.st_dev, info.st_ino, info.st_size,
            info.st_mtime_ns, info.st_ctime_ns]


def read_cache(path, fingerprint):
    try:
        with path.open(encoding="utf-8") as stream:
            cache = json.load(stream)
        if (isinstance(cache, dict) and
                cache.get("version") == CACHE_VERSION and
                cache.get("checker") == fingerprint and
                isinstance(cache.get("files"), dict)):
            return cache["files"]
    except (OSError, ValueError, UnicodeError):
        pass
    return {}


def write_cache(path, fingerprint, entries):
    temporary = None
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
                mode="w", encoding="utf-8", dir=path.parent,
                prefix="status-temporary-", delete=False) as stream:
            temporary = Path(stream.name)
            json.dump({"version": CACHE_VERSION, "checker": fingerprint,
                       "files": entries}, stream, separators=(",", ":"))
        os.replace(temporary, path)
    except OSError:
        pass
    finally:
        if temporary is not None:
            try:
                temporary.unlink(missing_ok=True)
            except OSError:
                pass


def cached_diagnostics(paths, cache_path):
    fingerprint = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    entries = read_cache(cache_path, fingerprint)
    dirty = False
    for path in paths:
        key = str(path)
        before = file_signature(path)
        entry = entries.get(key)
        if (isinstance(entry, dict) and
                entry.get("signature") == before and
                isinstance(entry.get("diagnostics"), list) and
                all(isinstance(line, str)
                    for line in entry["diagnostics"])):
            diagnostics = entry["diagnostics"]
        else:
            diagnostics = diagnostics_for(path)
            after = file_signature(path)
            if before == after:
                entries[key] = {"signature": after,
                                "diagnostics": diagnostics}
                dirty = True
            elif key in entries:
                del entries[key]
                dirty = True
        for diagnostic in diagnostics:
            sys.stdout.write(diagnostic)
    if dirty:
        write_cache(cache_path, fingerprint, entries)


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="detect inlineable int32 status temporaries"
    )
    parser.add_argument("paths", nargs="*", help="C files or directories")
    parser.add_argument("--no-cache", action="store_true",
                        help="parse sources without reading or writing cache")
    parser.add_argument(
        "--file-list", metavar="PATH",
        help="read C file paths from PATH, one per line",
    )
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    inputs = list(args.paths)
    if args.file_list is not None:
        inputs.extend(listed_paths(Path(args.file_list)))

    try:
        paths = collect_paths(inputs)
        if args.no_cache:
            for path in paths:
                for diagnostic in diagnostics_for(path):
                    sys.stdout.write(diagnostic)
        elif paths:
            cache_path = Path(os.environ.get(CACHE_ENV, DEFAULT_CACHE))
            cached_diagnostics(paths, cache_path)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
