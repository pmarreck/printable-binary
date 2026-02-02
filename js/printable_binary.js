/**
 * PrintableBinary JavaScript Implementation
 * Encodes arbitrary binary data into human-readable UTF-8 strings and decodes them back
 *
 * This is a browser and Deno compatible implementation of the PrintableBinary algorithm.
 * It can be used as an ES module or included directly in HTML.
 */

const isNodeEnv = typeof process !== 'undefined' && !!process.versions?.node;
const isDenoEnv = typeof Deno !== 'undefined' && typeof Deno.readTextFileSync === 'function';

const CONTROL_NAMES = [
  'NUL','SOH','STX','ETX','EOT','ENQ','ACK','BEL',
  'BS','TAB','LF','VT','FF','CR','SO','SI',
  'DLE','DC1','DC2','DC3','DC4','NAK','SYN','ETB',
  'CAN','EM','SUB','ESC','FS','GS','RS','US'
];

function asciiName(byte) {
  if (byte <= 0x1F) {
    return CONTROL_NAMES[byte];
  }
  if (byte === 0x20) {
    return 'SPACE';
  }
  if (byte === 0x7F) {
    return 'DEL';
  }
  if (byte >= 0x21 && byte <= 0x7E) {
    const ch = String.fromCharCode(byte);
    const sanitized = ch === '\\' || ch === "'" ? '\\' + ch : ch;
    return `'${sanitized}'`;
  }
  return `0x${byte.toString(16).toUpperCase().padStart(2, '0')}`;
}

function parseCharacterMap(text) {
  if (typeof text !== 'string') {
    throw new Error('Character map must be provided as text');
  }
  const lines = text.replace(/\r\n/g, '\n').split('\n');
  if (lines.length < 256) {
    throw new Error(`Character map requires 256 lines, found ${lines.length}`);
  }
  return lines.slice(0, 256);
}

function warnSpacesAfterNewline(str) {
  if (/[\n\r] {2,}/.test(str)) {
    console.warn('Warning: spaces after newline are treated as data in --spaces mode');
  }
}

let defaultCharacterMap = null;
if (isNodeEnv) {
  const { readFileSync } = await import('node:fs');
  const { fileURLToPath } = await import('node:url');
  const { dirname, join } = await import('node:path');

  const moduleDir = dirname(fileURLToPath(import.meta.url));
  const candidates = [];
  if (process.env.PRINTABLE_BINARY_MAP) {
    candidates.push(process.env.PRINTABLE_BINARY_MAP);
  }
  candidates.push(join(moduleDir, 'character_map.txt'));
  candidates.push(join(moduleDir, '..', 'character_map.txt'));
  candidates.push(join(moduleDir, '..', 'bin', 'character_map.txt'));
  candidates.push(join(moduleDir, '..', 'docs', 'character_map.txt'));
  candidates.push(join(process.cwd(), 'character_map.txt'));

  let lastError;
  for (const candidate of candidates) {
    try {
      const text = readFileSync(candidate, 'utf8');
      defaultCharacterMap = parseCharacterMap(text);
      break;
    } catch (err) {
      lastError = err;
    }
  }

  if (!defaultCharacterMap) {
    throw new Error(`PrintableBinary: Unable to load character_map.txt. Set PRINTABLE_BINARY_MAP or place the map alongside printable_binary.js. Last error: ${lastError?.message ?? 'none'}`);
  }
} else if (isDenoEnv) {
  const moduleDir = new URL('./', import.meta.url);
  const candidates = [];
  if (Deno.env?.get('PRINTABLE_BINARY_MAP')) {
    candidates.push(Deno.env.get('PRINTABLE_BINARY_MAP'));
  }
  candidates.push(new URL('character_map.txt', moduleDir).pathname);
  candidates.push(new URL('../character_map.txt', moduleDir).pathname);
  candidates.push(new URL('../bin/character_map.txt', moduleDir).pathname);
  candidates.push(new URL('../docs/character_map.txt', moduleDir).pathname);
  candidates.push(`${Deno.cwd()}/character_map.txt`);

  let lastError;
  for (const candidate of candidates) {
    try {
      const text = Deno.readTextFileSync(candidate);
      defaultCharacterMap = parseCharacterMap(text);
      break;
    } catch (err) {
      lastError = err;
    }
  }

  if (!defaultCharacterMap) {
    throw new Error(`PrintableBinary: Unable to load character_map.txt. Set PRINTABLE_BINARY_MAP or place the map alongside printable_binary.js. Last error: ${lastError?.message ?? 'none'}`);
  }
}

class PrintableBinary {
  constructor(options = {}) {
    this.encodeMap = new Map(); // number (0-255) -> string (UTF-8 character)
    this.decodeMap = new Map(); // string (UTF-8 character) -> number (0-255)

    const mapLines = options.map || defaultCharacterMap;
    if (!mapLines) {
      throw new Error('PrintableBinary: character map not provided. Supply { map: [...] } when constructing in non-Node environments.');
    }

    this.buildMaps(mapLines);
  }

  static parseMap(text) {
    return parseCharacterMap(text);
  }

  buildMaps(mapLines) {
    if (!Array.isArray(mapLines) || mapLines.length < 256) {
      throw new Error('PrintableBinary: character map must be an array of 256 entries');
    }

    for (let i = 0; i < 256; i++) {
      const char = mapLines[i];
      if (typeof char !== 'string' || char.length === 0) {
        throw new Error(`PrintableBinary: invalid character mapping at index ${i}`);
      }
      this.encodeMap.set(i, char);
      this.decodeMap.set(char, i);
    }
  }

  /**
   * Encode binary data (Uint8Array or ArrayBuffer) to printable UTF-8 string
   * @param {Uint8Array|ArrayBuffer} binaryData - The binary data to encode
   * @param {Object} options - Optional encoding options
   * @param {boolean} options.spaces - Preserve literal spaces (don't encode to ␣)
   * @param {boolean} options.tabs - Preserve literal tabs (don't encode to ⇥)
   * @param {boolean} options.crlf - Preserve literal CR/LF (don't encode to ⏎/↧)
   * @param {string} options.preserve - String of specific characters to preserve
   * @param {string} options.format - Format specification (e.g., "8x10" for 8 chars per group, 10 groups per line)
   * @returns {string} The encoded printable string
   */
  encode(binaryData, options = {}) {
    // Convert ArrayBuffer to Uint8Array if needed
    if (binaryData instanceof ArrayBuffer) {
      binaryData = new Uint8Array(binaryData);
    }

    if (!(binaryData instanceof Uint8Array)) {
      throw new Error("Input must be a Uint8Array or ArrayBuffer");
    }

    // Optimization: Build string in chunks to avoid massive array join
    // Join operations on million-element arrays are slow
    const CHUNK_SIZE = 65536; // 64KB chunks - balance between memory and performance
    const chunks = [];
    let currentChunk = [];

    const spacesMode = options.spaces === true;
    const tabsMode = options.tabs === true;
    const crlfMode = options.crlf === true;
    const preserveChars = options.preserve || '';

    // Build a Set of byte values to preserve
    const preserveSet = new Set();
    for (let i = 0; i < preserveChars.length; i++) {
      preserveSet.add(preserveChars.charCodeAt(i));
    }

    for (let i = 0; i < binaryData.length; i++) {
      const byte = binaryData[i];
      let encoded;

      // Check preservation modes
      if (spacesMode && byte === 0x20) {
        encoded = ' ';
      } else if (tabsMode && byte === 0x09) {
        encoded = '\t';
      } else if (crlfMode && (byte === 0x0A || byte === 0x0D)) {
        encoded = String.fromCharCode(byte);
      } else if (preserveSet.has(byte)) {
        encoded = String.fromCharCode(byte);
      } else {
        encoded = this.encodeMap.get(byte);
      }

      if (encoded !== undefined) {
        currentChunk.push(encoded);

        // Periodically join the chunk and reset
        if (currentChunk.length >= CHUNK_SIZE) {
          chunks.push(currentChunk.join(''));
          currentChunk = [];
        }
      }
    }

    // Don't forget the last chunk
    if (currentChunk.length > 0) {
      chunks.push(currentChunk.join(''));
    }

    let output = chunks.join('');

    // Apply formatting if requested
    if (options.format) {
      output = this.formatOutput(output, options.format, options);
    }

    return output;
  }

  getMappings() {
    const entries = [];
    for (let i = 0; i < 256; i++) {
      const mapping = this.encodeMap.get(i) ?? '';
      entries.push({
        byte: i,
        hex: `0x${i.toString(16).toUpperCase().padStart(2, '0')}`,
        dec: i,
        ascii: asciiName(i),
        mapping
      });
    }
    return entries;
  }

  /**
   * Format encoded output with grouping and line breaks
   * @param {string} encoded - The encoded string
   * @param {string} formatSpec - Format specification like "8x10" (8 chars per group, 10 groups per line)
   * @returns {string} Formatted output
   */
  formatOutput(encoded, formatSpec, options = {}) {
    // Parse format specification (e.g., "8x10" or "75x1")
    const match = formatSpec.match(/^(\d+)x(\d+)$/);
    if (!match) {
      throw new Error(`Invalid format specification: ${formatSpec}. Expected format like "8x10"`);
    }

    const charsPerGroup = parseInt(match[1], 10);
    const groupsPerLine = parseInt(match[2], 10);
    const glyphs = Array.from(encoded);
    const groupSeparator = options.spaces ? '\t' : ' ';

    if (groupsPerLine === 1) {
      const result = [];
      for (let index = 0; index < glyphs.length; index += charsPerGroup) {
        result.push(glyphs.slice(index, index + charsPerGroup).join(''));
      }
      return result.join('\n');
    }

    const result = [];
    let charCount = 0;
    let groupCount = 0;

    for (let i = 0; i < glyphs.length; i++) {
      result.push(glyphs[i]);
      charCount++;

      // Check if we've completed a group
      if (charCount === charsPerGroup) {
        groupCount++;
        charCount = 0;

        if (i < glyphs.length - 1) {
          if (groupCount === groupsPerLine) {
            result.push('\n');
            groupCount = 0;
          } else {
            result.push(groupSeparator);
          }
        } else if (groupCount === groupsPerLine) {
          groupCount = 0;
        }
      }
    }

    return result.join('');
  }

  /**
   * Decode printable UTF-8 string back to binary data
   * @param {string} printableString - The encoded string to decode
   * @param {Object} options - Optional decoding options
   * @param {boolean} options.spaces - Treat literal spaces as data (decode them to space bytes)
   * @param {boolean} options.stripWhitespace - Strip whitespace before decoding (for block-formatted input)
   * @param {boolean} options.warnOnIndent - Warn if spaces appear after newlines in spaces mode
   * @returns {Uint8Array} The decoded binary data
   */
  decode(printableString, options = {}) {
    if (typeof printableString !== 'string') {
      throw new Error("Input must be a string");
    }

    const spacesMode = options.spaces === true;
    const stripWhitespace = options.stripWhitespace === true;

    if (spacesMode && options.warnOnIndent) {
      warnSpacesAfterNewline(printableString);
    }

    // Only strip whitespace if explicitly requested
    let cleanedString = printableString;
    if (stripWhitespace) {
      cleanedString = spacesMode
        ? printableString.replace(/[\r\n\t]/g, '')
        : printableString.replace(/[\r\n\t ]/g, '');
    }

    const result = [];
    let i = 0;

    // Process the string one character at a time
    while (i < cleanedString.length) {
      if (spacesMode && cleanedString[i] === ' ') {
        result.push(0x20);
        i += 1;
        continue;
      }
      let matched = false;

      // Try to match longest first (up to 4 code points for our charset)
      // In JavaScript, some characters may be represented as surrogate pairs
      for (let len = Math.min(4, cleanedString.length - i); len >= 1; len--) {
        const sub = cleanedString.substring(i, i + len);
        const decoded = this.decodeMap.get(sub);

        if (decoded !== undefined) {
          result.push(decoded);
          i += len;
          matched = true;
          break;
        }
      }

      // If we didn't match any character in our map, pass through the UTF-8 character intact
      if (!matched) {
        // Get the code point at current position (handles surrogate pairs)
        const codePoint = cleanedString.codePointAt(i);
        if (codePoint !== undefined) {
          // Encode the code point to UTF-8 bytes and add to result
          const char = String.fromCodePoint(codePoint);
          const encoder = new TextEncoder();
          const bytes = encoder.encode(char);
          for (const byte of bytes) {
            result.push(byte);
          }
          // Advance by the number of UTF-16 code units (1 for BMP, 2 for supplementary)
          i += char.length;
        } else {
          i++;
        }
      }
    }

    return new Uint8Array(result);
  }

  /**
   * Encode a string (treating it as UTF-8) to printable format
   * This is a convenience method for string input
   * @param {string} str - The string to encode
   * @returns {string} The encoded printable string
   */
  encodeString(str, options = {}) {
    const encoder = new TextEncoder();
    const bytes = encoder.encode(str);
    return this.encode(bytes, options);
  }

  /**
   * Decode a printable string back to a UTF-8 string
   * This is a convenience method that returns a string instead of bytes
   * @param {string} printableString - The encoded string to decode
   * @returns {string} The decoded string
   */
  decodeToString(printableString, options = {}) {
    const bytes = this.decode(printableString, options);
    const decoder = new TextDecoder();
    return decoder.decode(bytes);
  }
}

// Export for different environments
// ES Module export
export default PrintableBinary;

// CommonJS export for Node.js compatibility
if (typeof module !== 'undefined' && module.exports) {
  module.exports = PrintableBinary;
}

// Global export for browser <script> tag usage
if (typeof window !== 'undefined') {
  window.PrintableBinary = PrintableBinary;
}

// Deno compatibility
if (typeof Deno !== 'undefined') {
  // Already exported via ES module
}
