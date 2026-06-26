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

## DONE (JS container codec, MFIC-guarded)
- [x] JS class `crc32`/`crc32hex` (ISO-HDLC, vector-pinned: ""=0, "123456789"=cbf43926,
      "a"=e8b7be43) + `encodeToContainer` (bytes+meta -> schema-v1 obj) +
      `decodeFromContainer` (self-verifying: crc32_encoded pre-decode, byte_length+
      crc32 post-decode, throws on mismatch; tolerates missing optional fields).
      RED->GREEN. 13 new tests in test/js/test_printable_binary.js (46 total).
- [x] Wired orphaned test/js suite into CI as `test-js-unit` (was unguarded) — the
      container round-trip oracle + crc32 vector pins now run in CI.

## NEXT (web held for Peter mockup)
- [ ] Node CLI (bin/printable-binary-node.js): container verb (emit/consume
      <name>.pbf.json; reads file + best-effort fs.stat metadata; -/@stdin/@stdout).
- [ ] C CLI (FFI, src/printable_binary_ffi_main.c): container verb dogfooding
      pb_crc32 + pb_encode/pb_decode; add `uint32_t pb_crc32(const char*,size_t);`
      to src/printable_binary.h; hand-rolled flat-schema JSON parse/assemble.
- [ ] Cross-impl differential (test/test_cross_implementation.sh): C-emitted
      container decodes in JS & vice versa (byte+metadata identical).
- [ ] Per-impl container round-trip in the cross-impl CLI suite (feature-detected).
- [ ] Web demo (index.html): decode UI (paste + drop) + metadata restore. MOCK
      BOTH (explicit Encode/Decode toggle AND auto-detect) for Peter FIRST, then build.
- [ ] Docs: README section, dirtree notes.

## Discipline
jj-only (never raw git); rm-safe (mv ~/.Trash, never rm); per-unit ./test green;
./build + full suite before push; commit per logical unit; ping Einstein at milestones.

## Session checkpoint (2026-06-26 ~4:18pm EDT)
Shipped + verified GREEN across all 5 platforms (Garnix "All checks" + GH Actions):
- cd6d32d1  feat(core): CRC-32 primitive + pb_crc32 FFI export
- cb1ce8e3  feat(js): printable-binary-file.json container codec + crc32 (+test-js-unit CI guard)
test-no-ffi-symbols [linux] PASSED → the pub crc32 leaked no pb_* symbol (invariant verified).
SCM hazard caught + re-homed onto yolo (Einstein confirmed). ef7496c2 kept as safety net.

BLOCKING THE ISSUE'S CORE FIX: Peter's web-UI mockup pick (Option A vs B in the
spec's "Web-UI mockups" section). AskUserQuestion blocked in-session → awaiting his
reply in-pane or via Einstein.

Queued next (non-gated, resume here): Node CLI container verb → C FFI CLI verb
(+pb_crc32 in src/printable_binary.h) → cross-impl differential (C container ↔ JS).
Then web UI once Peter picks.
