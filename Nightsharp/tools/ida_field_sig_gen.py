"""Sinh signature cho FIELD OFFSET va ghi thang vao offset_signatures.json.

Chay MOT LAN tren IDB cua ban build HIEN TAI (build da biet offset dung).
Voi moi field trong offset.h, script:
  1. Tim cac lenh trong .text co displacement == gia tri offset hien tai.
  2. Uu tien lenh nam trong cac ham "neo" da biet (SCOPE_FUNCS) -> dung ngu nghia.
  3. Sinh pattern duy nhat quanh lenh do, wildcard chinh displacement.
  4. Ghi target kieu "field" vao offset_signatures.json.

Sau do, khi game update: chi can chay ida_offset_scanner.py, no doc lai
displacement tu lenh -> ra offset moi tu dong.

    IDA -> File -> Script file... -> tools/ida_field_sig_gen.py
"""

from __future__ import annotations

import json
import re
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
                            r"\ida_field_sig_gen.py")).resolve()
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

# Namespace chi chua hang so/enum -> khong scan.
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

# Ham "neo" cho tung namespace: uu tien lay lenh nam trong nhung ham nay
# vi chung chac chan thao tac dung loai object -> signature co ngu nghia.
SCOPE = {
    "SpellSlotLayout": [0x974680, 0x96A2B0, 0x90E4D0, 0x985400, 0xBE7460,
                        0xBD41C0, 0x9835E0, 0x981D20],
    "SpellCastInfoLayout": [0x27B340, 0x985400, 0x985CB0, 0x297510,
                            0x2A0250, 0x989C30, 0xBD41C0],
    "SpellCastInfoEventLayout": [0x989920, 0x981D20, 0x97B700, 0x9835E0,
                                 0x985400, 0x297510],
    "SpellInputLayout": [0x985400, 0x985CB0, 0x970350, 0x96A2B0],
    "SpellInfoLayout": [0x96A2B0, 0x985400, 0x9835E0],
    "SpellDataLayout": [0x96A2B0, 0x9121E0, 0x985400],
    "SpellDataResourceLayout": [0x9121E0, 0x90E4D0, 0x97B700, 0x970350],
    "SpellBookLayout": [0x974680, 0xBD1220, 0xBD1310],
    "AiManager": [0x283190, 0x5715A0, 0x562AD0],
    "SpellRuntime": [0x280BB0, 0xBD1220, 0xBC7BD0],
    "ProcessCastSpellRequestLayout": [0x297510],

    # ── Object / unit / champion ─────────────────────────────────────────
    "All": [0x571110, 0x2942D0, 0x2B6690, 0x27BFB0, 0x562AD0, 0x5644C0,
            0x565B00, 0x294EF0, 0x29F860, 0x29EEC0],
    "AttackableUnit": [0x2B6690, 0x29F860, 0x213450, 0x30E950, 0x314820],
    "AIHeroClient": [0x57D620, 0x57D520, 0x213450, 0x294EF0, 0x29F860,
                     0x2B6690, 0x314820],
    # ── Missile ──────────────────────────────────────────────────────────
    "MissileClient": [0x97B700, 0x9633A0, 0x981D20],
    "MissileEventLayout": [0x97B700, 0x9633A0, 0x981D20],
    # ── Buff ─────────────────────────────────────────────────────────────
    "BuffManagerRuntime": [0x21FFF0, 0x21F220],
    "BuffManagerLayout": [0x21FFF0, 0x21F220],
    "BuffDataLayout": [0x21FFF0, 0x21F220],
    "BuffEventLayout": [0x21FFF0, 0x21F220],
    # ── Hud / drawing / minimap ──────────────────────────────────────────
    "HudRuntime": [0xBD41C0, 0xBFAEB0, 0xBCF800, 0xBE5AF0],
    "HudCursorTargetLogicLayout": [0xBE5AF0, 0xBCF800],
    "HudSpellTargetingLayout": [0xBD41C0, 0xBFAEB0],
    "HudInputLayout": [0xBD41C0, 0xBFAEB0],
    "TacticalMapLayout": [0x133A050, 0x1214240, 0x1214450],
    "DrawingRuntime": [0x133A050],
    "ChatViewControllerLayout": [0xB86450],
    # ── NavGrid ──────────────────────────────────────────────────────────
    "NavGridLayout": [0x1288340],
    # ── Rune ─────────────────────────────────────────────────────────────
    "RuneManagerLayout": [0x70EE80, 0x711C60, 0x716180],
    "RuneDataLayout": [0x716180, 0x70EE80],
    "RuneTreeDataLayout": [0x716260, 0x711C60],
    # ── Item / animation / skin / mission ────────────────────────────────
    "ItemRuntime": [0x212840, 0x22D590, 0x294EF0],
    "AnimationLayout": [0xE44570, 0x29A350, 0x212840],
    "SkinRuntime": [0x212840, 0x22D590],
    "MissionInfo": [0x282B60],
}

ida_auto.auto_wait()
BASE = ida_nalt.get_imagebase()

TEXT = []
for _i in range(ida_segment.get_segm_qty()):
    _s = ida_segment.getnseg(_i)
    if ida_segment.get_segm_name(_s) == ".text":
        _lo, _hi = int(_s.start_ea), int(_s.end_ea)
        TEXT.append((_lo, bytes(ida_bytes.get_bytes(_lo, _hi - _lo))))


def count_matches(pattern, cap=3):
    total = 0
    for _lo, data in TEXT:
        total += len(find_pattern_offsets(data, pattern, max_results=cap))
        if total >= cap:
            return total
    return total


def insn_displacements(ea):
    """(displacement, byte-offset-trong-lenh, do-rong) cho lenh tai ea."""
    insn = ida_ua.insn_t()
    size = ida_ua.decode_insn(insn, ea)
    if size <= 0:
        return []
    raw = ida_bytes.get_bytes(ea, size)
    if raw is None:
        return []
    out = []
    for op in insn.ops:
        if op.type == ida_ua.o_void:
            break
        if op.type not in (ida_ua.o_displ, ida_ua.o_phrase):
            continue
        value = op.addr & 0xFFFFFFFF
        # Tim vi tri byte cua displacement trong lenh (little endian).
        for width in (4, 1):
            if value >= (1 << (8 * width)):
                continue
            needle = value.to_bytes(width, "little")
            pos = raw.find(needle)
            if pos > 0:
                out.append((value, pos, width, size))
                break
    return out


def scan_for_value(value, scope_funcs):
    """Tra ve list (ea, disp_off, width, size) co displacement == value."""
    hits = []
    for frva in scope_funcs:
        f = ida_funcs.get_func(BASE + frva)
        if f is None:
            continue
        ea, end = int(f.start_ea), int(f.end_ea)
        while ea < end:
            insn = ida_ua.insn_t()
            size = ida_ua.decode_insn(insn, ea)
            if size <= 0:
                ea += 1
                continue
            for v, off, width, isize in insn_displacements(ea):
                if v == value:
                    hits.append((ea, off, width, isize))
            ea += size
    return hits


def build_pattern(ea, disp_off, width, size, max_span=40):
    """Pattern duy nhat, wildcard displacement. Uu tien mo rong trong ham."""
    func = ida_funcs.get_func(ea)
    lo = int(func.start_ea) if func else ea
    hi = int(func.end_ea) if func else ea + size

    def window(start, end, dindex):
        raw = ida_bytes.get_bytes(start, end - start)
        if raw is None:
            return None
        toks = ["%02X" % b for b in raw]
        for k in range(dindex, dindex + width):
            if 0 <= k < len(toks):
                toks[k] = "??"
        return " ".join(toks)

    # Mo rong ve sau (trong ham) truoc: giu ngu canh cua chinh accessor.
    for back in range(0, max_span):
        start = ea - back
        if start < lo:
            break
        end = min(ea + size, hi)
        pat = window(start, end, back + disp_off)
        if pat and count_matches(pat) == 1:
            return pat, back + disp_off, "in-func"
    # Roi mo rong ve truoc, van trong ham.
    for tail in range(0, max_span):
        end = ea + size + tail
        if end > hi:
            break
        pat = window(ea, end, disp_off)
        if pat and count_matches(pat) == 1:
            return pat, disp_off, "in-func"
    # Cuoi cung: cho phep tran qua bien ham (de vo hon khi client recompile).
    for tail in range(0, max_span):
        pat = window(ea, ea + size + tail, disp_off)
        if pat and count_matches(pat) == 1:
            return pat, disp_off, "spill"
    return None, 0, None


# ── Doc offset.h ──────────────────────────────────────────────────────────
DECL = re.compile(
    r"^\s*(?:inline\s+)?constexpr\s+[^=;]+?\s+"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^;]+);")
NSRE = re.compile(r"^\s*namespace\s+([A-Za-z_][A-Za-z0-9_]*)\b")
HEXLIT = re.compile(r"^0x[0-9A-Fa-f]+$")


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


stack, pending, fields = [], None, []
for raw_line in HEADER.read_text(encoding="utf-8").splitlines():
    code = strip_comment(raw_line)
    m = DECL.match(code)
    if m:
        ns = [n for n in stack if n]
        full = "::".join(ns + [m.group(1)])
        init = m.group(2).strip()
        if HEXLIT.match(init):
            val = int(init, 16)
            leaf = ns[-1] if ns else ""
            if (val < 0x100000 and leaf not in CONST_NS
                    and full not in CONST_NAMES):
                fields.append((full, leaf, val))
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

digest = ida_nalt.retrieve_input_file_sha256()
SHA = bytes(digest).hex() if digest else ""

catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
existing = {t["id"]: t for t in catalog["targets"]}

added = skipped = 0
stats = {"in-func": 0, "spill": 0}
for full, leaf, value in fields:
    if full in existing:
        continue
    hits = scan_for_value(value, SCOPE.get(leaf, []))
    chosen = None
    for ea, doff, width, size in hits:
        pat, real_doff, quality = build_pattern(ea, doff, width, size)
        if pat:
            chosen = (pat, real_doff, width, quality)
            break
    if not chosen and value >= 0x80:
        # Khong co trong ham neo -> tim toan .text bang chinh gia tri
        # displacement (4 byte LE). Chi nhan khi sinh duoc pattern DUY NHAT,
        # nen van an toan; danh dau 'wide' de biet la kem tin cay hon.
        needle = value.to_bytes(4, "little")
        tried = 0
        for _lo, data in TEXT:
            pos = 0
            while tried < 40:
                pos = data.find(needle, pos)
                if pos < 0:
                    break
                for back in range(2, 9):
                    ea = _lo + pos - back
                    for v, doff, width, size in insn_displacements(ea):
                        if v != value:
                            continue
                        tried += 1
                        pat, real_doff, q = build_pattern(
                            ea, doff, width, size)
                        if pat:
                            chosen = (pat, real_doff, width,
                                      "wide" if q == "in-func" else "spill")
                            break
                    if chosen:
                        break
                if chosen:
                    break
                pos += 1
            if chosen:
                break
    if not chosen:
        skipped += 1
        continue
    pat, real_doff, width, quality = chosen
    stats[quality] = stats.get(quality, 0) + 1
    entry = {
        "id": full,
        "target_kind": "field",
        "required": True,
        "sig_quality": quality,
        "expected": {SHA: "0x%X" % value},
    }
    if quality != "in-func":
        entry["notes"] = (
            "Pattern duy nhat nhung KHONG neo trong ham ngu nghia; "
            "kiem tra lai neu build moi bao sai lech."
        )
    catalog["targets"].append({
        **entry,
        "locators": [{
            "pattern": pat,
            "sections": [".text"],
            "resolver": {
                "kind": "displacement",
                "displacement_offset": real_doff,
                "width": width,
            },
            "validations": {"min": 0, "max": 0x10000},
        }],
    })
    added += 1

atomic_write_text(CATALOG, json.dumps(catalog, indent=2,
                                      ensure_ascii=False) + "\n")
print("[NightSharp] them %d field, bo qua %d, tong target %d"
      % (added, skipped, len(catalog["targets"])))
print("[NightSharp] chat luong pattern:", stats)
