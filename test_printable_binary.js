#!/usr/bin/env -S deno run --allow-read

/**
 * Test harness for PrintableBinary JavaScript implementation
 * Can be run with Deno: deno run --allow-read test_printable_binary.js
 * Or with Node.js: node test_printable_binary.js
 */

import PrintableBinary from './printable_binary.js';

// Test utilities
let testCount = 0;
let passCount = 0;
let failCount = 0;

function assert(condition, message) {
  testCount++;
  if (condition) {
    passCount++;
    console.log(`✓ Test ${testCount}: ${message}`);
  } else {
    failCount++;
    console.error(`✗ Test ${testCount}: ${message}`);
    throw new Error(`Assertion failed: ${message}`);
  }
}

function assertEquals(actual, expected, message) {
  const isEqual = JSON.stringify(actual) === JSON.stringify(expected);
  assert(isEqual, message);
  if (!isEqual) {
    console.error(`  Expected: ${JSON.stringify(expected)}`);
    console.error(`  Actual:   ${JSON.stringify(actual)}`);
  }
}

function assertArrayEquals(actual, expected, message) {
  const isEqual = actual.length === expected.length &&
                  actual.every((val, idx) => val === expected[idx]);
  assert(isEqual, message);
  if (!isEqual) {
    console.error(`  Expected: [${expected.join(', ')}]`);
    console.error(`  Actual:   [${actual.join(', ')}]`);
  }
}

// Run tests
console.log('PrintableBinary JavaScript Implementation Tests');
console.log('=' .repeat(50));

const encoder = new PrintableBinary();

// Test 1: Basic encoding/decoding
console.log('\n--- Basic Encoding/Decoding Tests ---');
{
  const input = new Uint8Array([72, 101, 108, 108, 111]); // "Hello"
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'Basic encode/decode round-trip');
}

// Test 2: String encoding/decoding convenience methods
{
  const input = "Hello, World!";
  const encoded = encoder.encodeString(input);
  const decoded = encoder.decodeToString(encoded);
  assertEquals(decoded, input, 'String encode/decode round-trip');
}

// Test 3: Control characters
console.log('\n--- Control Character Tests ---');
{
  const input = new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]);
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'Control characters (0-15) encode/decode');
}

{
  const input = new Uint8Array([16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31]);
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'Control characters (16-31) encode/decode');
}

// Test 4: Special ASCII characters
console.log('\n--- Special ASCII Character Tests ---');
{
  const input = new Uint8Array([32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 45, 47]);
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'Special ASCII characters encode/decode');
}

// Test 5: Regular printable ASCII
console.log('\n--- Regular ASCII Tests ---');
{
  const input = new Uint8Array([65, 66, 67, 97, 98, 99, 48, 49, 50]); // "ABCabc012"
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'Regular ASCII characters encode/decode');
}

// Test 6: Extended bytes (128-255)
console.log('\n--- Extended Byte Tests ---');
{
  const input = new Uint8Array([128, 150, 152, 180, 184, 200, 255]);
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'Extended bytes encode/decode');
}

// Test 7: All possible byte values
console.log('\n--- Comprehensive Byte Range Test ---');
{
  const input = new Uint8Array(256);
  for (let i = 0; i < 256; i++) {
    input[i] = i;
  }
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'All byte values (0-255) encode/decode');
}

// Test 8: Empty input
console.log('\n--- Edge Case Tests ---');
{
  const input = new Uint8Array([]);
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'Empty input encode/decode');
}

// Test 9: Single byte
{
  const input = new Uint8Array([42]);
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'Single byte encode/decode');
}

// Test 10: NUL bytes
{
  const input = new Uint8Array([0, 0, 0, 1, 0, 2, 0]);
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'NUL bytes encode/decode');
}

// Test 11: Decode with whitespace (should be ignored)
console.log('\n--- Whitespace Handling Tests ---');
{
  const input = "Hello";
  const encoded = encoder.encodeString(input);
  const withWhitespace = encoded.split('').join(' '); // Add spaces between characters
  const decoded = encoder.decodeToString(withWhitespace);
  assertEquals(decoded, input, 'Decode with spaces between characters');
}

{
  const input = "Test";
  const encoded = encoder.encodeString(input);
  const withNewlines = encoded.split('').join('\n'); // Add newlines
  const decoded = encoder.decodeToString(withNewlines);
  assertEquals(decoded, input, 'Decode with newlines between characters');
}

// Test 12: Known mappings verification
console.log('\n--- Known Mapping Verification Tests ---');
{
  // Test specific byte mappings from the specification
  const byte0 = new Uint8Array([0]);
  const encoded0 = encoder.encode(byte0);
  assertEquals(encoded0, "\u2205", 'Byte 0 maps to ∅ (U+2205)');
}

{
  const byte32 = new Uint8Array([32]);
  const encoded32 = encoder.encode(byte32);
  assertEquals(encoded32, "\u2423", 'Byte 32 (space) maps to ␣ (U+2423)');
}

{
  const byte34 = new Uint8Array([34]);
  const encoded34 = encoder.encode(byte34);
  assertEquals(encoded34, "\u02F5", 'Byte 34 (") maps to ˵ (U+02F5)');
}

{
  const byte92 = new Uint8Array([92]);
  const encoded92 = encoder.encode(byte92);
  assertEquals(encoded92, "\u29F7", 'Byte 92 (\\) maps to ⧷ (U+29F7)');
}

{
  const byte127 = new Uint8Array([127]);
  const encoded127 = encoder.encode(byte127);
  assertEquals(encoded127, "\u2326", 'Byte 127 (DEL) maps to ⌦ (U+2326)');
}

// Test 13: Mapping export metadata
console.log('\n--- Mapping Export Tests ---');
{
  const mappings = encoder.getMappings();
  assert(Array.isArray(mappings) && mappings.length === 256, 'getMappings returns 256 entries');
  const first = mappings[0];
  assertEquals(first.byte, 0, 'First mapping byte index');
  assertEquals(first.ascii, 'NUL', 'First mapping ASCII name');
  assertEquals(first.mapping, "\u2205", 'First mapping character');
  const last = mappings[255];
  assertEquals(last.byte, 255, 'Last mapping byte index');
  assert(typeof last.mapping === 'string' && last.mapping.length > 0, 'Last mapping has glyph');
}

// Note: Bytes 152 and 184 previously had special mappings (U+014C and U+014F)
// but with the new Latin Extended-A mapping for 128-191, they follow the pattern:
// Byte 152 → U+0100 + (152-128) = U+0118
// Byte 184 → U+0100 + (184-128) = U+0138
// The round-trip test in "All byte values (0-255) encode/decode" verifies these work correctly

// Test 13: Binary data patterns
console.log('\n--- Binary Pattern Tests ---');
{
  // Test with repeating pattern
  const input = new Uint8Array([170, 85, 170, 85, 170, 85]); // 0xAA, 0x55 pattern
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'Repeating binary pattern');
}

{
  // Test with sequential bytes
  const input = new Uint8Array([100, 101, 102, 103, 104, 105]);
  const encoded = encoder.encode(input);
  const decoded = encoder.decode(encoded);
  assertArrayEquals(Array.from(decoded), Array.from(input), 'Sequential byte values');
}

// Test 14: Large data test
console.log('\n--- Performance Tests ---');
{
  const size = 10000;
  const input = new Uint8Array(size);
  for (let i = 0; i < size; i++) {
    input[i] = i % 256;
  }

  const startEncode = performance.now();
  const encoded = encoder.encode(input);
  const encodeTime = performance.now() - startEncode;

  const startDecode = performance.now();
  const decoded = encoder.decode(encoded);
  const decodeTime = performance.now() - startDecode;

  assertArrayEquals(Array.from(decoded), Array.from(input), `Large data (${size} bytes) encode/decode`);
  console.log(`  Encode time: ${encodeTime.toFixed(2)}ms`);
  console.log(`  Decode time: ${decodeTime.toFixed(2)}ms`);
}

// Test 15: Error handling
console.log('\n--- Error Handling Tests ---');
{
  try {
    encoder.encode("not a Uint8Array");
    assert(false, 'Should throw error for non-Uint8Array input');
  } catch (e) {
    assert(true, 'Throws error for invalid encode input');
  }
}

{
  try {
    encoder.decode(123);
    assert(false, 'Should throw error for non-string input');
  } catch (e) {
    assert(true, 'Throws error for invalid decode input');
  }
}

// Summary
console.log('\n' + '='.repeat(50));
console.log('Test Summary:');
console.log(`Total tests: ${testCount}`);
console.log(`Passed: ${passCount}`);
console.log(`Failed: ${failCount}`);

if (failCount === 0) {
  console.log('\n🎉 All tests passed!');
  if (typeof Deno !== 'undefined') {
    Deno.exit(0);
  } else if (typeof process !== 'undefined') {
    process.exit(0);
  }
} else {
  console.log(`\n❌ ${failCount} test(s) failed`);
  if (typeof Deno !== 'undefined') {
    Deno.exit(1);
  } else if (typeof process !== 'undefined') {
    process.exit(1);
  }
}
