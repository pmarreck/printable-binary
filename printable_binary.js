/**
 * PrintableBinary JavaScript Implementation
 * Encodes arbitrary binary data into human-readable UTF-8 strings and decodes them back
 *
 * This is a browser and Deno compatible implementation of the PrintableBinary algorithm.
 * It can be used as an ES module or included directly in HTML.
 */

const isNodeEnv = typeof process !== 'undefined' && !!process.versions?.node;

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

    for (let i = 0; i < binaryData.length; i++) {
      const byte = binaryData[i];
      const encoded = this.encodeMap.get(byte);
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
      output = this.formatOutput(output, options.format);
    }

    return output;
  }

  /**
   * Format encoded output with grouping and line breaks
   * @param {string} encoded - The encoded string
   * @param {string} formatSpec - Format specification like "8x10" (8 chars per group, 10 groups per line)
   * @returns {string} Formatted output
   */
  formatOutput(encoded, formatSpec) {
    // Parse format specification (e.g., "8x10" or "75x1")
    const match = formatSpec.match(/^(\d+)x(\d+)$/);
    if (!match) {
      throw new Error(`Invalid format specification: ${formatSpec}. Expected format like "8x10"`);
    }

    const charsPerGroup = parseInt(match[1], 10);
    const groupsPerLine = parseInt(match[2], 10);
    const glyphs = Array.from(encoded);

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
            result.push(' ');
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
   * @returns {Uint8Array} The decoded binary data
   */
  decode(printableString) {
    if (typeof printableString !== 'string') {
      throw new Error("Input must be a string");
    }

    // Clean up the input string - remove whitespace
    // Also remove receipt emoji if present (used in disassembly mode)
    let cleanedString = printableString.replace(/[\r\n\t ]/g, '');

    // Remove receipt emoji (🧾) which is used as separator in disassembly output
    const receiptEmoji = "\u{1F9FE}";
    cleanedString = cleanedString.replace(new RegExp(receiptEmoji, 'g'), '');

    const result = [];
    let i = 0;

    // Process the string one character at a time
    while (i < cleanedString.length) {
      let matched = false;

      // Try to match longest first (up to 3 UTF-16 code units for our charset)
      // In JavaScript, some characters may be represented as surrogate pairs
      for (let len = Math.min(3, cleanedString.length - i); len >= 1; len--) {
        const sub = cleanedString.substring(i, i + len);
        const decoded = this.decodeMap.get(sub);

        if (decoded !== undefined) {
          result.push(decoded);
          i += len;
          matched = true;
          break;
        }
      }

      // If we didn't match any character in our map, skip this character
      if (!matched) {
        i++;
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
  encodeString(str) {
    const encoder = new TextEncoder();
    const bytes = encoder.encode(str);
    return this.encode(bytes);
  }

  /**
   * Decode a printable string back to a UTF-8 string
   * This is a convenience method that returns a string instead of bytes
   * @param {string} printableString - The encoded string to decode
   * @returns {string} The decoded string
   */
  decodeToString(printableString) {
    const bytes = this.decode(printableString);
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
