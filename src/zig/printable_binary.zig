//! PrintableBinary - Encode binary data as printable UTF-8 and decode it back
//!
//! This is the core library with no I/O dependencies. It provides pure
//! encoding and decoding functions that can be integrated into any Zig project.
//!
//! ## Usage as a library
//! ```zig
//! const pb = @import("printable_binary");
//! 
//! // Encode
//! const encoded = try pb.encode(allocator, input_bytes, .{});
//! defer allocator.free(encoded);
//!
//! // Decode  
//! const decoded = try pb.decode(allocator, encoded_string, .{});
//! defer allocator.free(decoded);
//! ```

const std = @import("std");

/// Character map for encoding bytes 0-255 to UTF-8 sequences.
/// Index corresponds to byte value.
pub const character_map = [256][]const u8{
    "·",
    "¯",
    "«",
    "»",
    "ϟ",
    "¿",
    "¡",
    "ª",
    "⌫",
    "⇥",
    "¶",
    "↧",
    "§",
    "⏎",
    "ȯ",
    "ʘ",
    "Ɣ",
    "¹",
    "²",
    "º",
    "³",
    "µ",
    "ɨ",
    "⏹",
    "©",
    "¦",
    "Ƶ",
    "⎋",
    "Ξ",
    "ǁ",
    "ǀ",
    "¬",
    "␣",
    "ǃ",
    "˵",
    "♯",
    "Ꞩ",
    "‰",
    "⅋",
    "ʼ",
    "❨",
    "❩",
    "⁎",
    "⨦",
    "٫",
    "˗",
    ".",
    "⁄",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "꞉",
    ";",
    "˂",
    "꞊",
    "˃",
    "Ɂ",
    "@",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "⟦",
    "⧷",
    "⟧",
    "^",
    "_",
    "ˋ",
    "a",
    "b",
    "c",
    "d",
    "e",
    "f",
    "g",
    "h",
    "i",
    "j",
    "k",
    "l",
    "m",
    "n",
    "o",
    "p",
    "q",
    "r",
    "s",
    "t",
    "u",
    "v",
    "w",
    "x",
    "y",
    "z",
    "❴",
    "∣",
    "❵",
    "˜",
    "⌦",
    "ă",
    "Ă",
    "Ǎ",
    "ǟ",
    "Ǟ",
    "ȧ",
    "Ȧ",
    "ǡ",
    "ƀ",
    "Ƀ",
    "Ɓ",
    "ƃ",
    "Ƃ",
    "ć",
    "Ć",
    "ĉ",
    "Ĉ",
    "č",
    "Č",
    "ċ",
    "Ċ",
    "ď",
    "Ď",
    "Đ",
    "ȸ",
    "Ɗ",
    "ƌ",
    "Ƌ",
    "ȡ",
    "ĕ",
    "Ĕ",
    "Ě",
    "ė",
    "ȩ",
    "Ȩ",
    "ƒ",
    "Ƒ",
    "ǵ",
    "Ǵ",
    "ğ",
    "Ğ",
    "ǧ",
    "Ǧ",
    "ḡ",
    "Ḡ",
    "ĥ",
    "Ĥ",
    "ȟ",
    "Ȟ",
    "ƕ",
    "Ƕ",
    "ĭ",
    "Ĭ",
    "Ǐ",
    "İ",
    "ȉ",
    "ȋ",
    "ĵ",
    "Ĵ",
    "ǰ",
    "ǩ",
    "Ǩ",
    "ķ",
    "Ķ",
    "ƙ",
    "Ƙ",
    "ĺ",
    "Ĺ",
    "ľ",
    "Ľ",
    "ƚ",
    "Ƚ",
    "Ń",
    "ǹ",
    "Ň",
    "ņ",
    "Ņ",
    "ȵ",
    "ŏ",
    "Ŏ",
    "Ǒ",
    "ȫ",
    "Ȫ",
    "ȱ",
    "ƥ",
    "Ƥ",
    "ȹ",
    "ɋ",
    "ŕ",
    "Ŕ",
    "ř",
    "Ř",
    "ŗ",
    "Ŗ",
    "ś",
    "Ś",
    "š",
    "Š",
    "ş",
    "Ş",
    "ť",
    "Ť",
    "ţ",
    "Ţ",
    "ț",
    "Ț",
    "ŭ",
    "Ŭ",
    "Ǔ",
    "ű",
    "ȕ",
    "Ʉ",
    "Ṿ",
    "Ʋ",
    "ŵ",
    "Ŵ",
    "ŷ",
    "Ŷ",
    "Ÿ",
    "ȳ",
    "ƴ",
    "Ƴ",
    "ź",
    "Ź",
    "ž",
    "Ž",
    "ż",
    "Ż",
};

/// Encoding options
pub const EncodeOptions = struct {
    /// Preserve literal spaces (don't encode to ␣)
    spaces: bool = false,
    /// Preserve literal tabs (don't encode to ⇥)
    tabs: bool = false,
    /// Preserve literal CR/LF (don't encode to ⏎/↧)
    crlf: bool = false,
    /// Set of bytes to preserve as-is (not encoded)
    preserve_chars: []const u8 = &.{},
};

/// Decoding options
pub const DecodeOptions = struct {
    /// Treat literal spaces as data (decode them to space bytes)
    spaces: bool = false,
    /// Strip whitespace before decoding (for formatted input)
    strip_whitespace: bool = false,
};

/// Format options for grouping encoded output
pub const FormatOptions = struct {
    /// Characters per group
    group_size: usize = 8,
    /// Groups per line
    groups_per_line: usize = 10,
    /// Use tabs instead of spaces between groups
    use_tabs: bool = false,
};

// Decode map built at comptime for O(log n) reverse lookup
const DecodeEntry = struct {
    key: u64,
    value: u8,
};

fn buildDecodeMap() [256]DecodeEntry {
    @setEvalBranchQuota(100000);
    var entries: [256]DecodeEntry = undefined;
    for (0..256) |i| {
        const utf8 = character_map[i];
        var key: u64 = 0;
        for (utf8) |byte| {
            key = (key << 8) | byte;
        }
        entries[i] = .{ .key = key, .value = @intCast(i) };
    }
    // Insertion sort by key for binary search
    for (0..256) |i| {
        var j = i;
        while (j > 0 and entries[j].key < entries[j - 1].key) {
            const tmp = entries[j];
            entries[j] = entries[j - 1];
            entries[j - 1] = tmp;
            j -= 1;
        }
    }
    return entries;
}

const decode_map = buildDecodeMap();

/// Get UTF-8 sequence length from first byte
pub fn utf8SeqLen(first_byte: u8) u3 {
    if (first_byte < 0x80) return 1;
    if (first_byte < 0xE0) return 2;
    if (first_byte < 0xF0) return 3;
    return 4;
}

fn makeKey(bytes: []const u8) u64 {
    var key: u64 = 0;
    for (bytes) |b| {
        key = (key << 8) | b;
    }
    return key;
}

fn decodeLookup(bytes: []const u8) ?u8 {
    const key = makeKey(bytes);
    var left: usize = 0;
    var right: usize = 256;
    while (left < right) {
        const mid = left + (right - left) / 2;
        if (decode_map[mid].key == key) {
            return decode_map[mid].value;
        } else if (decode_map[mid].key < key) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return null;
}

/// Encode binary data to printable UTF-8.
/// Caller owns the returned slice and must free it with the same allocator.
pub fn encode(allocator: std.mem.Allocator, input: []const u8, options: EncodeOptions) ![]u8 {
    var result: std.ArrayListUnmanaged(u8) = .{};
    errdefer result.deinit(allocator);

    // Build preserve set
    var preserve_set = [_]bool{false} ** 256;
    for (options.preserve_chars) |c| {
        preserve_set[c] = true;
    }

    for (input) |byte| {
        if (options.spaces and byte == ' ') {
            try result.append(allocator, ' ');
        } else if (options.tabs and byte == '\t') {
            try result.append(allocator, '\t');
        } else if (options.crlf and (byte == '\n' or byte == '\r')) {
            try result.append(allocator, byte);
        } else if (preserve_set[byte]) {
            try result.append(allocator, byte);
        } else {
            try result.appendSlice(allocator, character_map[byte]);
        }
    }

    return result.toOwnedSlice(allocator);
}

/// Decode printable UTF-8 back to binary data.
/// Unrecognized UTF-8 characters pass through unchanged.
/// Caller owns the returned slice and must free it with the same allocator.
pub fn decode(allocator: std.mem.Allocator, input: []const u8, options: DecodeOptions) ![]u8 {
    var result: std.ArrayListUnmanaged(u8) = .{};
    errdefer result.deinit(allocator);

    // Optionally strip whitespace
    var cleaned: []const u8 = undefined;
    var cleaned_buf: ?[]u8 = null;
    defer if (cleaned_buf) |buf| allocator.free(buf);

    if (options.strip_whitespace) {
        var clean_list: std.ArrayListUnmanaged(u8) = .{};
        errdefer clean_list.deinit(allocator);
        for (input) |c| {
            const skip = if (options.spaces)
                (c == '\n' or c == '\r' or c == '\t')
            else
                (c == '\n' or c == '\r' or c == '\t' or c == ' ');
            if (!skip) {
                try clean_list.append(allocator, c);
            }
        }
        cleaned_buf = try clean_list.toOwnedSlice(allocator);
        cleaned = cleaned_buf.?;
    } else {
        cleaned = input;
    }

    var i: usize = 0;
    while (i < cleaned.len) {
        // Handle literal spaces in spaces mode
        if (options.spaces and cleaned[i] == ' ') {
            try result.append(allocator, ' ');
            i += 1;
            continue;
        }

        const seq_len = utf8SeqLen(cleaned[i]);
        const remaining = cleaned.len - i;
        const actual_len: usize = if (seq_len > remaining) remaining else seq_len;

        // Try to match, longest first
        var matched = false;
        var try_len = actual_len;
        while (try_len >= 1) : (try_len -= 1) {
            if (decodeLookup(cleaned[i .. i + try_len])) |byte| {
                try result.append(allocator, byte);
                i += try_len;
                matched = true;
                break;
            }
        }

        // Pass through unrecognized UTF-8 characters
        if (!matched) {
            for (0..actual_len) |j| {
                try result.append(allocator, cleaned[i + j]);
            }
            i += actual_len;
        }
    }

    return result.toOwnedSlice(allocator);
}

/// Format encoded output into groups for readability.
/// Caller owns the returned slice and must free it with the same allocator.
pub fn format(allocator: std.mem.Allocator, input: []const u8, options: FormatOptions) ![]u8 {
    var result: std.ArrayListUnmanaged(u8) = .{};
    errdefer result.deinit(allocator);

    const separator: u8 = if (options.use_tabs) '\t' else ' ';
    var char_count: usize = 0;
    var i: usize = 0;

    while (i < input.len) {
        const seq_len = utf8SeqLen(input[i]);
        const remaining = input.len - i;
        const actual_len: usize = if (seq_len > remaining) remaining else seq_len;

        try result.appendSlice(allocator, input[i .. i + actual_len]);
        char_count += 1;
        i += actual_len;

        if (char_count % options.group_size == 0 and i < input.len) {
            if ((char_count / options.group_size) % options.groups_per_line == 0) {
                try result.append(allocator, '\n');
            } else {
                try result.append(allocator, separator);
            }
        }
    }

    return result.toOwnedSlice(allocator);
}

/// Get the mapping for a specific byte value
pub fn getMapping(byte: u8) []const u8 {
    return character_map[byte];
}

/// Comptime verification that all 256 mappings exist and are valid UTF-8
pub fn verifyMappings() bool {
    @setEvalBranchQuota(100000);
    for (0..256) |i| {
        const mapping = character_map[i];
        if (mapping.len == 0) return false;
        // Verify it's valid UTF-8
        if (!std.unicode.utf8ValidateSlice(mapping)) return false;
    }
    return true;
}

comptime {
    if (!verifyMappings()) {
        @compileError("Character map validation failed - not all 256 bytes have valid UTF-8 mappings");
    }
}
