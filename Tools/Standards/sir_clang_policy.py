#!/usr/bin/env python3
"""SIREngine first-party Clang 23 AST policy gate.

Reads authentic compilation commands; never manufactures build arguments. A small
standalone test mode accepts --file/--std and is restricted to fixture validation.
This is an AST gate, not a compiled clang-tidy plugin.
"""
from __future__ import annotations

import argparse
import bisect
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

SOURCE_DIRS = ("Source", "Tests", "Tools")
EXCLUDED_PARTS = {"ThirdParty", "External", "Vendor", "out", ".git", "build"}
DECL_KINDS = {"VarDecl", "FieldDecl", "ParmVarDecl"}
METHOD_KINDS = {"CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl", "CXXConversionDecl"}
FORBIDDEN_STD = (
    "thread", "mutex", "lock_guard", "condition_variable", "function",
    "unique_ptr", "shared_ptr", "weak_ptr", "string_view", "size_t",
    "copy", "copy_n", "copy_if", "fill", "fill_n", "move", "move_backward",
    "copy_backward", "transform", "for_each", "accumulate", "bind", "mem_fn",
    "ranges", "views", "async", "future",
)
FORBIDDEN_KIND = {
    "LambdaExpr": ("SIR001", "lambdas are forbidden"),
    "CXXForRangeStmt": ("SIR002", "range-based for loops are forbidden"),
    "CXXConstCastExpr": ("SIR003", "const_cast is forbidden"),
    "CXXThrowExpr": ("SIR004", "throw is forbidden; use a nonthrowing result"),
    "CXXTryStmt": ("SIR005", "try/catch is forbidden"),
    "UsingDirectiveDecl": ("SIR006", "using namespace is forbidden"),
    "TypeAliasDecl": ("SIR007", "using type aliases are forbidden"),
    "NamespaceDecl": ("SIR008", "SIREngine declarations must be in the global namespace"),
    "CXXNewExpr": (None, None),  # allowed: overridden operators route allocation
}
TYPE_PRIMITIVE = re.compile(r"^(?:const\s+|volatile\s+|signed\s+|unsigned\s+)*(?:int|short|long|char)(?:\s|\*|&|\[|$)")
LEX_SKIP = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|(?:u8|u|U|L)?\'(?:\\.|[^\'\\])*\'')


def mask_noncode(data: str) -> str:
    return LEX_SKIP.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), data)


class SourceCache:
    def __init__(self):
        self._data = {}

    def get(self, path: Path):
        path = path.resolve()
        if path not in self._data:
            try:
                src = path.read_bytes().decode("latin-1")
            except OSError:
                src = ""
            starts = [0] + [m.end() for m in re.finditer("\n", src)]
            self._data[path] = (src, mask_noncode(src), starts)
        return self._data[path]


class Checker:
    def __init__(self, root: Path, mainfile: Path, standalone=False):
        self.root = root.resolve()
        self.mainfile = mainfile.resolve()
        self.standalone = standalone
        self.cache = SourceCache()
        self.errors = []
        self.seen = set()

    def owned(self, path):
        if not path:
            return False
        path = Path(path).resolve()
        if self.standalone:
            return path == self.mainfile
        try:
            rel = path.relative_to(self.root)
        except ValueError:
            return False
        return len(rel.parts) >= 2 and rel.parts[0] in SOURCE_DIRS and not any(
            segment in EXCLUDED_PARTS for segment in rel.parts
        )

    def location(self, node, inherited):
        loc = node.get("loc", {})
        begin = node.get("range", {}).get("begin", {})
        raw = loc.get("file") or begin.get("file")
        file = Path(raw).resolve() if raw else inherited
        off = loc.get("offset", begin.get("offset", 0))
        if file:
            src, _, starts = self.cache.get(file)
            if isinstance(off, int) and 0 <= off <= len(src):
                line = bisect.bisect_right(starts, off)
                col = off - starts[line - 1] + 1
                return file, line, col
        return file, loc.get("line", 1), loc.get("col", 1)

    def segment(self, node, path):
        if path is None:
            return ""
        src = self.cache.get(path)[0]
        rng = node.get("range", {})
        begin, end = rng.get("begin", {}), rng.get("end", {})
        a, b = begin.get("offset"), end.get("offset")
        if isinstance(a, int) and isinstance(b, int) and 0 <= a <= b < len(src):
            return src[a:b + max(1, end.get("tokLen", 1))]
        return ""

    def report(self, node, path, rule, message, pos=None):
        if not self.owned(path) or not rule:
            return
        locpath, line, col = self.location(node, path)
        if pos is not None:
            starts = self.cache.get(path)[2]
            line = bisect.bisect_right(starts, pos)
            col = pos - starts[line - 1] + 1
        key = (str(path), line, col, rule)
        if key not in self.seen:
            self.seen.add(key)
            self.errors.append({"file": str(path), "line": line, "column": col, "rule": rule, "message": message})

    def annotated(self, path, node, category):
        src = self.cache.get(path)[0]
        begin = node.get("range", {}).get("begin", {}).get("offset", node.get("loc", {}).get("offset", 0))
        if not isinstance(begin, int):
            return False
        prefix = src[max(0, begin - 240):begin]
        lines = prefix.splitlines()[-3:]
        return any(re.search(r"SIR-POLICY-ALLOW:\s*" + re.escape(category) + r"\b\s+\S.{8,}", line) for line in lines)

    @staticmethod
    def nonimplicit(node):
        return not node.get("isImplicit", False)

    @staticmethod
    def is_class(node):
        return node.get("kind") == "CXXRecordDecl" and node.get("tagUsed") == "class" and node.get("completeDefinition")

    def check_record(self, node, path):
        members = [c for c in node.get("inner", []) if self.nonimplicit(c)]
        tag = node.get("tagUsed")
        methods = [c for c in members if c.get("kind") in METHOD_KINDS]
        if tag == "class":
            ctor_dtor_positions = [i for i, c in enumerate(members) if c.get("kind") in ("CXXConstructorDecl", "CXXDestructorDecl")]
            if ctor_dtor_positions:
                meaningful = [i for i in range(min(ctor_dtor_positions), max(ctor_dtor_positions) + 1)
                              if members[i].get("kind") != "AccessSpecDecl"]
                if meaningful != ctor_dtor_positions:
                    self.report(node, path, "SIR020", "constructors and destructor must be grouped without intervening members")
            names = {"copy_ctor": False, "move_ctor": False, "copy_assign": False, "move_assign": False}
            class_name = re.escape(node.get("name", ""))
            for child in methods:
                kind = child.get("kind")
                signature = child.get("type", {}).get("qualType", "")
                if kind == "CXXConstructorDecl":
                    if re.search(r"\(\s*const\s+" + class_name + r"\s*&\s*\)", signature):
                        names["copy_ctor"] = True
                    if re.search(r"\(\s*" + class_name + r"\s*&&\s*\)", signature):
                        names["move_ctor"] = True
                elif child.get("name") == "operator=":
                    if re.search(r"\(\s*const\s+" + class_name + r"\s*&\s*\)", signature):
                        names["copy_assign"] = True
                    if re.search(r"\(\s*" + class_name + r"\s*&&\s*\)", signature):
                        names["move_assign"] = True
            missing = [k for k, v in names.items() if not v]
            if missing:
                self.report(node, path, "SIR021", "class must explicitly declare " + ", ".join(missing))
            specs = [c for c in members if c.get("kind") == "AccessSpecDecl"]
            order = {"public": 0, "protected": 1, "private": 2}
            levels = [order.get(c.get("access"), 0) for c in specs]
            if levels != sorted(levels) or len(levels) != len(set(levels)):
                self.report(node, path, "SIR022", "class access sections must occur once in public, protected, private order")
            for i, spec in enumerate(specs):
                if spec.get("access") != "protected":
                    continue
                start = members.index(spec)
                end = next((j for j in range(start + 1, len(members)) if members[j].get("kind") == "AccessSpecDecl"), len(members))
                if not any(c.get("kind") not in ("AccessSpecDecl",) for c in members[start + 1:end]):
                    self.report(spec, path, "SIR023", "empty protected section is forbidden")
        for method in methods:
            kind = method.get("kind")
            segment = self.segment(method, path)
            has_body = any(c.get("kind") in ("CompoundStmt", "CXXTryStmt") for c in method.get("inner", []))
            const_eval = method.get("constexpr") or method.get("consteval") or re.search(r"\b(?:constexpr|consteval)\b", segment[:150])
            if has_body and not const_eval:
                self.report(method, path, "SIR024", "methods must be defined outside class/struct declarations (constexpr/consteval excepted)")
            if kind == "CXXConstructorDecl" and not re.search(r"\bexplicit\b", segment.split("(", 1)[0]):
                self.report(method, path, "SIR025", "every constructor must be explicitly declared explicit")
            if kind == "CXXDestructorDecl" and re.search(r"\(\s*void\s*\)", segment):
                self.report(method, path, "SIR026", "destructor declarations must use () instead of ( void )")
            if tag == "struct" and kind == "CXXMethodDecl" and not method.get("name", "").startswith("operator"):
                body = next((c for c in method.get("inner", []) if c.get("kind") == "CompoundStmt"), None)
                too_large = body and len(body.get("inner", [])) > 3
                if too_large:
                    self.report(method, path, "SIR027", "struct utility methods must remain very small (at most three direct statements)")
                if not body and not self.annotated(path, method, "struct-utility"):
                    # External definition may be small; declaration alone cannot prove it.
                    pass

    def check_decl(self, node, path, parent):
        kind = node.get("kind", "")
        if kind in FORBIDDEN_KIND and FORBIDDEN_KIND[kind][0]:
            rule, msg = FORBIDDEN_KIND[kind]
            self.report(node, path, rule, msg)
        if kind == "TypedefDecl" and self.nonimplicit(node):
            part = self.segment(node, path)
            if part and not re.match(r"\s*typedef\s+(?:struct|enum|union)\b", part):
                self.report(node, path, "SIR009", "typedef aliases are forbidden except canonical typedef struct/enum/union declarations")
        if kind in DECL_KINDS:
            qual = node.get("type", {}).get("qualType", "")
            if kind == "FieldDecl" and re.match(r"^(?:const\s+)?bool(?:\s*\[|$)", qual) and not node.get("isBitfield"):
                self.report(node, path, "SIR010", "stored bool fields must use bitfields")
            if TYPE_PRIMITIVE.search(qual) and not self.annotated(path, node, "integer-abi"):
                self.report(node, path, "SIR011", "use fixed-width integer types instead of primitive " + qual)
            if re.match(r"^(?:const\s+)?double(?:\s|$)", qual) and not self.annotated(path, node, "double"):
                self.report(node, path, "SIR012", "double requires a documented reason")
            if kind == "VarDecl" and parent and parent.get("kind") in ("TranslationUnitDecl", "NamespaceDecl"):
                self.report(node, path, "SIR013", "free-floating variables are forbidden; use class static constexpr members")
            snippet = self.segment(node, path)
            prefix = snippet.split(node.get("name", ""), 1)[0] if node.get("name") else snippet
            if re.search(r"\bauto\b", prefix):
                self.report(node, path, "SIR014", "auto declarations are forbidden")
            if re.search(r"\binline\b", prefix):
                self.report(node, path, "SIR015", "explicit inline variables are forbidden")
        if kind == "EnumDecl" and node.get("name") and node.get("scopedEnumTag") != "class":
            self.report(node, path, "SIR037", "enumerations must be enum class sir[EnumName]")
        if kind == "EnumDecl" and node.get("fixedUnderlyingType"):
            under = node["fixedUnderlyingType"].get("qualType", "")
            if under not in ("int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t", "int64_t", "uint64_t"):
                self.report(node, path, "SIR029", "enum class must have a fixed-width underlying type")
        if kind in ("FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl", "FunctionTemplateDecl"):
            params = [c for c in node.get("inner", []) if c.get("kind") == "ParmVarDecl"]
            if len(params) > 4:
                self.report(node, path, "SIR016", "functions may have at most four explicit parameters")
            signature = node.get("type", {}).get("qualType", "")
            returned = signature.split("(", 1)[0].strip()
            if kind == "FunctionDecl" and node.get("name") != "main" and TYPE_PRIMITIVE.search(returned):
                if not self.annotated(path, node, "integer-abi"):
                    self.report(node, path, "SIR035", "function return type must use a fixed-width integer type")
            if kind == "FunctionDecl" and parent and parent.get("kind") in ("TranslationUnitDecl", "NamespaceDecl"):
                if node.get("name") not in ("main", "operator new", "operator delete", "operator new[]", "operator delete[]") and not self.annotated(path, node, "free-function"):
                    self.report(node, path, "SIR017", "free functions require a documented SIR-POLICY-ALLOW: free-function reason")
            if kind != "FunctionTemplateDecl":
                if (kind == "CXXMethodDecl" and path and path.suffix.lower() in (".h", ".hpp")
                        and parent and parent.get("kind") == "TranslationUnitDecl"
                        and any(c.get("kind") == "CompoundStmt" for c in node.get("inner", []))
                        and not node.get("constexpr") and not node.get("consteval")
                        and "SIR_FORCEINLINE" not in self.segment(node, path)):
                    self.report(node, path, "SIR038", "out-of-class header method definitions require SIR_FORCEINLINE")
                seg = self.segment(node, path)
                head = seg.split("(", 1)[0]
                if re.search(r"\binline\b", head):
                    self.report(node, path, "SIR018", "explicit inline is forbidden; use SIR_FORCEINLINE when required")
                if re.search(r"\bauto\b", head):
                    self.report(node, path, "SIR019", "auto return types are forbidden")
        if kind == "IfStmt":
            seg = self.segment(node, path)
            # Check the first condition; body operators are outside the closing delimiter.
            pos = seg.find("(")
            if pos >= 0:
                level, end = 0, -1
                for i in range(pos, len(seg)):
                    if seg[i] == "(":
                        level += 1
                    elif seg[i] == ")":
                        level -= 1
                        if not level:
                            end = i
                            break
                if end >= 0 and len(re.findall(r"&&|\|\|", seg[pos:end])) >= 3:
                    self.report(node, path, "SIR028", "if statements may contain at most three conditions")

    def walk(self, root):
        # Clang's AST JSON omits repeated file properties for sibling nodes.
        last_known = self.mainfile

        def visit(node, parent=None, inherited=None):
            nonlocal last_known
            loc = node.get("loc", {})
            begin = node.get("range", {}).get("begin", {})
            explicit = loc.get("file") or begin.get("file")
            if explicit:
                last_known = Path(explicit).resolve()
            # Top-level siblings inherit the last Clang source file, not the main TU.
            # Clang elides repeated filenames from header AST nodes.
            if explicit:
                current = Path(explicit).resolve()
            elif parent and parent.get("kind") == "TranslationUnitDecl":
                current = last_known
            else:
                current = inherited or last_known
            # A synthetic/implicit record may have positions referring to another TU.
            if self.owned(current) and self.nonimplicit(node):
                self.check_decl(node, current, parent)
                if node.get("kind") == "CXXRecordDecl" and node.get("completeDefinition"):
                    self.check_record(node, current)
            for child in node.get("inner", []):
                visit(child, node, current)

        visit(root, None, self.mainfile)

    def scan_text(self, files):
        for path in files:
            if not self.owned(path):
                continue
            original, clean, starts = self.cache.get(path)
            pattern = re.compile(r"\bstd\s*::\s*(" + "|".join(re.escape(n) for n in FORBIDDEN_STD) + r")\b")
            for match in pattern.finditer(clean):
                self.report({}, path, "SIR030", "forbidden std:: facility: " + match.group(0), pos=match.start())
            for match in re.finditer(r"(?<![\w:])(?:malloc|free|calloc|realloc)\s*\(", clean):
                self.report({}, path, "SIR031", "C heap allocation APIs are forbidden", pos=match.start())
            for match in re.finditer(r"\busing\s+[a-zA-Z_]\w*\s*=", clean):
                self.report({}, path, "SIR032", "using aliases are forbidden", pos=match.start())
            for match in re.finditer(r"\busing\s+namespace\b", clean):
                self.report({}, path, "SIR033", "using namespace is forbidden", pos=match.start())
            for match in re.finditer(r"\b(?:const_cast|dynamic_pointer_cast|static_pointer_cast)\s*<", clean):
                if match.group(0).startswith("const_cast"):
                    self.report({}, path, "SIR034", "const_cast is forbidden", pos=match.start())

    def scan_headers(self):
        if self.standalone:
            return
        for dirname in SOURCE_DIRS:
            base = self.root / dirname
            if not base.is_dir():
                continue
            for path in base.rglob("*"):
                if path.suffix.lower() not in (".h", ".hpp") or not self.owned(path):
                    continue
                src, clean, _ = self.cache.get(path)
                basename = path.stem
                if not basename.startswith("SIR"):
                    continue
                expected = "__SIR_" + basename[3:].upper() + "_H__"
                match = re.search(r"^\s*#\s*ifndef\s+(\w+)", clean, re.M)
                # Preprocessor lines survive masking of comments and strings.
                define = re.search(r"^\s*#\s*define\s+(\w+)", clean, re.M)
                if not match or match.group(1) != expected or not define or define.group(1) != expected:
                    self.report({}, path, "SIR040", "header guard must declare and define " + expected, pos=0)
                if path.name != "SIRPCH.h" and "#include <Core/SIRPCH.h>" not in src:
                    self.report({}, path, "SIR041", "first-party headers must include <Core/SIRPCH.h>", pos=0)
                self.scan_text([path])


def source_from_compdb(entry, root):
    base = Path(entry.get("directory", str(root))).resolve()
    p = Path(entry["file"])
    return p.resolve() if p.is_absolute() else (base / p).resolve()


def run_ast(clang, file, argv, cwd, timeout, max_bytes):
    # Retain all authentic compilation flags; suppress only output/dependency emission.
    args = list(argv)
    if not args:
        raise ValueError("empty compilation command")
    args[0] = clang
    cleaned = []
    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ("-o", "-MF", "-MT", "-MQ", "-MJ"):
            i += 2
            continue
        if arg in ("-c", "-MD", "-MMD", "-MP", "-MG") or arg.startswith("-o") and len(arg) > 2:
            i += 1
            continue
        cleaned.append(arg)
        i += 1
    cleaned += ["-fsyntax-only", "-Xclang", "-ast-dump=json"]
    proc = subprocess.run(cleaned, cwd=cwd, capture_output=True, timeout=timeout)
    if len(proc.stdout) > max_bytes:
        raise ValueError("AST exceeded configured maximum output bytes")
    if proc.returncode:
        raise ValueError(proc.stderr.decode("utf-8", errors="replace")[-4000:])
    return json.loads(proc.stdout)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo", type=Path, default=Path.cwd())
    ap.add_argument("--compdb", type=Path, help="authentic compile_commands.json from the project build")
    ap.add_argument("--file", type=Path, help="isolated fixture mode, without compilation database")
    ap.add_argument("--std", choices=("c++17", "c++23"), default="c++23")
    ap.add_argument("--clang", default="clang++-23")
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("--max-ast-mb", type=int, default=160)
    ap.add_argument("--json", type=Path, help="write diagnostics and coverage as JSON")
    args = ap.parse_args()
    root = args.repo.resolve()
    if not args.file and not args.compdb:
        ap.error("--compdb is required for repository qualification")
    if args.file and args.compdb:
        ap.error("choose --file or --compdb, not both")
    if args.file:
        path = args.file.resolve()
        entries = [(path, [args.clang, "-std=" + args.std, str(path)], path.parent)]
    else:
        data = json.loads(args.compdb.resolve().read_text())
        entries = []
        for entry in data:
            file = source_from_compdb(entry, root)
            command = entry.get("arguments") or shlex.split(entry["command"])
            directory = Path(entry["directory"]).resolve()
            entries.append((file, command, directory))
    dedup = set()
    diagnostics = []
    examined = []
    failed = []
    for file, command, directory in entries:
        check = Checker(root, file, standalone=bool(args.file))
        if not check.owned(file):
            continue
        if (str(file), tuple(command)) in dedup:
            continue
        dedup.add((str(file), tuple(command)))
        try:
            tree = run_ast(args.clang, file, command, directory, args.timeout, args.max_ast_mb * 1024 * 1024)
            check.walk(tree)
            check.scan_text([file])
            examined.append(str(file))
            diagnostics.extend(check.errors)
        except (OSError, ValueError, subprocess.TimeoutExpired, json.JSONDecodeError) as exc:
            failed.append({"file": str(file), "reason": str(exc)[-1500:]})
    if args.compdb:
        header_checker = Checker(root, root / "Source", standalone=False)
        header_checker.scan_headers()
        diagnostics.extend(header_checker.errors)
    unique = { (d["file"], d["line"], d["column"], d["rule"]): d for d in diagnostics }
    diagnostics = sorted(unique.values(), key=lambda d: (d["file"], d["line"], d["column"], d["rule"]))
    status = "FAIL" if diagnostics else "PASS"
    if failed or not examined:
        status = "INCOMPLETE"
    report = {"status": status, "checked_tus": len(examined), "diagnostics": diagnostics, "failed_tus": failed}
    for d in diagnostics:
        print(f'{d["file"]}:{d["line"]}:{d["column"]}: error: {d["message"]} [{d["rule"]}]')
    for f in failed:
        print(f'{f["file"]}: error: AST parse failed: {f["reason"]}', file=sys.stderr)
    print(f'SIREngine AST policy: {status}, TUs={len(examined)}, violations={len(diagnostics)}, failures={len(failed)}')
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2) + "\n")
    return 2 if status == "INCOMPLETE" else (1 if status == "FAIL" else 0)


if __name__ == "__main__":
    sys.exit(main())
