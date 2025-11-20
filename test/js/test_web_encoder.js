#!/usr/bin/env node

// Test the web encoder to see if it matches CLI output
import PrintableBinary from '../js/printable_binary.js';
import fs from 'fs';

const encoder = new PrintableBinary();

// Read the test file
const inputFile = process.argv[2] || '/home/pmarreck/Pictures/big-desktops/shadow of the colossus.jpg';
const data = fs.readFileSync(inputFile);

console.error(`Input file: ${inputFile}`);
console.error(`Input size: ${data.length} bytes`);

// Encode without formatting
const encoded = encoder.encode(data);
console.error(`Encoded size: ${encoded.length} characters`);

// Calculate UTF-8 byte size
const encodedBytes = Buffer.byteLength(encoded, 'utf8');
console.error(`Encoded UTF-8 bytes: ${encodedBytes}`);

// Output the encoded data
process.stdout.write(encoded);
