"""Run inside IDA to generate C:\lol\offset.h from signature rules.

Usage:
    IDA -> File -> Script file... -> tools/ida_offset_scanner.py

The currently opened IDB is the scan source.  Patterns may anchor at function
entries, mid-function semantic blocks, or xrefs to globals/callees.
"""

from __future__ import annotations

import hashlib
import importlib
import json
import re
import sys
import traceback
from pathlib import Path
from typing import Sequence


SCRIPT_PATH = Path(
    globals().get(
        "__file__",
        r"C:\Users\MR THINH\Desktop\Script-New\Nightsharp\tools"
        r"\ida_offset_scanner.py",
    )
).resolve()
TOOLS_DIR = SCRIPT_PATH.parent
REPOSITORY_ROOT = TOOLS_DIR.parent
if str(REPOSITORY_ROOT) not in sys.path:
    sys.path.insert(0, str(REPOSITORY_ROOT))

import tools.offset_scanner_core as _scanner_core  # noqa: E402

# IDA keeps Python modules alive between Script File invocations.  Reload the
# pure scanner core so rerunning an updated script never uses stale resolvers.
_scanner_core = importlib.reload(_scanner_core)

from tools.offset_scanner_core import (  # noqa: E402
    CatalogError,
    MemoryAdapter,
    apply_replacements,
    atomic_write_text,
    find_pattern_offsets,
    resolve_catalog,
    resolve_reflection_fields,
)


CATALOG_PATH = TOOLS_DIR / "offset_signatures.json"
TEMPLATE_PATH = REPOSITORY_ROOT / "core" / "offset.h"
OUTPUT_PATH = Path(r"C:\lol\offset.h")
REPORT_PATH = Path(r"C:\lol\offset-report.json")


class IdaMemoryAdapter(MemoryAdapter):
    def __init__(self) -> None:
        import ida_ida
        import ida_nalt
        import ida_segment

        self._ida_ida = ida_ida
        self._ida_nalt = ida_nalt
        self._ida_segment = ida_segment
        self._segments = []
        self._segment_bytes: dict[tuple[int, int], bytes] = {}
        self._pattern_cache: dict[tuple[str, tuple[str, ...]], list[int]] = {}
        self._reflection_cache: dict[
            tuple[str, tuple[str, ...], int], list[dict[str, int]]
        ] = {}
        for index in range(ida_segment.get_segm_qty()):
            segment = ida_segment.getnseg(index)
            if segment is not None:
                self._segments.append(segment)

    def find_pattern(self, pattern: str, sections: Sequence[str]) -> list[int]:
        import ida_bytes

        cache_key = (pattern, tuple(sections))
        cached = self._pattern_cache.get(cache_key)
        if cached is not None:
            return list(cached)

        allowed = set(sections)
        matches: list[int] = []
        for segment in self._segments:
            name = self._ida_segment.get_segm_name(segment)
            if name not in allowed:
                continue
            bounds = (int(segment.start_ea), int(segment.end_ea))
            segment_data = self._segment_bytes.get(bounds)
            if segment_data is None:
                segment_data = ida_bytes.get_bytes(
                    bounds[0], bounds[1] - bounds[0]
                )
                if segment_data is None:
                    raise CatalogError(
                        f"cannot read IDA segment {name!r} "
                        f"[0x{bounds[0]:X}, 0x{bounds[1]:X})"
                    )
                segment_data = bytes(segment_data)
                self._segment_bytes[bounds] = segment_data
            offsets = find_pattern_offsets(segment_data, pattern)
            matches.extend(bounds[0] + offset for offset in offsets)
        self._pattern_cache[cache_key] = matches
        return list(matches)

    def read_bytes(self, address: int, size: int) -> bytes:
        import ida_bytes

        data = ida_bytes.get_bytes(address, size)
        return b"" if data is None else bytes(data)

    def function_start(self, address: int) -> int:
        import ida_funcs

        function = ida_funcs.get_func(address)
        if function is None:
            raise CatalogError(f"no IDA function contains 0x{address:X}")
        return int(function.start_ea)

    def segment_name(self, address: int) -> str:
        segment = self._ida_segment.getseg(address)
        if segment is None:
            return ""
        return self._ida_segment.get_segm_name(segment)

    def image_bounds(self):
        return (
            int(self._ida_ida.inf_get_min_ea()),
            int(self._ida_ida.inf_get_max_ea()),
        )

    def image_base(self) -> int:
        return int(self._ida_nalt.get_imagebase())

    def find_reflection_field(
        self,
        property_name: str,
        base_registers: Sequence[str],
        max_instructions: int,
    ) -> list[dict[str, int]]:
        """Find reflected field registration calls and decode their RCX field.

        Riot's registration helpers receive `(fieldAddress, owner, name, ...)`
        in RCX/RDX/R8.  A semantic string xref therefore survives code motion
        while the LEA/MOV that defines RCX exposes the relative field value.
        """

        import ida_bytes
        import ida_funcs
        import ida_idaapi
        import ida_idp
        import ida_ua
        import idautils
        import idc

        cache_key = (
            property_name,
            tuple(register.lower() for register in base_registers),
            max_instructions,
        )
        cached = self._reflection_cache.get(cache_key)
        if cached is not None:
            return [dict(candidate) for candidate in cached]

        needle = property_name.encode("utf-8") + b"\0"
        string_addresses: list[int] = []
        for segment in self._segments:
            segment_name = self._ida_segment.get_segm_name(segment)
            if segment_name not in {".rdata", ".data"}:
                continue
            cursor = int(segment.start_ea)
            segment_end = int(segment.end_ea)
            while cursor < segment_end:
                string_address = int(
                    ida_bytes.find_bytes(
                        needle, cursor, range_end=segment_end
                    )
                )
                if string_address == ida_idaapi.BADADDR:
                    break
                string_addresses.append(string_address)
                cursor = string_address + 1

        allowed_bases = {register.lower() for register in base_registers}
        candidates: list[dict[str, int]] = []
        for string_address in string_addresses:
            for xref in idautils.XrefsTo(string_address):
                xref_address = int(xref.frm)
                function = ida_funcs.get_func(xref_address)
                if function is None:
                    continue

                string_instruction = ida_ua.insn_t()
                if ida_ua.decode_insn(
                    string_instruction, xref_address
                ) <= 0:
                    continue
                if idc.print_insn_mnem(xref_address).lower() != "lea":
                    continue
                destination = ida_idp.get_reg_name(
                    string_instruction.ops[0].reg, 8
                ).lower()
                if destination != "r8":
                    continue
                if int(string_instruction.ops[1].addr) != string_address:
                    continue

                call_address: int | None = None
                cursor_ea = xref_address
                for _ in range(max_instructions):
                    cursor_ea = int(idc.next_head(cursor_ea, function.end_ea))
                    if cursor_ea >= function.end_ea:
                        break
                    if idc.print_insn_mnem(cursor_ea).lower() == "call":
                        call_address = cursor_ea
                        break
                if call_address is None:
                    continue

                rcx_displacement: int | None = None
                rcx_base = ""
                cursor_ea = call_address
                for _ in range(max_instructions):
                    cursor_ea = int(
                        idc.prev_head(cursor_ea, function.start_ea)
                    )
                    if cursor_ea < function.start_ea:
                        break
                    instruction = ida_ua.insn_t()
                    if ida_ua.decode_insn(instruction, cursor_ea) <= 0:
                        continue
                    first_operand = instruction.ops[0]
                    if first_operand.type != ida_ua.o_reg:
                        continue
                    first_register = ida_idp.get_reg_name(
                        first_operand.reg, 8
                    ).lower()
                    if first_register != "rcx":
                        continue

                    mnemonic = idc.print_insn_mnem(cursor_ea).lower()
                    source = instruction.ops[1]
                    if mnemonic == "mov" and source.type == ida_ua.o_reg:
                        rcx_base = ida_idp.get_reg_name(
                            source.reg, 8
                        ).lower()
                        rcx_displacement = 0
                    elif mnemonic == "lea" and source.type == ida_ua.o_displ:
                        rcx_base = ida_idp.get_reg_name(
                            source.reg, 8
                        ).lower()
                        rcx_displacement = int(source.addr)
                    break

                if rcx_displacement is None:
                    continue
                if allowed_bases and rcx_base not in allowed_bases:
                    continue
                candidates.append(
                    {
                        "xref": xref_address,
                        "call": call_address,
                        "function": int(function.start_ea),
                        "displacement": rcx_displacement,
                    }
                )

        self._reflection_cache[cache_key] = [
            dict(candidate) for candidate in candidates
        ]
        return candidates


def _input_metadata() -> dict[str, object]:
    import ida_nalt
    import idc

    digest = ida_nalt.retrieve_input_file_sha256()
    sha256 = bytes(digest).hex() if digest else ""
    return {
        "idb_path": idc.get_idb_path(),
        "input_path": ida_nalt.get_input_file_path(),
        "root_filename": ida_nalt.get_root_filename(),
        "input_size": int(ida_nalt.retrieve_input_file_size()),
        "image_base": f"0x{int(ida_nalt.get_imagebase()):X}",
        "sha256": sha256,
    }


def _validate_fixture(
    catalog: dict, metadata: dict[str, object], results: dict[str, int]
) -> str:
    sha256 = str(metadata["sha256"]).lower()
    fixture = catalog.get("fixtures", {}).get(sha256)
    if fixture is None:
        return "unknown"

    expected_by_id = {
        target["id"]: target.get("expected", {}).get(sha256)
        for target in catalog.get("targets", [])
        if target.get("mode", "scan") in {"scan", "reflection_field"}
    }
    missing = sorted(
        target_id
        for target_id in results
        if expected_by_id.get(target_id) is None
    )
    if missing:
        raise CatalogError(
            "known fixture lacks expected RVA for: " + ", ".join(missing)
        )

    mismatches = []
    for target_id, actual in results.items():
        expected = int(str(expected_by_id[target_id]), 0)
        if actual != expected:
            mismatches.append(
                f"{target_id}: expected 0x{expected:X}, got 0x{actual:X}"
            )
    if mismatches:
        raise CatalogError("fixture mismatch:\n  " + "\n  ".join(mismatches))
    return str(fixture.get("version", fixture.get("label", "known")))


def _render_source_metadata(
    template: str, version: str, sha256: str
) -> str:
    header, separator, body = template.partition("namespace Offset")
    if not separator:
        raise CatalogError("template is missing namespace Offset")
    header, version_count = re.subn(
        r"(?<=League of Legends )[0-9]+(?:\.[0-9]+)+",
        version,
        header,
        count=1,
    )
    header, sha_count = re.subn(
        r"\b[0-9A-Fa-f]{64}\b",
        sha256.lower(),
        header,
        count=1,
    )
    if version_count != 1 or sha_count != 1:
        raise CatalogError(
            "template source banner does not contain one version and SHA-256"
        )
    return header + separator + body


def run() -> dict[str, object]:
    import ida_auto

    ida_auto.auto_wait()
    catalog = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    template = TEMPLATE_PATH.read_bytes().decode("utf-8")
    metadata = _input_metadata()
    adapter = IdaMemoryAdapter()

    rva_results, rva_report = resolve_catalog(
        adapter, catalog, fixture_sha256=str(metadata["sha256"]).lower()
    )
    field_results, field_report = resolve_reflection_fields(
        adapter, catalog, fixture_sha256=str(metadata["sha256"]).lower()
    )
    overlap = sorted(set(rva_results) & set(field_results))
    if overlap:
        raise CatalogError(
            "target(s) resolved by both RVA and field scanners: "
            + ", ".join(overlap)
        )
    results = {**rva_results, **field_results}
    target_report = rva_report + field_report
    version = _validate_fixture(catalog, metadata, results)
    rendered = apply_replacements(template, results)
    rendered = _render_source_metadata(
        rendered, version, str(metadata["sha256"])
    )
    rendered_sha256 = hashlib.sha256(rendered.encode("utf-8")).hexdigest()

    report: dict[str, object] = {
        "status": "ok",
        "fixture_version": version,
        "input": metadata,
        "catalog": str(CATALOG_PATH),
        "template": str(TEMPLATE_PATH),
        "output": str(OUTPUT_PATH),
        "resolved_count": len(results),
        "output_sha256": rendered_sha256,
        "targets": target_report,
    }

    # Scan and fixture validation are complete before either artifact changes.
    # Publish the report only after the header succeeds, so an "ok" report
    # never describes an output file that failed to replace.
    atomic_write_text(OUTPUT_PATH, rendered)
    atomic_write_text(
        REPORT_PATH, json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    )
    print(
        f"[NightSharp] resolved {len(results)} target(s) for {version}; "
        f"wrote {OUTPUT_PATH}"
    )
    return report


if __name__ == "__main__":
    try:
        run()
    except Exception as error:
        print(f"[NightSharp] offset scan failed: {error}")
        traceback.print_exc()
        raise
