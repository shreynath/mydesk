#!/usr/bin/env python3
"""Check a MyDesk capability manifest against repository evidence.

FUNCTIONAL and VERIFIED_PARITY require automated test names that appear in the
product test runner. NOT_STARTED entries cannot cite tests. The checker does
not treat FUNCTIONAL as verified parity.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ALLOWED_STATUS = {
    "NOT_STARTED",
    "IMPLEMENTING",
    "FUNCTIONAL",
    "PARTIAL_PARITY",
    "VERIFIED_PARITY",
    "REGRESSION",
}

REQUIRED_FIELDS = (
    "id",
    "target_application",
    "target_version",
    "category",
    "name",
    "command",
    "reference_behavior",
    "expected_input",
    "expected_output",
    "formats",
    "implementation",
    "tests",
    "manual_tests",
    "performance",
    "status",
    "evidence",
    "differences",
)

EVIDENCE_STATUS = {"FUNCTIONAL", "VERIFIED_PARITY", "REGRESSION"}


def fail(messages: list[str]) -> int:
    for message in messages:
        print(f"ERROR: {message}", file=sys.stderr)
    print(f"{len(messages)} manifest error(s)", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--tests", required=True, type=Path, nargs="+")
    parser.add_argument("--root", required=True, type=Path)
    args = parser.parse_args()

    errors: list[str] = []
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    test_source = "\n".join(path.read_text(encoding="utf-8") for path in args.tests)
    test_labels = ", ".join(str(path) for path in args.tests)
    capabilities = manifest.get("capabilities")
    if not isinstance(capabilities, list) or not capabilities:
        return fail(["capabilities must be a non-empty list"])

    seen: set[str] = set()
    counts = {status: 0 for status in ALLOWED_STATUS}
    for index, capability in enumerate(capabilities):
        label = f"capabilities[{index}]"
        if not isinstance(capability, dict):
            errors.append(f"{label} is not an object")
            continue
        missing = [field for field in REQUIRED_FIELDS if field not in capability]
        if missing:
            errors.append(f"{label} missing {', '.join(missing)}")
            continue
        cap_id = str(capability["id"])
        label = cap_id
        if cap_id in seen:
            errors.append(f"{label} duplicate id")
        seen.add(cap_id)
        status = capability["status"]
        if status not in ALLOWED_STATUS:
            errors.append(f"{label} has unknown status {status}")
            continue
        counts[status] += 1
        tests = capability["tests"]
        if not isinstance(tests, list) or not all(isinstance(name, str) for name in tests):
            errors.append(f"{label} tests must be a list of strings")
            tests = []
        if status == "NOT_STARTED" and tests:
            errors.append(f"{label} is NOT_STARTED but lists tests")
        if status in EVIDENCE_STATUS and not tests:
            errors.append(f"{label} status {status} requires automated tests")
        for name in tests:
            needle = f'"{name}"'
            if needle not in test_source:
                errors.append(f"{label} test {name} is not registered in {test_labels}")
        if status in EVIDENCE_STATUS:
            implementation = capability["implementation"]
            paths = implementation if isinstance(implementation, list) else [implementation]
            if not paths or any(not isinstance(path, str) or not path for path in paths):
                errors.append(f"{label} requires an implementation path")
            else:
                for path in paths:
                    if not (args.root / path).exists():
                        errors.append(f"{label} implementation path missing: {path}")
            if not str(capability["evidence"]).strip():
                errors.append(f"{label} requires evidence")
        if not str(capability["differences"]).strip():
            errors.append(f"{label} must record known differences")

    if errors:
        return fail(errors)

    print(
        f"OK {args.manifest.name}: {len(capabilities)} capabilities "
        + ", ".join(f"{status}={counts[status]}" for status in ALLOWED_STATUS)
    )
    verified = counts["VERIFIED_PARITY"]
    print(
        f"Verified parity is {verified} of {len(capabilities)} capabilities. "
        "FUNCTIONAL is not counted as complete parity."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
