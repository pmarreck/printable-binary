import PrintableBinary from '../js/printable_binary.js';

const encoder = new PrintableBinary();

// Test data: "Hello, World!" (13 bytes)
const input = new Uint8Array([72, 101, 108, 108, 111, 44, 32, 87, 111, 114, 108, 100, 33]);

console.log('Testing format option:');
console.log('=====================\n');

// Test without format
const plain = encoder.encode(input);
console.log('Without format:');
console.log(plain);
console.log('Length:', plain.length);
console.log();

// Test with 8x10 format
const formatted8x10 = encoder.encode(input, { format: '8x10' });
console.log('With format "8x10":');
console.log(formatted8x10);
console.log('Length:', formatted8x10.length);
console.log();

// Test with 75x1 format
const formatted75x1 = encoder.encode(input, { format: '75x1' });
console.log('With format "75x1":');
console.log(formatted75x1);
console.log('Length:', formatted75x1.length);
console.log();

// Test decoding formatted output (should work since whitespace is ignored)
const decoded = encoder.decode(formatted75x1);
console.log('Decoded from 75x1 formatted output:');
console.log(Array.from(decoded));
console.log('Match:', Array.from(decoded).every((val, idx) => val === input[idx]));
