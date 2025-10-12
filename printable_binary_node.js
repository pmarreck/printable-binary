#!/usr/bin/env node

/**
 * PrintableBinary Node.js CLI
 * Provides a command-line interface that mirrors the Lua/C tools
 * Supports encode/decode and optional formatting.
 */

import fs from 'fs';
import PrintableBinary from './printable_binary.js';

function printUsage() {
  const progname = process.argv[1]
    ? process.argv[1].split('/').slice(-1)[0]
    : 'printable_binary_node';

  const usage = `
PrintableBinary (JavaScript CLI) - Encode binary data as printable UTF-8 and decode it back

Usage: ${progname} [options] [file]

Options:
  -d, --decode          Decode mode (default is encode mode)
  -f, --format NxM      Format output in groups (e.g. 75x1)
  -f=NXM, --format=NXM  Alternate syntax for specifying formatting
  -h, --help            Show this help

If no file is specified, input is read from stdin.
Output is written to stdout.
`;
  process.stderr.write(usage);
}

function parseArgs(argv) {
  let decodeMode = false;
  let formatSpec = null;
  let filePath = null;

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];

    if (arg === '-h' || arg === '--help') {
      printUsage();
      process.exit(0);
    } else if (arg === '-d' || arg === '--decode') {
      decodeMode = true;
    } else if (arg === '-f' || arg === '--format') {
      if (i + 1 >= argv.length) {
        process.stderr.write('Error: --format requires a value like 75x1\n');
        process.exit(1);
      }
      formatSpec = argv[++i];
    } else if (arg.startsWith('-f=')) {
      formatSpec = arg.slice(3);
    } else if (arg.startsWith('--format=')) {
      formatSpec = arg.slice(9);
    } else if (arg.startsWith('-')) {
      process.stderr.write(`Error: Unknown option ${arg}\n`);
      printUsage();
      process.exit(1);
    } else if (!filePath) {
      filePath = arg;
    } else {
      process.stderr.write(`Error: Unexpected argument ${arg}\n`);
      process.exit(1);
    }
  }

  return { decodeMode, formatSpec, filePath };
}

async function readInput(filePath) {
  if (filePath) {
    return fs.promises.readFile(filePath);
  }

  // Read from stdin
  const chunks = [];
  for await (const chunk of process.stdin) {
    chunks.push(chunk);
  }
  return Buffer.concat(chunks);
}

async function main() {
  const { decodeMode, formatSpec, filePath } = parseArgs(process.argv.slice(2));
  const pb = new PrintableBinary();

  try {
    const input = await readInput(filePath);

    if (decodeMode) {
      const decoded = pb.decode(input.toString('utf8'));
      process.stdout.write(Buffer.from(decoded));
    } else {
      const encoded = pb.encode(input, formatSpec ? { format: formatSpec } : {});
      process.stdout.write(encoded);
    }
  } catch (err) {
    process.stderr.write(`Error: ${err.message}\n`);
    process.exit(1);
  }
}

main();
