# ADR 0004: DWG compatibility strategy

## Status

Accepted (2026-10-10)

## Context

Users expect DWG. Autodesk DWG is proprietary. Illegal reverse engineering and unlicensed libraries are out of scope.

## Decision

1. Ship **DXF** import/export first with explicit partial-support reports (LINE/CIRCLE/ARC/LWPOLYLINE/LAYER).
2. Evaluate **lawfully licensed** DWG toolkits (e.g. LibreDWG if license-compatible with MPL-2.0 after review, or commercial SDKs under separate terms) behind `core/exchange` interfaces.
3. Never advertise “full DWG support” without versioned fixture corpora and a published unsupported-object list.

## Consequences

- DXF is the honest interchange path today.
- DWG remains `unsupported` in the compatibility matrix until a licensed path is integrated and tested.
