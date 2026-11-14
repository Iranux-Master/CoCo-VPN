#!/usr/bin/env python3
"""Generate docs/ai/CODE_MAP.md: the QML <-> C++ wiring of CoCo VPN, without reading the source.

Graphify (tools/graphify/README.md) does not parse QML, so this script covers the gap an agent
otherwise fills by grepping 150+ QML files:

  1. every object CoreController exposes to QML (context property name -> C++ class -> header),
  2. the methods, properties and signals each of those classes makes callable from QML,
  3. which QML files use which context objects (and the reverse),
  4. the PageEnum pages and the QML file of each,
  5. a short index of the client source folders.

Deterministic, standard library only. Usage:
    python3 -I tools/codemap/build_codemap.py          # rewrite docs/ai/CODE_MAP.md
    python3 -I tools/codemap/build_codemap.py --check  # exit 1 when the committed file is stale (CI)
"""
from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CLIENT = ROOT / "client"
OUT = ROOT / "docs" / "ai" / "CODE_MAP.md"

CORE_CPP = CLIENT / "core" / "controllers" / "coreController.cpp"
CORE_H = CLIENT / "core" / "controllers" / "coreController.h"
QML_DIR = CLIENT / "ui" / "qml"
PAGE_ENUM = CLIENT / "ui" / "utils" / "pageEnum.h"


def rel(p: Path) -> str:
    return p.relative_to(ROOT).as_posix()


def read(p: Path) -> str:
    return p.read_text(encoding="utf-8", errors="replace")


def context_properties() -> dict[str, str]:
    """QML name -> member variable, from setQmlContextProperty("Name", m_member)."""
    text = read(CORE_CPP)
    out: dict[str, str] = {}
    for name, member in re.findall(r'setQmlContextProperty\(\s*"(\w+)"\s*,\s*(\w+)\s*\)', text):
        out.setdefault(name, member)
    return out


def member_types() -> dict[str, str]:
    """Member variable -> class name, from the CoreController header."""
    text = read(CORE_H)
    out: dict[str, str] = {}
    for cls, member in re.findall(r"^\s*(?:QSharedPointer<)?(\w+)>?\s*\*?\s*(m_\w+)\s*(?:\{\})?;", text, re.M):
        out[member] = cls
    return out


def class_headers() -> dict[str, Path]:
    """Class name -> header that declares it (client/ only, first match wins)."""
    out: dict[str, Path] = {}
    for h in sorted(CLIENT.rglob("*.h")):
        if "3rd" in h.parts:
            continue
        for cls in re.findall(r"^class\s+(\w+)\s*(?:final\s*)?:\s*public", read(h), re.M):
            out.setdefault(cls, h)
    return out


SECTION_RE = re.compile(r"^\s*(public|protected|private)\s*(slots|Q_SLOTS)?\s*:|^\s*(signals|Q_SIGNALS)\s*:", re.M)


def qml_api(header: Path, cls: str) -> dict[str, list[str]]:
    """Q_PROPERTY names, Q_INVOKABLE + public slots, and signals of one class."""
    text = read(header)
    m = re.search(r"^class\s+" + re.escape(cls) + r"\b[^;{]*\{", text, re.M)
    if not m:
        return {"properties": [], "callable": [], "signals": []}
    body = text[m.end():]
    end = re.search(r"^\};", body, re.M)
    body = body[: end.start()] if end else body
    props = re.findall(r"Q_PROPERTY\(\s*[\w:<>, *&]+?\s+(\w+)\s+(?:READ|MEMBER|WRITE)", body)
    callable_, signals = [], []
    section = "private"
    for line in body.splitlines():
        s = SECTION_RE.match(line)
        if s:
            if s.group(3):
                section = "signals"
            elif s.group(2) and s.group(1) == "public":
                section = "public_slots"
            else:
                section = s.group(1)
            continue
        fn = re.match(r"\s*(?:Q_INVOKABLE\s+)?(?:virtual\s+|static\s+)?[\w:<>,*& ]+?\b(\w+)\s*\(", line)
        if not fn or line.strip().startswith(("//", "Q_PROPERTY", "Q_OBJECT", "explicit", "~")):
            continue
        name = fn.group(1)
        if name in (cls, "Q_PROPERTY"):
            continue
        if "Q_INVOKABLE" in line or section == "public_slots":
            callable_.append(name)
        elif section == "signals":
            signals.append(name)
    dedup = lambda xs: sorted(dict.fromkeys(xs))  # noqa: E731
    return {"properties": dedup(props), "callable": dedup(callable_), "signals": dedup(signals)}


def qml_usage(names: list[str]) -> dict[Path, list[str]]:
    pattern = re.compile(r"\b(" + "|".join(sorted(map(re.escape, names), key=len, reverse=True)) + r")\b")
    out: dict[Path, list[str]] = {}
    for f in sorted(QML_DIR.rglob("*.qml")):
        used = sorted(set(pattern.findall(read(f))))
        out[f] = used
    return out


def page_enum() -> list[str]:
    text = read(PAGE_ENUM)
    m = re.search(r"enum\s+(?:class\s+)?PageEnum\s*\{(.*?)\}", text, re.S)
    if not m:
        return []
    return [x.strip().split("=")[0].strip() for x in m.group(1).split(",") if x.strip() and not x.strip().startswith("//")]


def folder_index() -> list[tuple[str, int, int]]:
    rows = []
    for d in sorted(p for p in CLIENT.rglob("*") if p.is_dir()):
        if any(part in ("3rd", "3rd-prebuilt", "images", "fonts", "translations", "gradle") for part in d.relative_to(CLIENT).parts):
            continue
        depth = len(d.relative_to(CLIENT).parts)
        if depth > 3:
            continue
        files = [f for f in d.iterdir() if f.is_file()]
        code = [f for f in files if f.suffix in (".cpp", ".h", ".qml", ".kt", ".java", ".swift", ".mm", ".m", ".sh")]
        if not code:
            continue
        classes = sum(len(re.findall(r"^class\s+\w+\s*(?:final\s*)?:", read(f), re.M)) for f in code if f.suffix == ".h")
        rows.append((rel(d), len(code), classes))
    return rows


def build() -> str:
    props = context_properties()
    types = member_types()
    headers = class_headers()
    usage = qml_usage(list(props))
    reverse: dict[str, list[str]] = defaultdict(list)
    for f, used in usage.items():
        for n in used:
            reverse[n].append(f.relative_to(QML_DIR).as_posix())

    L: list[str] = []
    w = L.append
    w("# Code map (generated)")
    w("")
    w("<!-- Generated by tools/codemap/build_codemap.py. Do not edit by hand: run the script and commit the result. -->")
    w("")
    w("The QML <-> C++ wiring of the client. Read this instead of grepping QML files. For C++ type")
    w("relationships (who calls whom, clusters), use the Graphify graph (`tools/graphify/README.md`).")
    w("All objects below are registered as QML context properties in `client/core/controllers/coreController.cpp`")
    w("(`CoreController::initModels` / `initControllers`). Note that several QML names differ from the class name,")
    w("for example `ExportController` in QML is `ExportUiController` in C++.")
    w("")
    w("## 1. QML context objects")
    w("")
    w("| QML name | C++ class | Header | Used by QML files |")
    w("|---|---|---|---|")
    for name in sorted(props):
        cls = types.get(props[name], "?")
        h = headers.get(cls)
        w(f"| `{name}` | `{cls}` | {('`' + rel(h) + '`') if h else '?'} | {len(reverse.get(name, []))} |")
    w("")
    w("## 2. QML-callable API per class")
    w("")
    w("Properties (`Q_PROPERTY`), callable methods (`Q_INVOKABLE` and `public slots`) and signals. Models also expose")
    w("their roles through `roleNames()`; read the header for those.")
    w("")
    seen = set()
    for name in sorted(props):
        cls = types.get(props[name], "?")
        if cls in seen or cls not in headers:
            continue
        seen.add(cls)
        api = qml_api(headers[cls], cls)
        if not any(api.values()):
            continue
        qml_names = ", ".join(f"`{n}`" for n in sorted(props) if types.get(props[n]) == cls)
        w(f"### `{cls}` (QML: {qml_names})")
        w("")
        if api["properties"]:
            w("- Properties: " + ", ".join(api["properties"]))
        if api["callable"]:
            w("- Callable: " + ", ".join(api["callable"]))
        if api["signals"]:
            w("- Signals: " + ", ".join(api["signals"]))
        w("")
    w("## 3. QML file -> context objects it uses")
    w("")
    for group in ("Pages2", "Components", "Controls2", "Config", "Filters", "Modules", ""):
        files = [f for f in usage if (f.parent.name == group if group else f.parent == QML_DIR)]
        if not files:
            continue
        w(f"### {group or 'Root'}")
        w("")
        w("| File | Uses |")
        w("|---|---|")
        for f in files:
            used = ", ".join(usage[f]) if usage[f] else "-"
            w(f"| `{f.relative_to(QML_DIR).as_posix()}` | {used} |")
        w("")
    w("## 4. Context object -> QML files")
    w("")
    for name in sorted(reverse):
        w(f"- **{name}**: " + ", ".join(f"`{x}`" for x in sorted(reverse[name])))
    w("")
    pages = page_enum()
    if pages:
        w("## 5. Pages (`PageLoader::PageEnum` in `client/ui/utils/pageEnum.h`)")
        w("")
        w("`PageController.goToPage(PageEnum.X)` loads `client/ui/qml/Pages2/X.qml`. A page needs an enum entry,")
        w("a QML file and a line in `client/ui/qml/qml.qrc`.")
        w("")
        qml_names = {f.stem for f in (QML_DIR / "Pages2").glob("*.qml")}
        missing = [p for p in pages if p not in qml_names]
        w(", ".join(f"`{p}`" for p in pages))
        if missing:
            w("")
            w("Enum entries without a QML file: " + ", ".join(f"`{p}`" for p in missing))
        w("")
    w("## 6. Client source folders")
    w("")
    w("| Folder | Code files | Classes |")
    w("|---|---|---|")
    for d, n, c in folder_index():
        w(f"| `{d}` | {n} | {c} |")
    w("")
    return "\n".join(L)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="fail when docs/ai/CODE_MAP.md is stale")
    args = ap.parse_args()
    text = build()
    if args.check:
        current = OUT.read_text(encoding="utf-8") if OUT.exists() else ""
        if current != text:
            print("docs/ai/CODE_MAP.md is stale: run python3 -I tools/codemap/build_codemap.py and commit it.", file=sys.stderr)
            return 1
        print("docs/ai/CODE_MAP.md is current.")
        return 0
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text, encoding="utf-8")
    print(f"wrote {rel(OUT)} ({len(text.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
