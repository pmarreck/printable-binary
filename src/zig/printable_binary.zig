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

// =============================================================================
// FFI Validation API
// =============================================================================

/// Whitespace handling flags for validation (bitfield)
pub const WhitespaceFlags = enum(c_uint) {
    reject_all = 0,
    allow_space = 1 << 0,
    allow_tab = 1 << 1,
    allow_lf = 1 << 2,
    allow_cr = 1 << 3,
    allow_all = 0x0F,
};

/// Result of validating a printable-binary encoded string
pub const ValidationResult = extern struct {
    is_valid: c_int, // 0 = invalid, 1 = valid
    error_position: i64, // -1 if valid, else byte offset of first invalid char
    error_codepoint: u32, // The invalid codepoint, or 0 if valid
};

/// Decode a UTF-8 sequence to a Unicode codepoint
fn decodeUtf8Codepoint(bytes: []const u8) ?u32 {
    if (bytes.len == 0) return null;

    const first = bytes[0];
    if (first < 0x80) {
        return first;
    } else if (first < 0xE0) {
        if (bytes.len < 2) return null;
        if ((bytes[1] & 0xC0) != 0x80) return null;
        return (@as(u32, first & 0x1F) << 6) | (bytes[1] & 0x3F);
    } else if (first < 0xF0) {
        if (bytes.len < 3) return null;
        if ((bytes[1] & 0xC0) != 0x80 or (bytes[2] & 0xC0) != 0x80) return null;
        return (@as(u32, first & 0x0F) << 12) | (@as(u32, bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
    } else {
        if (bytes.len < 4) return null;
        if ((bytes[1] & 0xC0) != 0x80 or (bytes[2] & 0xC0) != 0x80 or (bytes[3] & 0xC0) != 0x80) return null;
        return (@as(u32, first & 0x07) << 18) | (@as(u32, bytes[1] & 0x3F) << 12) | (@as(u32, bytes[2] & 0x3F) << 6) | (bytes[3] & 0x3F);
    }
}

/// Validate that a string contains only valid printable-binary encoded characters.
/// Returns validation result with position and codepoint of first error if invalid.
pub fn validate(input: []const u8, ws_flags: c_uint) ValidationResult {
    var i: usize = 0;

    while (i < input.len) {
        const byte = input[i];

        // Check whitespace handling
        if (byte == ' ') {
            if ((ws_flags & @intFromEnum(WhitespaceFlags.allow_space)) != 0) {
                i += 1;
                continue;
            }
        } else if (byte == '\t') {
            if ((ws_flags & @intFromEnum(WhitespaceFlags.allow_tab)) != 0) {
                i += 1;
                continue;
            }
        } else if (byte == '\n') {
            if ((ws_flags & @intFromEnum(WhitespaceFlags.allow_lf)) != 0) {
                i += 1;
                continue;
            }
        } else if (byte == '\r') {
            if ((ws_flags & @intFromEnum(WhitespaceFlags.allow_cr)) != 0) {
                i += 1;
                continue;
            }
        }

        // Determine UTF-8 sequence length
        const seq_len = utf8SeqLen(byte);
        const remaining = input.len - i;

        // Check for truncated UTF-8 sequence
        if (seq_len > remaining) {
            const codepoint = decodeUtf8Codepoint(input[i..]) orelse 0xFFFD;
            return ValidationResult{
                .is_valid = 0,
                .error_position = @intCast(i),
                .error_codepoint = codepoint,
            };
        }

        const seq = input[i .. i + seq_len];

        // Validate UTF-8 continuation bytes
        for (seq[1..]) |cont_byte| {
            if ((cont_byte & 0xC0) != 0x80) {
                const codepoint = decodeUtf8Codepoint(seq) orelse 0xFFFD;
                return ValidationResult{
                    .is_valid = 0,
                    .error_position = @intCast(i),
                    .error_codepoint = codepoint,
                };
            }
        }

        // Look up in decode map
        if (decodeLookup(seq) == null) {
            const codepoint = decodeUtf8Codepoint(seq) orelse 0xFFFD;
            return ValidationResult{
                .is_valid = 0,
                .error_position = @intCast(i),
                .error_codepoint = codepoint,
            };
        }

        i += seq_len;
    }

    return ValidationResult{
        .is_valid = 1,
        .error_position = -1,
        .error_codepoint = 0,
    };
}

/// C ABI export for validation function
export fn pb_validate(input: [*]const u8, input_len: usize, ws_flags: c_uint) callconv(.c) ValidationResult {
    const slice = if (input_len > 0) input[0..input_len] else &[_]u8{};
    return validate(slice, ws_flags);
}

// =============================================================================
// FFI Encode/Decode/Format API
// =============================================================================

/// Encode flags for C ABI (matches EncodeOptions)
pub const EncodeFlags = enum(c_uint) {
    none = 0,
    preserve_spaces = 1 << 0,
    preserve_tabs = 1 << 1,
    preserve_crlf = 1 << 2,
    preserve_all_whitespace = 0x07,
};

/// Decode flags for C ABI (matches DecodeOptions)
pub const DecodeFlags = enum(c_uint) {
    none = 0,
    spaces_mode = 1 << 0,
    strip_whitespace = 1 << 1,
};

/// Result structure for FFI functions that return allocated data
pub const FFIResult = extern struct {
    data: ?[*]u8, // NULL on error
    len: usize, // Length of data, or 0 on error
    error_code: c_int, // 0 = success, non-zero = error
};

// Use page allocator for FFI - simple and doesn't require libc
const ffi_allocator = std.heap.page_allocator;

/// Free memory allocated by pb_encode, pb_decode, or pb_format
export fn pb_free(ptr: ?[*]u8, len: usize) callconv(.c) void {
    if (ptr) |p| {
        ffi_allocator.free(p[0..len]);
    }
}

/// C ABI export for encode function
/// Caller must call pb_free() on result.data when done
export fn pb_encode(
    input: [*]const u8,
    input_len: usize,
    flags: c_uint,
    preserve_chars: ?[*]const u8,
    preserve_chars_len: usize,
) callconv(.c) FFIResult {
    const input_slice = if (input_len > 0) input[0..input_len] else &[_]u8{};
    const preserve_slice = if (preserve_chars != null and preserve_chars_len > 0)
        preserve_chars.?[0..preserve_chars_len]
    else
        &[_]u8{};

    const options = EncodeOptions{
        .spaces = (flags & @intFromEnum(EncodeFlags.preserve_spaces)) != 0,
        .tabs = (flags & @intFromEnum(EncodeFlags.preserve_tabs)) != 0,
        .crlf = (flags & @intFromEnum(EncodeFlags.preserve_crlf)) != 0,
        .preserve_chars = preserve_slice,
    };

    const result = encode(ffi_allocator, input_slice, options) catch {
        return FFIResult{ .data = null, .len = 0, .error_code = 1 };
    };

    return FFIResult{
        .data = result.ptr,
        .len = result.len,
        .error_code = 0,
    };
}

/// C ABI export for decode function
/// Caller must call pb_free() on result.data when done
export fn pb_decode(
    input: [*]const u8,
    input_len: usize,
    flags: c_uint,
) callconv(.c) FFIResult {
    const input_slice = if (input_len > 0) input[0..input_len] else &[_]u8{};

    const options = DecodeOptions{
        .spaces = (flags & @intFromEnum(DecodeFlags.spaces_mode)) != 0,
        .strip_whitespace = (flags & @intFromEnum(DecodeFlags.strip_whitespace)) != 0,
    };

    const result = decode(ffi_allocator, input_slice, options) catch {
        return FFIResult{ .data = null, .len = 0, .error_code = 1 };
    };

    return FFIResult{
        .data = result.ptr,
        .len = result.len,
        .error_code = 0,
    };
}

/// C ABI export for format function
/// Caller must call pb_free() on result.data when done
export fn pb_format(
    input: [*]const u8,
    input_len: usize,
    group_size: usize,
    groups_per_line: usize,
    use_tabs: c_int,
) callconv(.c) FFIResult {
    const input_slice = if (input_len > 0) input[0..input_len] else &[_]u8{};

    const options = FormatOptions{
        .group_size = if (group_size > 0) group_size else 8,
        .groups_per_line = if (groups_per_line > 0) groups_per_line else 10,
        .use_tabs = use_tabs != 0,
    };

    const result = format(ffi_allocator, input_slice, options) catch {
        return FFIResult{ .data = null, .len = 0, .error_code = 1 };
    };

    return FFIResult{
        .data = result.ptr,
        .len = result.len,
        .error_code = 0,
    };
}

/// Get the mapping for a byte value (returns pointer to static data, do not free)
export fn pb_get_mapping(byte: u8) callconv(.c) [*]const u8 {
    return character_map[byte].ptr;
}

/// Get the length of a mapping for a byte value
export fn pb_get_mapping_len(byte: u8) callconv(.c) usize {
    return character_map[byte].len;
}
