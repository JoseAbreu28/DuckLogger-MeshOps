# lib/microdot/

## Purpose

- Vendored Microdot micro-framework providing `Microdot`, `Response`, `send_file`, and WebSocket support for the device API.

## Ownership

- Upstream Microdot. This repo owns only local patches.

## Local Contracts

- Treat as third-party code: no style refactors, no mass edits, no new features.
- Patch only what a specific bug requires, keep the patch minimal and noted here when it diverges from upstream.
- `api.py` depends on `Microdot`, `send_file`, `Response`, `with_websocket`, and `WebSocketError`; keep those exports stable.

## Work Guidance

- If an upstream sync is needed, replace whole files and re-run the API tests rather than editing piecemeal.

## Verification

- `python -m pytest -q tests/test_api.py` and `python -m compileall -q lib/microdot`.

## Child DOX Index

- None. This boundary is a single vendored dependency.
