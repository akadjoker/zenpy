#!/usr/bin/env python3
import argparse
import ast
import html
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import List, Optional


@dataclass
class FunctionDoc:
    name: str
    signature: str
    doc: str
    lineno: int


@dataclass
class ClassDoc:
    name: str
    bases: List[str]
    doc: str
    lineno: int
    methods: List[FunctionDoc]


@dataclass
class ModuleDoc:
    path: str
    doc: str
    classes: List[ClassDoc]
    functions: List[FunctionDoc]


@dataclass
class ApiDoc:
    generated_from: List[str]
    modules: List[ModuleDoc]


def _ann_to_str(node: Optional[ast.AST]) -> str:
    if node is None:
        return ""
    try:
        return ast.unparse(node)
    except Exception:
        return ""


def _format_arg(a: ast.arg, default: Optional[ast.AST]) -> str:
    s = a.arg
    ann = _ann_to_str(a.annotation)
    if ann:
        s += f": {ann}"
    if default is not None:
        try:
            s += f"={ast.unparse(default)}"
        except Exception:
            s += "=<...>"
    return s


def _signature(node: ast.FunctionDef) -> str:
    args = node.args
    parts: List[str] = []

    posonly = list(args.posonlyargs)
    regular = list(args.args)
    defaults = list(args.defaults)
    default_offset = len(posonly) + len(regular) - len(defaults)

    all_pos = posonly + regular
    for i, arg in enumerate(all_pos):
        default_node = defaults[i - default_offset] if i >= default_offset else None
        parts.append(_format_arg(arg, default_node))
        if i + 1 == len(posonly) and posonly:
            parts.append("/")

    if args.vararg:
        parts.append("*" + _format_arg(args.vararg, None))
    elif args.kwonlyargs:
        parts.append("*")

    for kwarg, kwdefault in zip(args.kwonlyargs, args.kw_defaults):
        parts.append(_format_arg(kwarg, kwdefault))

    if args.kwarg:
        parts.append("**" + _format_arg(args.kwarg, None))

    ret = _ann_to_str(node.returns)
    sig = f"({', '.join(parts)})"
    if ret:
        sig += f" -> {ret}"
    return sig


def _func_doc(node: ast.FunctionDef) -> FunctionDoc:
    return FunctionDoc(
        name=node.name,
        signature=_signature(node),
        doc=ast.get_docstring(node) or "",
        lineno=getattr(node, "lineno", 0),
    )


def _class_doc(node: ast.ClassDef) -> ClassDoc:
    bases = []
    for b in node.bases:
        try:
            bases.append(ast.unparse(b))
        except Exception:
            bases.append("<base>")

    methods: List[FunctionDoc] = []
    for n in node.body:
        if isinstance(n, ast.FunctionDef):
            methods.append(_func_doc(n))

    return ClassDoc(
        name=node.name,
        bases=bases,
        doc=ast.get_docstring(node) or "",
        lineno=getattr(node, "lineno", 0),
        methods=methods,
    )


def parse_module(path: Path) -> Optional[ModuleDoc]:
    text = path.read_text(encoding="utf-8")
    try:
        tree = ast.parse(text, filename=str(path))
    except SyntaxError:
        return None

    classes: List[ClassDoc] = []
    functions: List[FunctionDoc] = []

    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            classes.append(_class_doc(node))
        elif isinstance(node, ast.FunctionDef):
            functions.append(_func_doc(node))

    return ModuleDoc(
        path=str(path),
        doc=ast.get_docstring(tree) or "",
        classes=classes,
        functions=functions,
    )


def render_html(api: ApiDoc, title: str = "Engine API") -> str:
    out: List[str] = []
    out.append("<!doctype html>")
    out.append("<html><head><meta charset='utf-8'>")
    out.append(f"<title>{html.escape(title)}</title>")
    out.append(
        "<style>body{font-family:ui-sans-serif,system-ui;margin:2rem;line-height:1.45;}"
        "code{background:#f3f3f3;padding:.1rem .3rem;border-radius:4px;}"
        "h1,h2,h3{margin-top:1.2em;}"
        "section{border-top:1px solid #ddd;padding-top:1rem;margin-top:1rem;}"
        "small{color:#666;}"
        "</style></head><body>"
    )
    out.append(f"<h1>{html.escape(title)}</h1>")

    for mod in api.modules:
        out.append("<section>")
        out.append(f"<h2>{html.escape(mod.path)}</h2>")
        if mod.doc:
            out.append(f"<p>{html.escape(mod.doc)}</p>")

        if mod.functions:
            out.append("<h3>Functions</h3>")
            for f in mod.functions:
                out.append(
                    f"<div><code>{html.escape(f.name + f.signature)}</code> "
                    f"<small>(line {f.lineno})</small></div>"
                )
                if f.doc:
                    out.append(f"<p>{html.escape(f.doc)}</p>")

        if mod.classes:
            out.append("<h3>Classes</h3>")
            for c in mod.classes:
                base_txt = f"({', '.join(c.bases)})" if c.bases else ""
                out.append(
                    f"<div><code>{html.escape(c.name + base_txt)}</code> "
                    f"<small>(line {c.lineno})</small></div>"
                )
                if c.doc:
                    out.append(f"<p>{html.escape(c.doc)}</p>")
                for m in c.methods:
                    out.append(
                        f"<div style='margin-left:1rem'><code>{html.escape(m.name + m.signature)}</code> "
                        f"<small>(line {m.lineno})</small></div>"
                    )
                    if m.doc:
                        out.append(f"<p style='margin-left:1rem'>{html.escape(m.doc)}</p>")

        out.append("</section>")

    out.append("</body></html>")
    return "\n".join(out)


def render_rst(api: ApiDoc, title: str = "Engine API") -> str:
    out: List[str] = []
    out.append(title)
    out.append("=" * len(title))
    out.append("")

    for mod in api.modules:
        out.append(mod.path)
        out.append("-" * len(mod.path))
        out.append("")
        if mod.doc:
            out.append(mod.doc)
            out.append("")

        if mod.functions:
            out.append("Functions")
            out.append("~~~~~~~~~")
            out.append("")
            for f in mod.functions:
                out.append(f"* ``{f.name}{f.signature}`` (line {f.lineno})")
                if f.doc:
                    out.append(f"  {f.doc}")
            out.append("")

        if mod.classes:
            out.append("Classes")
            out.append("~~~~~~~")
            out.append("")
            for c in mod.classes:
                base_txt = f"({', '.join(c.bases)})" if c.bases else ""
                out.append(f"* ``{c.name}{base_txt}`` (line {c.lineno})")
                if c.doc:
                    out.append(f"  {c.doc}")
                for m in c.methods:
                    out.append(f"  * ``{m.name}{m.signature}`` (line {m.lineno})")
                    if m.doc:
                        out.append(f"    {m.doc}")
            out.append("")

    return "\n".join(out)


def collect_files(inputs: List[str]) -> List[Path]:
    files: List[Path] = []
    for item in inputs:
        p = Path(item)
        if p.is_file() and p.suffix == ".py":
            files.append(p)
        elif p.is_dir():
            files.extend(sorted(x for x in p.rglob("*.py") if x.is_file()))
    return sorted(set(files))


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate API docs JSON + HTML + RST from Python files")
    parser.add_argument("inputs", nargs="*", help="Python files or directories")
    parser.add_argument("--from-json", default="", help="Load API model from an existing JSON file")
    parser.add_argument("--json-out", default="docs/api/api.json")
    parser.add_argument("--html-out", default="docs/api/api.html")
    parser.add_argument("--rst-out", default="docs/api/api.rst")
    parser.add_argument("--title", default="Engine Script API")
    args = parser.parse_args()

    if args.from_json:
        model = json.loads(Path(args.from_json).read_text(encoding="utf-8"))
        api = ApiDoc(
            generated_from=model.get("generated_from", []),
            modules=[
                ModuleDoc(
                    path=m.get("path", ""),
                    doc=m.get("doc", ""),
                    classes=[
                        ClassDoc(
                            name=c.get("name", ""),
                            bases=c.get("bases", []),
                            doc=c.get("doc", ""),
                            lineno=c.get("lineno", 0),
                            methods=[
                                FunctionDoc(
                                    name=fn.get("name", ""),
                                    signature=fn.get("signature", "()"),
                                    doc=fn.get("doc", ""),
                                    lineno=fn.get("lineno", 0),
                                )
                                for fn in c.get("methods", [])
                            ],
                        )
                        for c in m.get("classes", [])
                    ],
                    functions=[
                        FunctionDoc(
                            name=f.get("name", ""),
                            signature=f.get("signature", "()"),
                            doc=f.get("doc", ""),
                            lineno=f.get("lineno", 0),
                        )
                        for f in m.get("functions", [])
                    ],
                )
                for m in model.get("modules", [])
            ],
        )
    else:
        if not args.inputs:
            parser.error("Provide inputs or use --from-json")
        files = collect_files(args.inputs)
        modules: List[ModuleDoc] = []
        for f in files:
            mod = parse_module(f)
            if mod is not None:
                modules.append(mod)
        api = ApiDoc(generated_from=[str(f) for f in files], modules=modules)

    json_path = Path(args.json_out)
    html_path = Path(args.html_out)
    rst_path = Path(args.rst_out)
    json_path.parent.mkdir(parents=True, exist_ok=True)
    html_path.parent.mkdir(parents=True, exist_ok=True)
    rst_path.parent.mkdir(parents=True, exist_ok=True)

    json_path.write_text(json.dumps(asdict(api), indent=2), encoding="utf-8")
    html_path.write_text(render_html(api, title=args.title), encoding="utf-8")
    rst_path.write_text(render_rst(api, title=args.title), encoding="utf-8")

    print(f"Generated {json_path}")
    print(f"Generated {html_path}")
    print(f"Generated {rst_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
