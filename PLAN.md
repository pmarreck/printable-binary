---
purpose: Track work on GitHub issue #1 — web-demo decode workflow + printable-binary-file.json metadata container
audience: both
maintained_by: agent
---

# PLAN — issue #1: decode workflow + printable-binary-file.json

GitHub: https://github.com/pmarreck/printable_binary/issues/1
Dispatch: inbox/2026-06-26-handle-issue-1.md (from Einstein)
Spec: docs/plans/2026-06-26-printable-binary-file-container-design.md
Ruling: inbox/processed/2026-06-26-RULING-issue-1-forks.md

## Forks RESOLVED (Einstein ruling, on Peter's behalf, 2026-06-26)
- [x] FORK 1 — Architecture: **A2** (schema-as-spec + shared crc32 in core via FFI;
      consumers assemble the trivial JSON envelope natively; cross-impl differential
      + CRC vector-pinning guard drift; web stays pure-JS).
- [x] FORK 2 — Checksum: **CRC-32/ISO-HDLC** (vector-pinned).
- [x] FORK 3 — Naming: **<name>.pbf.json**.
- [~] FORK 4 — Web UX: NON-BLOCKING. Mock BOTH (explicit Encode/Decode AND
      auto-detect) for Peter at the web gate; bias toward making decode obvious.
- Minors: snake_case keys; lowercase 8-hex crc; container is ADDITIVE (raw .pbt stays).

## SCM NOTE (resolved 2026-06-26 ~3:52pm EDT) — see Einstein ping
Session began with the working copy on a STALE base (dd0f8014, Jun 10), which
PREDATES the Jun 24 "refactor(ffi): move pb_* exports into ffi.zig" + Jun 25 CI.
First crc32 pass landed there (export wrongly in printable_binary.zig). Re-homed
NON-DESTRUCTIVELY onto yolo (3a6682dc): `jj new yolo` + restored docs + re-applied
code in the CORRECT files. Old work preserved at ef7496c2 (change pqzmoppq/0,
divergent) — recoverable via jj oplog. NOT pushed; stale divergent commit NOT
abandoned — both await Peter's ok.

## DONE (core-first TDD, on yolo base, all green)
- [x] `pub fn crc32` (src/zig/printable_binary.zig) — CRC-32/ISO-HDLC, bitwise,
      vector test (CRC32("")=0, "123456789"=0xCBF43926, "a"=0xE8B7BE43). RED→GREEN.
- [x] `export fn pb_crc32` (src/zig/ffi.zig) — delegates to pb.crc32; FFI test
      (vector + null-safety). RED→GREEN.
- [x] Green on: test-zig-unit, test-ffi-cli, test-zig (ReleaseFast + cross-impl).
      no-ffi-symbols invariant safe by construction (Linux CI enforces on push).

## NEXT (after Peter spec sign-off; web held for Peter mockup)
- [ ] C header decl `uint32_t pb_crc32(const char*, size_t);` (src/printable_binary.h)
      — add when the CLI container verb consumes it (TDD-driven).
- [ ] Container codec — round-trip oracle FIRST: decode(encode(bytes,meta)) ==
      (bytes,meta), byte+metadata identical. JS side (js/printable_binary.js) +
      C CLI side.
- [ ] JS CRC-32 (vector-pinned to same constant) for the web demo.
- [ ] CLI verb to emit/consume <name>.pbf.json (-/@stdin/@stdout + JSON I/O conv).
- [ ] Cross-impl differential: C-built container decodes in JS & vice versa.
- [ ] Web demo: decode UI (paste + drop) + metadata restore. MOCK BOTH for Peter
      FIRST (can't see rendered output), then build.
- [ ] Docs: README section, dirtree notes, schema doc finalize.

## Discipline
jj-only (never raw git); rm-safe (mv ~/.Trash, never rm); per-unit ./test green;
./build + full suite before push; commit per logical unit; ping Einstein at milestones.
