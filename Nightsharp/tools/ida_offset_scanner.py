"""Run inside IDA to generate C:\lol\offset.h from signature rules.

Usage:
    IDA -> File -> Script file... -> tools/ida_offset_scanner.py

The currently opened IDB is the scan source.  Patterns may anchor at function
entries, mid-function semantic blocks, or xrefs to globals/callees.
"""

from __future__ import annotations

import hashlib
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

from tools.offset_scanner_core import (  # noqa: E402
    CatalogError,
    MemoryAdapter,
    apply_replacements,
    atomic_write_text,
    find_pattern_offsets,
    resolve_catalog,
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
            remaining = 2 - len(matches)
            offsets = find_pattern_offsets(
                segment_data, pattern, max_results=remaining
            )
            matches.extend(bounds[0] + offset for offset in offsets)
            if len(matches) > 1:
                self._pattern_cache[cache_key] = matches
                return list(matches)
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

    def function_bounds(self, address: int) -> tuple[int, int]:
        import ida_funcs

        function = ida_funcs.get_func(address)
        if function is None:
            raise CatalogError(f"no IDA function contains 0x{address:X}")
        return (int(function.start_ea), int(function.end_ea))

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
        if target.get("mode", "scan") == "scan"
    }
    missing = sorted(
        target_id
        for target_id in results
        if expected_by_id.get(target_id) is None
    )
    if missing:
        raise CatalogError(
            "known fixture lacks expected value for: " + ", ".join(missing)
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

    results, target_report = resolve_catalog(
        adapter, catalog, fixture_sha256=str(metadata["sha256"]).lower()
    )
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
        f"[NightSharp] resolved {len(results)} offset(s) for {version}; "
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
