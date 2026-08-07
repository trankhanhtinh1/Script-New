"""Doi chieu offset.h voi offset_signatures.json. Chay bang Python he thong."""

import json
import re
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
REPO = TOOLS.parent
HEADER = REPO / "core" / "offset.h"
CATALOG = TOOLS / "offset_signatures.json"

DECL = re.compile(
    r"^\s*(?:inline\s+)?constexpr\s+[^=;]+?\s+"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^;]+);"
)
NS = re.compile(r"^\s*namespace\s+([A-Za-z_][A-Za-z0-9_]*)\b")


def strip_comment(line):
    out, i, q = [], 0, None
    while i < len(line):
        c = line[i]
        if q:
            if c == "\\":
                i += 2
                continue
            if c == q:
                q = None
            out.append(c)
            i += 1
            continue
        if line.startswith("//", i):
            break
        if c in '"\'':
            q = c
        out.append(c)
        i += 1
    return "".join(out)


stack, pending, entries = [], None, []
for raw in HEADER.read_text(encoding="utf-8").splitlines():
    code = strip_comment(raw)
    m = DECL.match(code)
    if m:
        ns = [n for n in stack if n]
        entries.append(("::".join(ns + [m.group(1)]), m.group(2).strip()))
    nm = NS.match(code)
    if nm:
        pending = nm.group(1)
    for c in code:
        if c == "{":
            stack.append(pending)
            pending = None
        elif c == "}":
            if stack:
                stack.pop()

covered = {t["id"] for t in
           json.loads(CATALOG.read_text(encoding="utf-8"))["targets"]}
HEXLIT = re.compile(r"^0x[0-9A-Fa-f]+$")

# Namespace chi chua hang so / enum / vtable slot -> khong phai field struct,
# khong doi khi game update => khong can scan.
CONST_NS = {
    "NavGridFlags", "NavGridCellLayout", "MinionClassRuntime",
    "JungleTypeRuntime", "D3D", "DrawingMatrixRuntime", "MouseInputLayout",
    "StatsRuntime", "DirectInputRuntime", "ObjectManagerRuntime",
    "BuffEntryLayout", "BuffScriptInstanceLayout",
    "SpellDataResourceNameLayout", "RuneEntryLayout", "ZoomRuntime",
}
CONST_NAMES = {
    "Offset::ControlRuntime::CastPacketOpcode",
    "Offset::SpellCastInfoEventLayout::TargetArrayEntryStride",
    "Offset::AnimationLayout::VariantEntryStride",
    "Offset::VTable::GameObjectBoundingRadius",
    "Offset::RuneManagerRuntime::GetRuneManagerVFunc",
    "Offset::All::DirectionVFunc",
    "Offset::DrawingRuntime::ScoreboardViewController",
}


def is_const(full):
    p = full.split("::")
    return (len(p) >= 2 and p[-2] in CONST_NS) or full in CONST_NAMES


rva, scan_field, const_field, alias = [], [], [], []
for full, init in entries:
    if not HEXLIT.match(init):
        alias.append(full)
    elif int(init, 16) >= 0x100000:
        rva.append(full)
    elif is_const(full):
        const_field.append(full)
    else:
        scan_field.append(full)


def report(title, items):
    miss = [f for f in items if f not in covered]
    print("%-22s tong=%-4d co_sign=%-4d thieu=%d"
          % (title, len(items), len(items) - len(miss), len(miss)))
    return miss


print("=== offset.h  <->  offset_signatures.json ===")
miss_rva = report("RVA", rva)
miss_field = report("Field CAN scan", scan_field)
print("%-22s tong=%-4d (hang so/enum, khong doi)"
      % ("Field hang so", len(const_field)))
print("%-22s tong=%-4d (alias/derived)" % ("Alias", len(alias)))

print("\n--- RVA THIEU (%d) ---" % len(miss_rva))
for f in miss_rva:
    print("  " + f)

groups = {}
for f in miss_field:
    groups.setdefault(f.split("::")[-2], []).append(f.split("::")[-1])
print("\n--- FIELD CAN SCAN theo namespace (%d field / %d nhom) ---"
      % (len(miss_field), len(groups)))
for ns in sorted(groups, key=lambda k: -len(groups[k])):
    print("  %-30s %2d" % (ns, len(groups[ns])))
