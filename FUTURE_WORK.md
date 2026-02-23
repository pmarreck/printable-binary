# Future Work / Proposed Ideas

- CLI wrapper resilience: add a smoke test so `bin/printable-binary` errors clearly when neither LuaJIT nor a WASM runner is available.
- JS/Node perf: cache the parsed `character_map` inside `js/printable_binary.js` to avoid repeated fs reads in tight loops.
- Map override parity: allow `PRINTABLE_BINARY_MAP` in the WASM test harness, mirroring Node/Deno behavior.
- CI ergonomics: run `test_all` per implementation (LuaJIT, C, APE, Node) in parallel to shorten logs and isolate failures.
