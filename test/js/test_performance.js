import PrintableBinary from '../js/printable_binary.js';

const encoder = new PrintableBinary();

// Create a 1MB test file
const size = 1024 * 1024; // 1MB
const input = new Uint8Array(size);
for (let i = 0; i < size; i++) {
  input[i] = i % 256;
}

console.log('Performance test with 1MB of data:');
console.log('===================================\n');

// Test encoding without format
console.log('Testing encode (no format)...');
let start = performance.now();
const plain = encoder.encode(input);
let end = performance.now();
console.log(`  Time: ${(end - start).toFixed(2)}ms`);
console.log(`  Output size: ${plain.length} chars`);
console.log();

// Test encoding with format
console.log('Testing encode (75x1 format)...');
start = performance.now();
const formatted = encoder.encode(input, { format: '75x1' });
end = performance.now();
console.log(`  Time: ${(end - start).toFixed(2)}ms`);
console.log(`  Output size: ${formatted.length} chars`);
console.log();

// Test formatting separately (simulating the old approach)
console.log('Testing formatOutput separately (old approach)...');
start = performance.now();
const formatted2 = encoder.formatOutput(plain, '75x1');
end = performance.now();
console.log(`  Time: ${(end - start).toFixed(2)}ms`);
console.log(`  Output size: ${formatted2.length} chars`);
console.log();

// Test decoding
console.log('Testing decode...');
start = performance.now();
const decoded = encoder.decode(formatted);
end = performance.now();
console.log(`  Time: ${(end - start).toFixed(2)}ms`);
console.log(`  Output size: ${decoded.length} bytes`);
console.log(`  Match: ${decoded.length === input.length}`);
