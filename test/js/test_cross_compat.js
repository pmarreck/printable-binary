#!/usr/bin/env -S deno run --allow-read --allow-run

/**
 * Cross-compatibility test between Lua and JavaScript implementations
 */

import PrintableBinary from '../../js/printable_binary.js';

const encoder = new PrintableBinary();

// Test string
const testString = "Hello, World!";

console.log("Cross-Implementation Compatibility Test");
console.log("=" .repeat(50));
console.log(`Test string: "${testString}"`);
console.log();

// Encode with JavaScript
const jsEncoded = encoder.encodeString(testString);
console.log(`JS encoded: ${jsEncoded}`);
console.log();

// Encode with Lua implementation
const luaProcess = Deno.run({
  cmd: ["./bin/printable-binary"],
  stdin: "piped",
  stdout: "piped",
  stderr: "piped"
});

const textEncoder = new TextEncoder();
await luaProcess.stdin.write(textEncoder.encode(testString));
await luaProcess.stdin.close();

const luaOutput = await luaProcess.output();
const luaStderr = await luaProcess.stderrOutput();
await luaProcess.status();

const textDecoder = new TextDecoder();
const luaEncoded = textDecoder.decode(luaOutput).trim();
const luaStderrText = textDecoder.decode(luaStderr);

console.log(`Lua encoded: ${luaEncoded}`);
console.log(`Lua stderr: ${luaStderrText}`);
console.log();

// Compare
if (jsEncoded === luaEncoded) {
  console.log("✓ Encodings match!");
} else {
  console.log("✗ Encodings differ!");
  console.log(`  JS:  ${jsEncoded}`);
  console.log(`  Lua: ${luaEncoded}`);

  // Show byte-by-byte comparison
  console.log("\nByte-by-byte comparison:");
  const maxLen = Math.max(jsEncoded.length, luaEncoded.length);
  for (let i = 0; i < maxLen; i++) {
    const jsChar = jsEncoded[i] || '(missing)';
    const luaChar = luaEncoded[i] || '(missing)';
    const match = jsChar === luaChar ? '✓' : '✗';
    console.log(`  ${i}: JS='${jsChar}' (U+${jsChar.codePointAt(0)?.toString(16).toUpperCase().padStart(4, '0')}) | Lua='${luaChar}' (U+${luaChar.codePointAt(0)?.toString(16).toUpperCase().padStart(4, '0')}) ${match}`);
  }
}

console.log();

// Test decoding
const jsDecoded = encoder.decodeToString(luaEncoded);
console.log(`Decoded from Lua output: "${jsDecoded}"`);

if (jsDecoded === testString) {
  console.log("✓ JS can decode Lua output correctly!");
} else {
  console.log("✗ JS decoding of Lua output failed!");
}
