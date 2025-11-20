# Future Work / Proposed Ideas

- APE disassembly: cstool/popen inside Cosmopolitan is flaky; keep disassembly tests skipped for APE unless we add an in-process Capstone binding.
- CLI wrapper resilience: add a smoke test so `bin/printable_binary` errors clearly when neither LuaJIT nor a WASM runner is available.
- JS/Node perf: cache the parsed `character_map` inside `js/printable_binary.js` to avoid repeated fs reads in tight loops.
- Map override parity: allow `PRINTABLE_BINARY_MAP` in the WASM test harness, mirroring Node/Deno behavior.
- CI ergonomics: run `test_all` per implementation (LuaJIT, C, APE, Node) in parallel to shorten logs and isolate failures.
- Docs: mention that APE disassembly is skipped by default and how to run native disassembly with `cstool`/`objdump` if needed.
