/**
 * PrintableBinary JavaScript Implementation
 * Encodes arbitrary binary data into human-readable UTF-8 strings and decodes them back
 *
 * This is a browser and Deno compatible implementation of the PrintableBinary algorithm.
 * It can be used as an ES module or included directly in HTML.
 */

class PrintableBinary {
  constructor() {
    this.encodeMap = new Map(); // number (0-255) -> string (UTF-8 character)
    this.decodeMap = new Map(); // string (UTF-8 character) -> number (0-255)
    this.buildMaps();
  }

  /**
   * Helper to define an encoding and its reverse mapping
   */
  defChar(byteVal, utf8Str) {
    this.encodeMap.set(byteVal, utf8Str);
    this.decodeMap.set(utf8Str, byteVal);
  }

  /**
   * Build the encoding and decoding maps
   */
  buildMaps() {
    // Control Characters (0-31)
    this.defChar(0, "\u2205");   // ∅ (U+2205)
    this.defChar(1, "\u00AF");   // ¯ (U+00AF)
    this.defChar(2, "\u00AB");   // « (U+00AB)
    this.defChar(3, "\u00BB");   // » (U+00BB)
    this.defChar(4, "\u03DF");   // ϟ (U+03DF)
    this.defChar(5, "\u00BF");   // ¿ (U+00BF)
    this.defChar(6, "\u00A1");   // ¡ (U+00A1)
    this.defChar(7, "\u00AA");   // ª (U+00AA)
    this.defChar(8, "\u232B");   // ⌫ (U+232B)
    this.defChar(9, "\u21E5");   // ⇥ (U+21E5)
    this.defChar(10, "\u21E9");  // ⇩ (U+21E9)
    this.defChar(11, "\u21A7");  // ↧ (U+21A7)
    this.defChar(12, "\u00A7");  // § (U+00A7)
    this.defChar(13, "\u23CE");  // ⏎ (U+23CE)
    this.defChar(14, "\u022F");  // ȯ (U+022F)
    this.defChar(15, "\u0298");  // ʘ (U+0298)
    this.defChar(16, "\u0194");  // Ɣ (U+0194)
    this.defChar(17, "\u00B9");  // ¹ (U+00B9)
    this.defChar(18, "\u00B2");  // ² (U+00B2)
    this.defChar(19, "\u00BA");  // º (U+00BA)
    this.defChar(20, "\u00B3");  // ³ (U+00B3)
    this.defChar(21, "\u00B5");  // µ (U+00B5)
    this.defChar(22, "\u0268");  // ɨ (U+0268)
    this.defChar(23, "\u00AC");  // ¬ (U+00AC)
    this.defChar(24, "\u00A9");  // © (U+00A9)
    this.defChar(25, "\u00A6");  // ¦ (U+00A6)
    this.defChar(26, "\u01B5");  // Ƶ (U+01B5)
    this.defChar(27, "\u238B");  // ⎋ (U+238B)
    this.defChar(28, "\u039E");  // Ξ (U+039E)
    this.defChar(29, "\u01C1");  // ǁ (U+01C1)
    this.defChar(30, "\u01C0");  // ǀ (U+01C0)
    this.defChar(31, "\u00B6");  // ¶ (U+00B6)

    // Special ASCII characters (shell-safe encodings)
    this.defChar(32, "\u2423");  // ␣ (U+2423)
    this.defChar(33, "\uFE57");  // ﹗ (U+FE57) Small Exclamation Mark
    this.defChar(34, "\u02F5");  // ˵ (U+02F5)
    this.defChar(35, "\u266F");  // ♯ (U+266F) Music Sharp Sign
    this.defChar(36, "\uFE69");  // ﹩ (U+FE69) Small Dollar Sign
    this.defChar(37, "\uFE6A");  // ﹪ (U+FE6A) Small Percent Sign
    this.defChar(38, "\uFE60");  // ﹠ (U+FE60) Small Ampersand
    this.defChar(39, "\u02BC");  // ʼ (U+02BC MODIFIER LETTER APOSTROPHE)
    this.defChar(40, "\u2768");  // ❨ (U+2768) Medium Left Parenthesis Ornament
    this.defChar(41, "\u2769");  // ❩ (U+2769) Medium Right Parenthesis Ornament
    this.defChar(42, "\uFE61");  // ﹡ (U+FE61) Small Asterisk
    this.defChar(43, "\uFE62");  // ﹢ (U+FE62) Small Plus Sign
    this.defChar(45, "\uFE63");  // ﹣ (U+FE63) Small Hyphen-Minus
    this.defChar(47, "\u2044");  // ⁄ (U+2044) Fraction Slash
    this.defChar(58, "\uFE55");  // ﹕ (U+FE55) Small Colon
    this.defChar(59, "\uFE54");  // ﹔ (U+FE54) Small Semicolon
    this.defChar(61, "\uFE66");  // ﹦ (U+FE66) Small Equals Sign
    this.defChar(63, "\uFE56");  // ﹖ (U+FE56) Small Question Mark
    this.defChar(64, "\uFE6B");  // ﹫ (U+FE6B) Small Commercial At
    this.defChar(91, "\u27E6");  // ⟦ (U+27E6) Mathematical Left White Square Bracket
    this.defChar(92, "\u29F9");  // ⧹ (U+29F9) Big Reverse Solidus
    this.defChar(93, "\u27E7");  // ⟧ (U+27E7) Mathematical Right White Square Bracket
    this.defChar(96, "\u02CB");  // ˋ (U+02CB) Modifier Letter Grave Accent
    this.defChar(123, "\u2774"); // ❴ (U+2774) Medium Left Curly Bracket Ornament
    this.defChar(124, "\u2223"); // ∣ (U+2223) Divides
    this.defChar(125, "\u2775"); // ❵ (U+2775) Medium Right Curly Bracket Ornament
    this.defChar(126, "\u02DC"); // ˜ (U+02DC) Small Tilde
    this.defChar(127, "\u2326"); // ⌦ (U+2326)

    // Regular ASCII characters (remaining ones not specially encoded above)
    for (let i = 33; i <= 126; i++) {
      if (!this.encodeMap.has(i)) {
        this.defChar(i, String.fromCharCode(i));
      }
    }

    // Extended bytes (128-255)
    // Special cases first
    this.defChar(152, "\u014C"); // Ō (U+014C)
    this.defChar(184, "\u014F"); // ŏ (U+014F)

    // Pattern-based characters for the rest
    for (let i = 128; i <= 255; i++) {
      if (i !== 152 && i !== 184) {
        if (i < 192) {
          // Extended ASCII 128-191
          // Encoded as Latin-1 Supplement characters
          const char = String.fromCharCode(0xC3, i);
          this.defChar(i, char);
        } else {
          // Extended ASCII 192-255
          // Encoded as Latin Extended-A characters
          const char = String.fromCharCode(0xC4, i - 192 + 128);
          this.defChar(i, char);
        }
      }
    }
  }

  /**
   * Encode binary data (Uint8Array or ArrayBuffer) to printable UTF-8 string
   * @param {Uint8Array|ArrayBuffer} binaryData - The binary data to encode
   * @returns {string} The encoded printable string
   */
  encode(binaryData) {
    // Convert ArrayBuffer to Uint8Array if needed
    if (binaryData instanceof ArrayBuffer) {
      binaryData = new Uint8Array(binaryData);
    }

    if (!(binaryData instanceof Uint8Array)) {
      throw new Error("Input must be a Uint8Array or ArrayBuffer");
    }

    const result = [];
    for (let i = 0; i < binaryData.length; i++) {
      const byte = binaryData[i];
      const encoded = this.encodeMap.get(byte);
      if (encoded !== undefined) {
        result.push(encoded);
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
