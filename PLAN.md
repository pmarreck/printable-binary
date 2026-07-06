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

## Session checkpoint 2 (2026-06-26 ~9:16pm EDT)
ISSUE #1 CORE FIX SHIPPED + Peter-approved:
- 7b70844c  feat(web): Interface A (Encode/Decode tabs) + .pbf.json decode & metadata
  (decodeText router; container `data` serialized LAST per Peter; created_ms note).
Peter follow-up request: "all printable-binary executables understand .pbf.json".
- 9dda0bbd  feat(node-cli): -C/--container (emit+consume); test/test_container
  (parameterized CLI round-trip + self-verify) wired into CI as test-container-node.

ALL-EXECUTABLES STATUS (Peter's ask):
- [x] JS library + web demo
- [x] Node CLI (-C/--container)
- [ ] Zig CLI (src/zig/main.zig) — core has crc32; use std.json for the envelope.
- [ ] C FFI CLI (src/printable_binary_ffi_main.c) — pb_crc32 + hand-rolled flat JSON.
- [ ] C standalone (src/printable_binary.c) — own crc32 + hand-rolled flat JSON.
- [ ] Lua (bin/printable-binary) — own crc32 + Lua JSON.
PATTERN for each: add `-C` (encode→container, `-d -C` decode→restore+self-verify),
crc32 VECTOR-PINNED to CRC32("123456789")=0xCBF43926 (so all impls agree), then add
a test-container-<impl> flake check running test/test_container against it. Once ≥2
CLIs support it, add a cross-impl differential (impl-A container decodes via impl-B).
Schema: data LAST; optional POSIX/birthtime fields omitted when unavailable.

NOTE: local web preview server (node) still running on http://localhost:8099/ (bg
task biipba32h) — kill when done; the live GitHub Pages site also reflects 7b70844c.

## ALL EXECUTABLES DONE (2026-06-27) — issue #1 fully delivered
Every printable-binary executable understands .pbf.json (-C/--container):
- [x] JS library + web demo (Interface A)        [x] C FFI CLI (dogfoods pb_crc32)
- [x] Node CLI                                    [x] C standalone CLI
- [x] Zig CLI                                     [x] Lua reference CLI
- [x] Transport-resistance (canonicalize + lenient/pattern parse) in ALL impls
- [x] crc32 vector-pinned (CRC32("123456789")=0xCBF43926) across all
- [x] CI guards: test-container-{node,zig,ffi,c,lua} + test-container-cross
      (comprehensive MFIC differential: every impl decodes the Node reference's
      container byte-identically, and vice versa)
Shared: src/container_json.h (C), test/test_container (parameterized),
test/test_container_cross (differential).

## Elixir ~PB compile-time sigil demo (2026-06-30) — DONE, green in CI
Demonstrates embedding raw binary legibly inline in source (no fixture files):
`import PrintableBinary; @magic ~PB"..."` decodes glyphs -> raw bytes AT COMPILE
TIME (zero runtime cost; bytes baked into the BEAM). `"""` heredoc works since a
literal `"` never appears in the payload.
- [x] elixir/lib/printable_binary.ex — `defmacro sigil_PB({:"<<>>",_,[str]},_)` +
      `decode/1` (whitespace-tolerant). Map parsed from character_map.txt at compile
      time via @external_resource (single source of truth; same parse as all impls).
- [x] elixir/test/printable_binary_test.exs — 5 tests + 1 doctest (passthrough,
      control glyph byte0=·, whitespace-ignored heredoc, all-256 round-trip, raise).
- [x] flake check `test-elixir`: `mix test` + MFIC cross-impl guard — Zig CLI is the
      INDEPENDENT encoder oracle; Elixir decode/1 must reproduce originals over
      all-256 single bytes + 8 KiB random (2/3-byte glyph boundaries). Green.
- [x] elixir/_build gitignored (swept-in artifacts moved out, not committed).

## WIND-DOWN STATE (2026-07-06 ~07:05 EDT) — fleet migrates to Thelio
STATUS: fully green, everything pushed. `yolo = yolo@origin = e4b7275c` (clean WC).
Last CI (GH Actions run 28490792396 + Garnix, both commits): ALL GREEN incl test-elixir.

DONE (shipped + CI-green):
- [x] Issue #1: web decode workflow + `.pbf.json` container across ALL 5 impls
      (JS/web, Node, Zig, C-FFI, C-standalone, Lua); transport-resistant; MIME;
      crc32 vector-pinned; CI guards test-container-{node,zig,ffi,c,lua}+cross.
- [x] Rust crate (rust/): byte-identical to Zig, zero-alloc encode/decode, LTO;
      CI test-rust (Rust encode == Zig encode + decode round-trip over all 256).
- [x] build.zig: ReleaseFast default; in-process `--bench` (Zig 0.16 Io clock).
- [x] Elixir `~PB` compile-time sigil (elixir/): decodes glyphs -> raw binary at
      compile time. 5 tests + doctest. CI test-elixir = mix test + MFIC cross-check
      (Zig CLI is the INDEPENDENT encoder oracle; Elixir decode/1 must reproduce
      originals over all-256 + 8KiB random). README documented w/ verified glyphs.

PARKED — Peter's call, do NOT start autonomously:
- [ ] Transport protocol (Rust GUI <-> Zig core, RAW not container). GATED: Peter
      was speccing details with another LLM ("stand by on the specifics").
- [ ] crc32 table-driven optimization (Peter approved earlier, CONTAINER-ONLY scope).
      Bench before/after per the benchmark-before-after-optimizations discipline.
- [ ] Zig decode perf: Rust decode measured ~2.2x faster than Zig decode (encode is
      ~tied). Zig decode has headroom — candidate optimization pass if transport
      use-case wants it. (Rust uses O(1) decode tables: decode_1[256],
      decode_2[32][64] payload-indexed, decode_3 sorted binary-search.)

NEXT SESSION (on Thelio, full context restored): pick ONE parked item once Peter
directs. The concrete non-gated win is the Zig-decode optimization (mirror Rust's
table strategy in src/zig/printable_binary.zig decode path); everything else waits
on Peter's transport-protocol decision.
