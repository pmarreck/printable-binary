#!/usr/bin/env node

/**
 * PrintableBinary Node.js CLI
 * Provides a command-line interface that mirrors the Lua/C tools
 * Supports encode/decode and optional formatting.
 */

import fs from 'fs';
import PrintableBinary from '../js/printable_binary.js';

function printUsage() {
  const progname = process.argv[1]
    ? process.argv[1].split('/').slice(-1)[0]
    : 'printable_binary_node';

  const usage = `
PrintableBinary (JavaScript CLI) - Encode binary data as printable UTF-8 and decode it back

Usage: ${progname} [options] [file]

Options:
  -d, --decode          Decode mode (default is encode mode)
  -s, --spaces          Preserve literal spaces (decoder still ignores tabs/newlines/CR)
  -p, --passthrough     Pass input to stdout unchanged, send encoded data to stderr
  -f, --format NxM      Format output in groups (e.g. 75x1)
  -f=NXM, --format=NXM  Alternate syntax for specifying formatting
  --mappings            Show the byte-to-character mapping table
  --mappings-json       Output the mappings as JSON
  --mappings-csv        Output the mappings as CSV
  -h, --help            Show this help

Range options (select byte range from input before processing):
  --range X-Y              Byte range, 0-indexed inclusive (e.g., --range 0-9)
  --start X                Start offset (negative = from end, like xxd -s)
  --end Y                  End offset (inclusive)
  X-Y (positional)         Shorthand for --range X-Y
  Hex offsets supported: --range 0x0A-0xFF
  Omitted bounds: --range -9 (first 10 bytes), --range 10- (byte 10 to EOF)

Encoded output is whitespace-agnostic: you can reflow, indent, or wrap it freely because the decoder ignores real whitespace while rendering quotes, backslashes, tabs, and control bytes as visible, but clearly related, glyphs (e.g., SPACE→␣, TAB→⇥, CR→⏎, LF→↧, single quote→ʼ, double quote→˵, backslash→⧷). With --spaces, literal spaces are treated as data (and we warn on indented lines).

If no file is specified, input is read from stdin.
Output is written to stdout.
`;
  process.stderr.write(usage);
}

function csvEscape(value) {
  return `"${value.replace(/"/g, '""')}"`;
}

function outputMappings(entries, format) {
  if (format === 'json') {
    process.stdout.write(JSON.stringify(entries, null, 2) + '\n');
    return;
  }

  if (format === 'csv') {
    const lines = ['byte,hex,dec,ascii,mapping'];
    for (const entry of entries) {
      lines.push([
        entry.byte,
        entry.hex,
        entry.dec,
        csvEscape(entry.ascii),
        csvEscape(entry.mapping)
      ].join(','));
    }
    process.stdout.write(lines.join('\n') + '\n');
    return;
  }

  const header = `${'Byte'.padEnd(6)} ${'Dec'.padEnd(5)} ${'ASCII'.padEnd(12)} Mapping`;
  process.stdout.write(header + '\n');
  for (const entry of entries) {
    const line = `${entry.hex.padEnd(6)} ${String(entry.dec).padEnd(5)} ${entry.ascii.padEnd(12)} ${entry.mapping}`;
    process.stdout.write(line + '\n');
  }
}

function parseOffsetValue(s) {
  if (!s || s.length === 0) return null;
  if (s.startsWith('0x') || s.startsWith('0X')) {
    const val = parseInt(s, 16);
    return isNaN(val) ? null : val;
  }
  const val = parseInt(s, 10);
  return isNaN(val) ? null : val;
}

function parseRangeSpec(spec) {
  if (!spec || spec.length === 0) return null;
  // Find separator hyphen (not inside hex prefix)
  let sepIdx = -1;
  let i = 0;
  if (spec.length > 2 && spec[0] === '0' && (spec[1] === 'x' || spec[1] === 'X')) {
    i = 2;
    while (i < spec.length && /[0-9a-fA-F]/.test(spec[i])) i++;
  } else {
    while (i < spec.length && /[0-9]/.test(spec[i])) i++;
  }
  if (i < spec.length && spec[i] === '-') {
    sepIdx = i;
  } else if (spec[0] === '-') {
    sepIdx = 0;
  } else {
    return null;
  }

  let start = null;
  let end = null;
  if (sepIdx > 0) {
    start = parseOffsetValue(spec.slice(0, sepIdx));
  }
  const endStr = spec.slice(sepIdx + 1);
  if (endStr.length > 0) {
    end = parseOffsetValue(endStr);
  }
  return { start, end };
}

function isPositionalRange(s) {
  if (!s || s.length === 0 || s[0] === '-') return false;
  return /^(0x[0-9a-fA-F]+|\d+)-/.test(s);
}

function parseArgs(argv) {
  let decodeMode = false;
  let formatSpec = null;
  let filePath = null;
  let mappingsFormat = null;
  let passthrough = false;
  let spacesMode = false;
  let rangeStart = null;
  let rangeEnd = null;

  const setMappingsFormat = (mode) => {
    if (mappingsFormat && mappingsFormat !== mode) {
      process.stderr.write('Error: Only one mappings output option can be specified\n');
      process.exit(1);
    }
    mappingsFormat = mode;
  };

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];

    if (arg === '-h' || arg === '--help') {
      printUsage();
      process.exit(0);
    } else if (arg === '-d' || arg === '--decode') {
      decodeMode = true;
    } else if (arg === '-s' || arg === '--spaces') {
      spacesMode = true;
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
    } else if (arg === '--mappings') {
      setMappingsFormat('table');
    } else if (arg === '--mappings-json') {
      setMappingsFormat('json');
    } else if (arg === '--mappings-csv') {
      setMappingsFormat('csv');
    } else if (arg === '-p' || arg === '--passthrough') {
      passthrough = true;
    } else if (arg === '--range') {
      if (i + 1 >= argv.length) {
        process.stderr.write('Error: --range requires an argument\n');
        process.exit(1);
      }
      const parsed = parseRangeSpec(argv[++i]);
      if (!parsed) {
        process.stderr.write(`Error: Invalid range specification: ${argv[i]}\n`);
        process.exit(1);
      }
      if (parsed.start !== null) rangeStart = parsed.start;
      if (parsed.end !== null) rangeEnd = parsed.end;
    } else if (arg.startsWith('--range=')) {
      const parsed = parseRangeSpec(arg.slice(8));
      if (!parsed) {
        process.stderr.write(`Error: Invalid range specification: ${arg.slice(8)}\n`);
        process.exit(1);
      }
      if (parsed.start !== null) rangeStart = parsed.start;
      if (parsed.end !== null) rangeEnd = parsed.end;
    } else if (arg === '--start') {
      if (i + 1 >= argv.length) {
        process.stderr.write('Error: --start requires an argument\n');
        process.exit(1);
      }
      rangeStart = parseOffsetValue(argv[++i]);
    } else if (arg.startsWith('--start=')) {
      rangeStart = parseOffsetValue(arg.slice(8));
    } else if (arg === '--end') {
      if (i + 1 >= argv.length) {
        process.stderr.write('Error: --end requires an argument\n');
        process.exit(1);
      }
      rangeEnd = parseOffsetValue(argv[++i]);
    } else if (arg.startsWith('--end=')) {
      rangeEnd = parseOffsetValue(arg.slice(6));
    } else if (arg.startsWith('-')) {
      process.stderr.write(`Error: Unknown option ${arg}\n`);
      printUsage();
      process.exit(1);
    } else if (isPositionalRange(arg)) {
      const parsed = parseRangeSpec(arg);
      if (parsed) {
        if (parsed.start !== null) rangeStart = parsed.start;
        if (parsed.end !== null) rangeEnd = parsed.end;
      } else if (!filePath) {
        filePath = arg;
      } else {
        process.stderr.write(`Error: Unexpected argument ${arg}\n`);
        process.exit(1);
      }
    } else if (!filePath) {
      filePath = arg;
    } else {
      process.stderr.write(`Error: Unexpected argument ${arg}\n`);
      process.exit(1);
    }
  }

  return { decodeMode, formatSpec, filePath, mappingsFormat, passthrough, spacesMode, rangeStart, rangeEnd };
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
  const { decodeMode, formatSpec, filePath, mappingsFormat, passthrough, spacesMode, rangeStart, rangeEnd } = parseArgs(process.argv.slice(2));
  const pb = new PrintableBinary();

  try {
    if (mappingsFormat) {
      const entries = pb.getMappings();
      outputMappings(entries, mappingsFormat);
      return;
    }

    let input = await readInput(filePath);

    // Apply byte range if specified
    if (rangeStart !== null || rangeEnd !== null) {
      const inputLen = input.length;
      let start = rangeStart !== null ? rangeStart : 0;
      let end = rangeEnd !== null ? rangeEnd : inputLen - 1;

      // Handle negative start (from end)
      if (start < 0) {
        start = inputLen + start;
        if (start < 0) start = 0;
      }

      if (start >= inputLen) {
        process.stderr.write(`Warning: start offset ${start} exceeds input size ${inputLen}\n`);
        input = Buffer.alloc(0);
      } else if (start > end) {
        process.stderr.write('Warning: start offset exceeds end offset, empty range\n');
        input = Buffer.alloc(0);
      } else {
        if (end >= inputLen) {
          process.stderr.write(`Warning: end offset ${end} exceeds input size ${inputLen}, clamping to ${inputLen - 1}\n`);
          end = inputLen - 1;
        }
        input = input.subarray(start, end + 1);
      }
    }

    if (decodeMode) {
      if (passthrough) {
        process.stderr.write('Warning: --passthrough ignored in decode mode\n');
      }
      const decoded = pb.decode(input.toString('utf8'), {
        spaces: spacesMode,
        warnOnIndent: spacesMode
      });
      process.stdout.write(Buffer.from(decoded));
    } else {
      const options = { spaces: spacesMode };
      if (formatSpec) {
        options.format = formatSpec;
      }
      const encoded = pb.encode(input, options);
      if (passthrough) {
        process.stdout.write(input);
        process.stderr.write(encoded);
      } else {
        process.stdout.write(encoded);
      }
    }
  } catch (err) {
    process.stderr.write(`Error: ${err.message}\n`);
    process.exit(1);
  }
}

main();
