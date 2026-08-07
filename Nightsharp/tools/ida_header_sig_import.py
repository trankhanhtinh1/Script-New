"""Thay sign pattern trong offset_signatures.json bang pattern tu offset.h.

offset.h la nguon chuan: cac comment `//48 8B 05 ? ? ? ? ...` la sign pattern
ban da tu verify. Script nay:
  1. Doc tung dong `constexpr X = 0x...; //<pattern>` trong offset.h.
  2. Tach cac bien the ngan boi '||', doi '?' -> '??'.
  3. Scan .text; chi nhan bien the DUY NHAT.
  4. Tu suy resolver tai vi tri match (rip_rel32 / call_rel32 / displacement /
     match / function_start) va CHI GHI khi resolve ra dung gia tri offset.h.
  5. Ghi de locators cua target tuong ung.

    IDA -> File -> Script file... -> tools/ida_header_sig_import.py
"""

from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

import ida_auto
import ida_bytes
import ida_funcs
import ida_nalt
import ida_segment
import ida_ua

SCRIPT = Path(globals().get("__file__", r"C:\Users\MR THINH\Desktop"
                            r"\Script-New\Nightsharp\tools"
                            r"\ida_header_sig_import.py")).resolve()
TOOLS = SCRIPT.parent
REPO = TOOLS.parent
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))

from tools.offset_scanner_core import (  # noqa: E402
    find_pattern_offsets,
    atomic_write_text,
)

HEADER = REPO / "core" / "offset.h"
CATALOG = TOOLS / "offset_signatures.json"

ida_auto.auto_wait()
BASE = ida_nalt.get_imagebase()

SECTIONS = {}
BLOBS = []
for _i in range(ida_segment.get_segm_qty()):
    _s = ida_segment.getnseg(_i)
    _n = ida_segment.get_segm_name(_s)
    SECTIONS[_n] = (int(_s.start_ea), int(_s.end_ea))
    if _n == ".text":
        _lo, _hi = int(_s.start_ea), int(_s.end_ea)
        BLOBS.append((_lo, bytes(ida_bytes.get_bytes(_lo, _hi - _lo))))


def scan(pattern, cap=3):
    hits = []
    for lo, data in BLOBS:
        for off in find_pattern_offsets(data, pattern, max_results=cap):
            hits.append(lo + off)
            if len(hits) >= cap:
                return hits
    return hits


def section_of(ea):
    for name, (lo, hi) in SECTIONS.items():
        if lo <= ea < hi:
            return name
    return None


def try_resolvers(match_ea, want, kind_hint):
    """Sinh (locator, ) neu resolve ra dung `want`. want la RVA hoac field."""
    # Duyet vai lenh dau tien ke tu match de tim resolver phu hop.
    for insn_off in range(0, 24):
        ea = match_ea + insn_off
        insn = ida_ua.insn_t()
        size = ida_ua.decode_insn(insn, ea)
        if size <= 0:
            continue
        raw = ida_bytes.get_bytes(ea, size)
        if raw is None:
            continue

        # 1) rip-relative -> global
        if kind_hint in ("global", "any"):
            for doff in range(1, size - 3):
                disp = struct.unpack("<i", raw[doff:doff + 4])[0]
                target = ea + size + disp
                if (target - BASE) == want and section_of(target):
                    return {
                        "instruction_offset": insn_off,
                        "resolver": {
                            "kind": "rip_rel32",
                            "displacement_offset": doff,
                            "instruction_size": size,
                        },
                        "validations": {
                            "target_sections": [section_of(target)]},
                    }
        # 2) call/jmp rel32 -> function
        if kind_hint in ("function", "any") and raw[0] in (0xE8, 0xE9):
            disp = struct.unpack("<i", raw[1:5])[0]
            target = ea + 5 + disp
            if (target - BASE) == want:
                return {
                    "instruction_offset": insn_off,
                    "resolver": {
                        "kind": "call_rel32",
                        "displacement_offset": 1,
                        "instruction_size": 5,
                    },
                    "validations": {"target_sections": [".text"]},
                }
        # 3) displacement -> field offset
        if kind_hint in ("field", "any"):
            for op in insn.ops:
                if op.type == ida_ua.o_void:
                    break
                if op.type not in (ida_ua.o_displ, ida_ua.o_phrase):
                    continue
                if (op.addr & 0xFFFFFFFF) != want:
                    continue
                for width in (4, 1):
                    if want >= (1 << (8 * width)):
                        continue
                    pos = raw.find(want.to_bytes(width, "little"))
                    if pos > 0:
                        return {
                            "instruction_offset": insn_off,
                            "resolver": {
                                "kind": "displacement",
                                "displacement_offset": pos,
                                "width": width,
                            },
                            "validations": {"min": 0, "max": 0x10000},
                        }
    # 4) match / function_start (chinh dia chi match la ket qua)
    if (match_ea - BASE) == want:
        return {"resolver": {"kind": "match"},
                "validations": {"target_sections": [".text"]}}
    f = ida_funcs.get_func(match_ea)
    if f is not None and (int(f.start_ea) - BASE) == want:
        return {"resolver": {"kind": "function_start"},
                "validations": {"target_sections": [".text"]}}
    return None


# ── Doc offset.h ──────────────────────────────────────────────────────────
DECL = re.compile(
    r"^\s*(?:inline\s+)?constexpr\s+[^=;]+?\s+"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;\s*//\s*(.+)$")
NSRE = re.compile(r"^\s*namespace\s+([A-Za-z_][A-Za-z0-9_]*)\b")
TOKOK = re.compile(r"^(?:[0-9A-Fa-f]{2}|\?\??)$")

stack, pending, rows = [], None, []
for line in HEADER.read_text(encoding="utf-8").splitlines():
    m = DECL.match(line)
    if m:
        ns = [n for n in stack if n]
        # ns da chua 'Offset' (namespace ngoai cung) -> khong them tien to.
        rows.append(("::".join(ns + [m.group(1)]),
                     int(m.group(2), 16), m.group(3).strip()))
    code = line.split("//", 1)[0]
    nm = NSRE.match(code)
    if nm:
        pending = nm.group(1)
    for ch in code:
        if ch == "{":
            stack.append(pending)
            pending = None
        elif ch == "}":
            if stack:
                stack.pop()


def variants(comment):
    out = []
    for part in comment.split("||"):
        good = []
        for t in part.strip().split():
            if TOKOK.match(t):
                good.append("??" if t.startswith("?") else t.upper())
            elif good:
                break
        fixed = sum(1 for t in good if t != "??")
        if len(good) >= 5 and fixed >= 4:
            out.append(" ".join(good))
    return out


SHA = bytes(ida_nalt.retrieve_input_file_sha256()).hex()
catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
# Don cac target sai do lan chay truoc (id bi lap 'Offset::Offset::...').
before = len(catalog["targets"])
catalog["targets"] = [t for t in catalog["targets"]
                      if not t["id"].startswith("Offset::Offset::")]
if before != len(catalog["targets"]):
    print("[NightSharp] don %d target loi tu lan chay truoc"
          % (before - len(catalog["targets"])))
by_id = {t["id"]: t for t in catalog["targets"]}

replaced = added = skipped = 0
for tid, value, comment in rows:
    tgt = by_id.get(tid)
    kind = tgt.get("target_kind", "any") if tgt else (
        "field" if value < 0x100000 else "any")
    locators = []
    for v in variants(comment):
        hits = scan(v, cap=6)
        if not hits:
            continue
        loc = try_resolvers(hits[0], value, kind)
        if not loc:
            continue
        if len(hits) > 1:
            # Nhieu match van dung duoc NEU moi match resolve ra dung cung
            # gia tri (vd cung mot lenh `lea rcx,[r15+0x3108]` lap lai).
            # Phai so bang CHINH locator nay tren tung match; neu chi kiem
            # tra "co resolver nao do khop" thi se lot pattern mo ho.
            same = True
            for h in hits[1:]:
                other = try_resolvers(h, value, kind)
                if other is None or other.get("resolver") != loc["resolver"] \
                        or other.get("instruction_offset", 0) != \
                        loc.get("instruction_offset", 0):
                    same = False
                    break
            if not same:
                continue
            loc["allow_multiple"] = True
        loc = {"pattern": v, "sections": [".text"], **loc}
        if loc.get("instruction_offset") == 0:
            loc.pop("instruction_offset")
        locators.append(loc)
    if not locators:
        skipped += 1
        continue
    if tgt is None:
        catalog["targets"].append({
            "id": tid,
            "target_kind": "field" if value < 0x100000 else "global",
            "required": True,
            "expected": {SHA: "0x%X" % value},
            "locators": locators,
            "sig_source": "offset.h",
        })
        added += 1
    else:
        tgt["locators"] = locators
        tgt.setdefault("expected", {})[SHA] = "0x%X" % value
        tgt["sig_source"] = "offset.h"
        tgt.pop("notes", None)
        tgt.pop("sig_quality", None)
        replaced += 1

atomic_write_text(CATALOG, json.dumps(catalog, indent=2,
                                      ensure_ascii=False) + "\n")
print("[NightSharp] thay %d, them %d, bo qua %d | tong target %d"
      % (replaced, added, skipped, len(catalog["targets"])))
