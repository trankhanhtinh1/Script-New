"""Pure-Python core used by the IDAPython offset scanner.

This module deliberately has no IDA imports so its pattern, resolver, catalog,
and template behavior can be tested with the system Python interpreter.
"""

from __future__ import annotations

import re
import struct
import tempfile
from abc import ABC, abstractmethod
from os import replace
from pathlib import Path
from typing import Any, Dict, Iterable, Mapping, Sequence, Tuple


class CatalogError(RuntimeError):
    """Raised when a signature catalog cannot be resolved safely."""


class MemoryAdapter(ABC):
    """Small interface implemented by IDA and by unit-test fixtures."""

    @abstractmethod
    def find_pattern(self, pattern: str, sections: Sequence[str]) -> list[int]:
        raise NotImplementedError

    @abstractmethod
    def read_bytes(self, address: int, size: int) -> bytes:
        raise NotImplementedError

    @abstractmethod
    def function_start(self, address: int) -> int:
        raise NotImplementedError

    @abstractmethod
    def segment_name(self, address: int) -> str:
        raise NotImplementedError

    @abstractmethod
    def image_bounds(self) -> Tuple[int, int]:
        raise NotImplementedError

    @abstractmethod
    def image_base(self) -> int:
        raise NotImplementedError

    def function_bounds(self, address: int) -> Tuple[int, int]:
        """Return [start, end) of the function containing ``address``."""

        raise NotImplementedError


def compile_pattern(pattern: str) -> tuple[bytes, tuple[bool, ...]]:
    """Convert an IDA-style byte pattern to bytes plus a fixed-byte mask."""

    tokens = pattern.strip().split()
    if not tokens:
        raise CatalogError("empty byte pattern")

    values = bytearray()
    mask: list[bool] = []
    for token in tokens:
        if token in {"?", "??"}:
            values.append(0)
            mask.append(False)
            continue
        if not re.fullmatch(r"[0-9A-Fa-f]{2}", token):
            raise CatalogError(f"malformed pattern token: {token!r}")
        values.append(int(token, 16))
        mask.append(True)
    return bytes(values), tuple(mask)


def find_pattern_offsets(
    data: bytes, pattern: str, max_results: int | None = None
) -> list[int]:
    """Find IDA-style patterns in a byte buffer, including overlaps.

    The longest fixed run is used as an anchor, so large IDA segments can be
    searched without depending on version-specific IDA search APIs.
    """

    needle, mask = compile_pattern(pattern)
    fixed_runs: list[tuple[int, int]] = []
    run_start = None
    for index, is_fixed in enumerate(mask + (False,)):
        if is_fixed and run_start is None:
            run_start = index
        elif not is_fixed and run_start is not None:
            fixed_runs.append((run_start, index))
            run_start = None
    if not fixed_runs:
        raise CatalogError("pattern must contain at least one fixed byte")

    anchor_start, anchor_end = max(
        fixed_runs, key=lambda run: run[1] - run[0]
    )
    anchor = needle[anchor_start:anchor_end]
    matches: list[int] = []
    search_cursor = 0
    while True:
        anchor_position = data.find(anchor, search_cursor)
        if anchor_position < 0:
            break
        candidate = anchor_position - anchor_start
        valid = candidate >= 0 and candidate + len(needle) <= len(data)
        if valid:
            for fixed_start, fixed_end in fixed_runs:
                if (
                    data[
                        candidate + fixed_start : candidate + fixed_end
                    ]
                    != needle[fixed_start:fixed_end]
                ):
                    valid = False
                    break
        if valid:
            matches.append(candidate)
            if max_results is not None and len(matches) >= max_results:
                break
        search_cursor = anchor_position + 1
    return matches


def _read_exact(adapter: MemoryAdapter, address: int, size: int) -> bytes:
    data = adapter.read_bytes(address, size)
    if data is None or len(data) != size:
        raise CatalogError(
            f"cannot read {size} byte(s) at 0x{address:X}: "
            f"received {0 if data is None else len(data)}"
        )
    return data


def _relative_target(
    adapter: MemoryAdapter,
    instruction_address: int,
    displacement_offset: int,
    instruction_size: int,
    addend: int,
) -> int:
    displacement_bytes = _read_exact(
        adapter, instruction_address + displacement_offset, 4
    )
    displacement = struct.unpack("<i", displacement_bytes)[0]
    return instruction_address + instruction_size + displacement + addend


def _read_displacement(
    adapter: MemoryAdapter,
    instruction_address: int,
    displacement_offset: int,
    width: int,
    signed: bool,
) -> int:
    if width not in {1, 2, 4}:
        raise CatalogError(f"unsupported displacement width: {width}")
    raw = _read_exact(
        adapter, instruction_address + displacement_offset, width
    )
    formats = {1: "b" if signed else "B", 2: "h" if signed else "H",
               4: "i" if signed else "I"}
    return struct.unpack("<" + formats[width], raw)[0]


def _validate_field(
    target_id: str,
    resolved: int,
    validations: Mapping[str, Any],
) -> None:
    """Validate a struct field displacement (not an address)."""

    minimum = int(validations.get("min", 0))
    maximum = int(validations.get("max", 0x10000))
    if not minimum <= resolved <= maximum:
        raise CatalogError(
            f"{target_id}: field offset 0x{resolved:X} outside plausible "
            f"range [0x{minimum:X}, 0x{maximum:X}]"
        )

    alignment = int(validations.get("alignment", 1))
    if alignment > 1 and resolved % alignment:
        raise CatalogError(
            f"{target_id}: field offset 0x{resolved:X} is not "
            f"{alignment}-byte aligned"
        )


def _validate_target(
    adapter: MemoryAdapter,
    target_id: str,
    resolved: int,
    validations: Mapping[str, Any],
) -> None:
    image_start, image_end = adapter.image_bounds()
    if not image_start <= resolved < image_end:
        raise CatalogError(
            f"{target_id}: resolved RVA 0x{resolved:X} is outside "
            f"image [0x{image_start:X}, 0x{image_end:X})"
        )

    expected_sections = validations.get("target_sections")
    if expected_sections:
        actual_section = adapter.segment_name(resolved)
        if actual_section not in expected_sections:
            raise CatalogError(
                f"{target_id}: target 0x{resolved:X} is in {actual_section!r}; "
                f"expected one of {expected_sections!r}"
            )

    alignment = int(validations.get("alignment", 1))
    if alignment > 1 and resolved % alignment:
        raise CatalogError(
            f"{target_id}: target 0x{resolved:X} is not {alignment}-byte aligned"
        )

    fingerprint = validations.get("target_pattern")
    if fingerprint:
        expected, mask = compile_pattern(fingerprint)
        actual = _read_exact(adapter, resolved, len(expected))
        if any(
            is_fixed and actual[index] != expected[index]
            for index, is_fixed in enumerate(mask)
        ):
            raise CatalogError(
                f"{target_id}: target fingerprint failed at 0x{resolved:X}"
            )


def _resolve_locator(
    adapter: MemoryAdapter,
    target_id: str,
    locator: Mapping[str, Any],
    match_address: int,
) -> int:
    instruction_address = match_address + int(locator.get("instruction_offset", 0))
    resolver = locator.get("resolver", {})
    kind = resolver.get("kind")
    addend = int(resolver.get("addend", 0))

    if kind == "displacement":
        # Struct field offset: the value IS the instruction displacement,
        # not an address. Never run address validations against it.
        resolved = _read_displacement(
            adapter,
            instruction_address,
            int(resolver["displacement_offset"]),
            int(resolver.get("width", 4)),
            bool(resolver.get("signed", False)),
        ) + addend
        _validate_field(
            target_id, resolved, locator.get("validations", {})
        )
        return resolved

    if kind == "match":
        resolved = instruction_address + addend
    elif kind == "function_start":
        resolved = adapter.function_start(instruction_address) + addend
    elif kind in {"rip_rel32", "call_rel32", "call_rel32_follow_jmp"}:
        displacement_offset = int(resolver["displacement_offset"])
        instruction_size = int(resolver["instruction_size"])
        if kind in {"call_rel32", "call_rel32_follow_jmp"}:
            # 0xE8 = CALL rel32, 0xE9 = JMP rel32 (tail call). Both encode the
            # target the same way, and tail calls are a legitimate way to
            # locate a function, so accept either.
            opcode = _read_exact(adapter, instruction_address, 1)[0]
            if opcode not in (0xE8, 0xE9):
                raise CatalogError(
                    f"{target_id}: expected CALL/JMP rel32 at "
                    f"0x{instruction_address:X}, found opcode 0x{opcode:02X}"
                )
        resolved = _relative_target(
            adapter,
            instruction_address,
            displacement_offset,
            instruction_size,
            addend,
        )
        if kind == "call_rel32_follow_jmp":
            # The CALL target may be a thin wrapper (e.g. `add rcx, X; jmp target`).
            # Follow the leading JMP rel32 (opcode 0xE9) at the resolved address
            # to reach the real function. Stop on the first non-JMP instruction
            # or after max_follow_depth hops.
            max_depth = int(resolver.get("max_follow_depth", 8))
            for _ in range(max_depth):
                head = _read_exact(adapter, resolved, 1)
                if not head or head[0] != 0xE9:
                    break
                resolved = _relative_target(
                    adapter, resolved, 1, 5, 0
                )
    else:
        raise CatalogError(f"{target_id}: unsupported resolver kind {kind!r}")

    _validate_target(
        adapter, target_id, resolved, locator.get("validations", {})
    )
    return resolved


def _resolve_anchor_bounds(
    adapter: MemoryAdapter,
    target_id: str,
    anchor: Mapping[str, Any],
) -> Tuple[int, int]:
    """Locate the function that an anchored field pattern is scoped to.

    Anchoring is what keeps short field patterns (a single mov with a
    displacement) unambiguous: the same instruction shape occurs all over
    .text, but is unique inside the one accessor function that owns it.
    """

    pattern = anchor.get("pattern")
    if not pattern:
        raise CatalogError(f"{target_id}: anchor requires a pattern")
    compile_pattern(pattern)
    matches = adapter.find_pattern(pattern, anchor.get("sections", [".text"]))
    if not matches:
        raise CatalogError(f"{target_id}: anchor pattern not found")
    if len(matches) != 1:
        raise CatalogError(
            f"{target_id}: anchor pattern is ambiguous ({len(matches)} matches)"
        )
    return adapter.function_bounds(
        matches[0] + int(anchor.get("instruction_offset", 0))
    )


def _find_in_bounds(
    adapter: MemoryAdapter,
    pattern: str,
    bounds: Tuple[int, int],
) -> list[int]:
    start, end = bounds
    data = _read_exact(adapter, start, end - start)
    return [start + offset for offset in find_pattern_offsets(data, pattern)]


def resolve_catalog(
    adapter: MemoryAdapter,
    catalog: Mapping[str, Any],
    fixture_sha256: str | None = None,
) -> tuple[dict[str, int], list[dict[str, Any]]]:
    """Resolve all scan targets, failing on missing or ambiguous signatures.

    Locators are alternatives across builds. Every locator that matches the
    current IDB must have exactly one match, and all successful locators for a
    target must agree on the same RVA.
    """

    results: dict[str, int] = {}
    report: list[dict[str, Any]] = []
    image_base = adapter.image_base()

    for target in catalog.get("targets", []):
        target_id = target.get("id")
        if not target_id:
            raise CatalogError("catalog target is missing id")
        if target_id in results:
            raise CatalogError(f"duplicate catalog target id: {target_id}")

        mode = target.get("mode", "scan")
        if mode != "scan":
            continue

        is_field = target.get("target_kind") == "field"

        resolved_candidates: list[tuple[int, int, int]] = []
        locator_report: list[dict[str, Any]] = []
        for locator_index, locator in enumerate(target.get("locators", [])):
            pattern = locator.get("pattern", "")
            compile_pattern(pattern)
            fixture_allowlist = locator.get("fixtures")
            if fixture_allowlist and fixture_sha256 not in fixture_allowlist:
                locator_report.append(
                    {
                        "locator": locator_index,
                        "pattern": pattern,
                        "matches": [],
                        "status": "skipped-fixture",
                    }
                )
                continue
            anchor = locator.get("anchor")
            if anchor:
                bounds = _resolve_anchor_bounds(adapter, target_id, anchor)
                matches = _find_in_bounds(adapter, pattern, bounds)
            else:
                matches = adapter.find_pattern(
                    pattern, locator.get("sections", [".text"])
                )
            current_report: dict[str, Any] = {
                "locator": locator_index,
                "pattern": pattern,
                "matches": [
                    f"0x{address - image_base:X}" for address in matches
                ],
            }
            locator_report.append(current_report)

            if not matches:
                current_report["status"] = "not-found"
                continue
            if len(matches) != 1:
                # allow_multiple: the pattern legitimately repeats (e.g. the
                # same `lea rcx,[r15+0x3108]` emitted at several call sites).
                # Still refuse unless EVERY match resolves to the same value,
                # so a genuinely ambiguous pattern can never slip through.
                if not locator.get("allow_multiple"):
                    current_report["status"] = "ambiguous"
                    raise CatalogError(
                        f"{target_id}: locator {locator_index} is ambiguous "
                        f"({len(matches)} matches)"
                    )
                values = {
                    _resolve_locator(adapter, target_id, locator, match)
                    for match in matches
                }
                if len(values) != 1:
                    current_report["status"] = "ambiguous"
                    formatted = ", ".join(f"0x{v:X}" for v in sorted(values))
                    raise CatalogError(
                        f"{target_id}: locator {locator_index} has "
                        f"{len(matches)} matches resolving to different "
                        f"values ({formatted})"
                    )

            resolved = _resolve_locator(
                adapter, target_id, locator, matches[0]
            )
            resolved_candidates.append((resolved, locator_index, matches[0]))
            current_report["status"] = "resolved"
            if is_field:
                current_report["resolved_value"] = f"0x{resolved:X}"
            else:
                current_report["resolved_rva"] = (
                    f"0x{resolved - image_base:X}"
                )

        if not resolved_candidates:
            if target.get("required", True):
                raise CatalogError(f"{target_id}: no locator matched")
            continue

        distinct = {candidate[0] for candidate in resolved_candidates}
        if len(distinct) != 1:
            formatted = ", ".join(f"0x{value:X}" for value in sorted(distinct))
            raise CatalogError(
                f"{target_id}: matching locators disagree ({formatted})"
            )

        resolved_value = resolved_candidates[0][0]
        # Field offsets are struct displacements and are already final;
        # only addresses get rebased into RVAs.
        final_value = (
            resolved_value if is_field else resolved_value - image_base
        )
        results[target_id] = final_value
        report.append(
            {
                "id": target_id,
                ("resolved_value" if is_field else "resolved_rva"):
                    f"0x{final_value:X}",
                "locators": locator_report,
            }
        )

    return results, report


_DECLARATION_RE = re.compile(
    r"^(?P<prefix>\s*(?:inline\s+)?constexpr\s+[^=;]+?\s+"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*)"
    r"(?P<initializer>[^;]+)(?P<suffix>;.*)$"
)
_NAMESPACE_RE = re.compile(
    r"^\s*namespace\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\b"
)


def _strip_comments_for_braces(line: str, in_block_comment: bool) -> tuple[str, bool]:
    output: list[str] = []
    index = 0
    quote: str | None = None

    while index < len(line):
        if in_block_comment:
            end = line.find("*/", index)
            if end < 0:
                return "".join(output), True
            index = end + 2
            in_block_comment = False
            continue

        if quote:
            char = line[index]
            if char == "\\":
                index += 2
                continue
            if char == quote:
                quote = None
            index += 1
            continue

        if line.startswith("//", index):
            break
        if line.startswith("/*", index):
            in_block_comment = True
            index += 2
            continue
        char = line[index]
        if char in {'"', "'"}:
            quote = char
            index += 1
            continue
        output.append(char)
        index += 1

    return "".join(output), in_block_comment


def apply_replacements(template: str, replacements: Mapping[str, int]) -> str:
    """Replace selected constexpr initializer spans and preserve all other text."""

    remaining = set(replacements)
    scope_stack: list[str | None] = []
    in_block_comment = False
    pending_namespace: str | None = None
    replaced: set[str] = set()
    output: list[str] = []

    for original_line in template.splitlines(keepends=True):
        line_ending = ""
        body = original_line
        if body.endswith("\r\n"):
            body, line_ending = body[:-2], "\r\n"
        elif body.endswith("\n"):
            body, line_ending = body[:-1], "\n"

        code, in_block_comment = _strip_comments_for_braces(
            body, in_block_comment
        )
        active_namespaces = [name for name in scope_stack if name is not None]
        declaration = _DECLARATION_RE.match(body)
        active_declaration = _DECLARATION_RE.match(code)
        if declaration and active_declaration:
            target_id = "::".join(
                active_namespaces + [declaration.group("name")]
            )
            if target_id in replacements:
                if target_id in replaced:
                    raise CatalogError(
                        f"duplicate declaration for replacement target: "
                        f"{target_id}"
                    )
                body = (
                    declaration.group("prefix")
                    + f"0x{int(replacements[target_id]):X}"
                    + declaration.group("suffix")
                )
                remaining.remove(target_id)
                replaced.add(target_id)

        namespace_match = _NAMESPACE_RE.match(code)
        if namespace_match:
            if pending_namespace is not None:
                raise CatalogError(
                    f"namespace {pending_namespace!r} has no opening brace"
                )
            pending_namespace = namespace_match.group("name")
        for char in code:
            if char == "{":
                if pending_namespace is not None:
                    scope_stack.append(pending_namespace)
                    pending_namespace = None
                else:
                    scope_stack.append(None)
            elif char == "}":
                if not scope_stack:
                    raise CatalogError("template has an unmatched closing brace")
                scope_stack.pop()

        output.append(body + line_ending)

    if pending_namespace is not None:
        raise CatalogError(
            f"namespace {pending_namespace!r} has no opening brace"
        )
    if scope_stack:
        raise CatalogError(
            f"template has {len(scope_stack)} unclosed brace scope(s)"
        )
    if remaining:
        missing = ", ".join(sorted(remaining))
        raise CatalogError(f"replacement target(s) not found in template: {missing}")
    return "".join(output)


def atomic_write_text(
    path: str | Path, text: str, encoding: str = "utf-8"
) -> None:
    """Write a complete text artifact and atomically replace the destination."""

    destination = Path(path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding=encoding,
            newline="",
            prefix=f".{destination.name}.",
            suffix=".tmp",
            dir=destination.parent,
            delete=False,
        ) as temporary:
            temporary.write(text)
            temporary.flush()
            temporary_path = Path(temporary.name)
        replace(temporary_path, destination)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)
