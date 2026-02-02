//! PrintableBinary CLI - Thin I/O adapter around the core library
//!
//! This is a hexagonal architecture adapter that handles:
//! - Command line argument parsing
//! - File/stdin reading
//! - Stdout/stderr writing
//! - Exit codes
//!
//! All encoding/decoding logic is in the core library.

const std = @import("std");
const pb = @import("printable_binary");

const Options = struct {
    decode_mode: bool = false,
    passthrough_mode: bool = false,
    spaces_mode: bool = false,
    tabs_mode: bool = false,
    crlf_mode: bool = false,
    strip_whitespace: bool = false,
    format_mode: bool = false,
    format_group: usize = 8,
    format_groups_per_line: usize = 10,
    help_mode: bool = false,
    mappings_mode: MappingsMode = .none,
    input_file: ?[]const u8 = null,
    preserve_chars: ?[]const u8 = null, // null = not set, allocated if set
};

const MappingsMode = enum { none, table, json, csv };

// ============================================================================
// I/O Adapters (Zig 0.15.2 buffered I/O)
// ============================================================================

fn readInput(allocator: std.mem.Allocator, file_path: ?[]const u8) ![]u8 {
    if (file_path) |path| {
        if (!std.mem.eql(u8, path, "-")) {
            const file = try std.fs.cwd().openFile(path, .{});
            defer file.close();
            return try file.readToEndAlloc(allocator, std.math.maxInt(usize));
        }
    }
    return try std.fs.File.stdin().readToEndAlloc(allocator, std.math.maxInt(usize));
}

fn writeOutput(data: []const u8, to_stderr: bool) !void {
    var buf: [4096]u8 = undefined;
    if (to_stderr) {
        var w = std.fs.File.stderr().writer(&buf);
        try w.interface.writeAll(data);
        try w.interface.flush();
    } else {
        var w = std.fs.File.stdout().writer(&buf);
        try w.interface.writeAll(data);
        try w.interface.flush();
    }
}

fn writeStats(comptime fmt: []const u8, args: anytype) void {
    // Check if stats output is muted
    const mute_env = std.posix.getenv("PRINTABLE_BINARY_MUTE_STATS");
    if (mute_env != null and mute_env.?.len > 0 and mute_env.?[0] == '1') {
        return;
    }
    // Use unbuffered direct write for stderr messages
    var msg_buf: [1024]u8 = undefined;
    const msg = std.fmt.bufPrint(&msg_buf, fmt, args) catch return;
    _ = std.posix.write(std.posix.STDERR_FILENO, msg) catch {};
}

// ============================================================================
// Argument Parsing (I/O boundary - reads from OS)
// ============================================================================

fn parseFormatSpec(spec: []const u8) !struct { group: usize, per_line: usize } {
    var it = std.mem.splitScalar(u8, spec, 'x');
    const group_str = it.next() orelse return error.InvalidFormat;
    const per_line_str = it.next() orelse return error.InvalidFormat;
    if (it.next() != null) return error.InvalidFormat;

    const group = std.fmt.parseInt(usize, group_str, 10) catch return error.InvalidFormat;
    const per_line = std.fmt.parseInt(usize, per_line_str, 10) catch return error.InvalidFormat;

    if (group == 0 or per_line == 0) return error.InvalidFormat;
    return .{ .group = group, .per_line = per_line };
}

fn parseArgs(allocator: std.mem.Allocator) !Options {
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    var opts = Options{};
    var i: usize = 1;

    while (i < args.len) : (i += 1) {
        const arg = args[i];

        if (std.mem.eql(u8, arg, "--")) {
            if (i + 1 < args.len) opts.input_file = try allocator.dupe(u8, args[i + 1]);
            break;
        }

        if (arg.len == 0 or arg[0] != '-' or std.mem.eql(u8, arg, "-")) {
            if (opts.input_file != null) return error.MultipleInputFiles;
            opts.input_file = try allocator.dupe(u8, arg);
            continue;
        }

        if (arg.len > 1 and arg[1] == '-') {
            const name = arg[2..];
            if (std.mem.startsWith(u8, name, "format=")) {
                const parsed = try parseFormatSpec(name[7..]);
                opts.format_mode = true;
                opts.format_group = parsed.group;
                opts.format_groups_per_line = parsed.per_line;
            } else if (std.mem.startsWith(u8, name, "preserve=")) {
                opts.preserve_chars = try allocator.dupe(u8, name[9..]);
            } else if (std.mem.eql(u8, name, "decode")) {
                opts.decode_mode = true;
            } else if (std.mem.eql(u8, name, "passthrough")) {
                opts.passthrough_mode = true;
            } else if (std.mem.eql(u8, name, "spaces")) {
                opts.spaces_mode = true;
            } else if (std.mem.eql(u8, name, "tabs")) {
                opts.tabs_mode = true;
            } else if (std.mem.eql(u8, name, "crlf")) {
                opts.crlf_mode = true;
            } else if (std.mem.eql(u8, name, "preserve-whitespace")) {
                opts.spaces_mode = true;
                opts.tabs_mode = true;
                opts.crlf_mode = true;
            } else if (std.mem.eql(u8, name, "strip-whitespace")) {
                opts.strip_whitespace = true;
            } else if (std.mem.eql(u8, name, "format")) {
                opts.format_mode = true;
            } else if (std.mem.eql(u8, name, "mappings")) {
                opts.mappings_mode = .table;
            } else if (std.mem.eql(u8, name, "mappings-json")) {
                opts.mappings_mode = .json;
            } else if (std.mem.eql(u8, name, "mappings-csv")) {
                opts.mappings_mode = .csv;
            } else if (std.mem.eql(u8, name, "help")) {
                opts.help_mode = true;
            } else {
                return error.UnknownOption;
            }
        } else {
            // Short options
            var j: usize = 1;
            while (j < arg.len) : (j += 1) {
                switch (arg[j]) {
                    'd' => opts.decode_mode = true,
                    'p' => opts.passthrough_mode = true,
                    's' => opts.spaces_mode = true,
                    't' => opts.tabs_mode = true,
                    'n' => opts.crlf_mode = true,
                    'w' => {
                        opts.spaces_mode = true;
                        opts.tabs_mode = true;
                        opts.crlf_mode = true;
                    },
                    'S' => opts.strip_whitespace = true,
                    'h' => opts.help_mode = true,
                    'f' => {
                        if (j + 1 < arg.len) {
                            const parsed = parseFormatSpec(arg[j + 1 ..]) catch return error.InvalidFormat;
                            opts.format_mode = true;
                            opts.format_group = parsed.group;
                            opts.format_groups_per_line = parsed.per_line;
                            break;
                        } else {
                            opts.format_mode = true;
                        }
                    },
                    'P' => {
                        if (j + 1 < arg.len) {
                            opts.preserve_chars = try allocator.dupe(u8, arg[j + 1 ..]);
                            break;
                        } else if (i + 1 < args.len) {
                            i += 1;
                            opts.preserve_chars = try allocator.dupe(u8, args[i]);
                        } else {
                            return error.MissingValue;
                        }
                    },
                    else => return error.UnknownOption,
                }
            }
        }
    }
    return opts;
}

// ============================================================================
// Output Formatting (uses core library, writes to I/O)
// ============================================================================

fn printUsage() void {
    const help =
        \\PrintableBinary Zig - Encode binary data as printable UTF-8 and decode it back
        \\
        \\Usage: printable_binary_zig [options] [file]
        \\
        \\Options:
        \\  -d, --decode       Decode mode (default is encode mode)
        \\  -p, --passthrough  Pass input to stdout unchanged, send encoded data to stderr
        \\
        \\Encode options (preserve literal characters instead of encoding):
        \\  -s, --spaces       Preserve literal spaces (don't encode to visible glyph)
        \\  -t, --tabs         Preserve literal tabs (don't encode to visible glyph)
        \\  -n, --crlf         Preserve literal CR/LF (don't encode to visible glyph)
        \\  -w, --preserve-whitespace  Shorthand for -stn (preserve all whitespace)
        \\  -P, --preserve=CHARS       Preserve specific characters
        \\
        \\Decode options:
        \\  -S, --strip-whitespace     Strip whitespace before decoding (for formatted input)
        \\
        \\Format and output options:
        \\  -f[=NxM], --format[=NxM]   Format output in groups
        \\                              Default: 8x10 (groups of 8 chars, 10 groups per line)
        \\  --mappings         Show the byte-to-character mapping table
        \\  --mappings-json    Output mappings as JSON
        \\  --mappings-csv     Output mappings as CSV
        \\  -h, --help         Show this help
        \\
        \\If no file is specified, input is read from stdin.
        \\
    ;
    var buf: [4096]u8 = undefined;
    var w = std.fs.File.stderr().writer(&buf);
    w.interface.writeAll(help) catch {};
    w.interface.flush() catch {};
}

// ASCII names for control characters and special bytes
const ascii_names = [_][]const u8{
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS", "TAB", "LF", "VT", "FF", "CR", "SO", "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US",
    "SPACE", "!", "\"", "#", "$", "%", "&", "'",
    "(", ")", "*", "+", ",", "-", ".", "/",
    "0", "1", "2", "3", "4", "5", "6", "7",
    "8", "9", ":", ";", "<", "=", ">", "?",
    "@", "A", "B", "C", "D", "E", "F", "G",
    "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W",
    "X", "Y", "Z", "[", "\\", "]", "^", "_",
    "`", "a", "b", "c", "d", "e", "f", "g",
    "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w",
    "x", "y", "z", "{", "|", "}", "~", "DEL",
};

fn writeStdout(data: []const u8) void {
    _ = std.posix.write(std.posix.STDOUT_FILENO, data) catch {};
}

fn printMappings(mode: MappingsMode) !void {
    var line_buf: [256]u8 = undefined;
    switch (mode) {
        .table => {
            writeStdout("Byte   Dec   ASCII        Mapping\n");
            for (0..256) |i| {
                const ascii_name = if (i < 128) ascii_names[i] else "";
                const line = std.fmt.bufPrint(&line_buf, "0x{X:0>2}   {d:<5} {s:<12} {s}\n", .{
                    i, i, ascii_name, pb.character_map[i],
                }) catch continue;
                writeStdout(line);
            }
        },
        .json => {
            writeStdout("[\n");
            for (0..256) |i| {
                const raw_ascii = if (i < 128) ascii_names[i] else "";
                // Escape special JSON characters
                const ascii_escaped = if (std.mem.eql(u8, raw_ascii, "\""))
                    "\\\""
                else if (std.mem.eql(u8, raw_ascii, "\\"))
                    "\\\\"
                else
                    raw_ascii;
                const line = std.fmt.bufPrint(&line_buf, "  {{\"byte\": {d}, \"ascii\": \"{s}\", \"mapping\": \"{s}\"}}{s}\n", .{
                    i, ascii_escaped, pb.character_map[i], if (i < 255) "," else "",
                }) catch continue;
                writeStdout(line);
            }
            writeStdout("]\n");
        },
        .csv => {
            writeStdout("byte,hex,dec,ascii,mapping\n");
            for (0..256) |i| {
                const raw_ascii = if (i < 128) ascii_names[i] else "";
                const line = std.fmt.bufPrint(&line_buf, "{d},0x{X:0>2},{d},\"{s}\",\"{s}\"\n", .{
                    i, i, i, raw_ascii, pb.character_map[i],
                }) catch continue;
                writeStdout(line);
            }
        },
        .none => {},
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const opts = parseArgs(allocator) catch |err| {
        switch (err) {
            error.UnknownOption => writeStats("Error: Unknown option\n", .{}),
            error.InvalidFormat => writeStats("Error: Invalid format specification\n", .{}),
            error.MissingValue => writeStats("Error: Missing value for option\n", .{}),
            error.MultipleInputFiles => writeStats("Error: Multiple input files specified\n", .{}),
            else => writeStats("Error parsing arguments: {}\n", .{err}),
        }
        std.process.exit(1);
    };
    // Free allocated strings on exit
    defer if (opts.input_file) |f| allocator.free(f);
    defer if (opts.preserve_chars) |p| allocator.free(p);

    if (opts.help_mode) {
        printUsage();
        return;
    }

    if (opts.mappings_mode != .none) {
        try printMappings(opts.mappings_mode);
        return;
    }

    // Read input (I/O boundary)
    const input = readInput(allocator, opts.input_file) catch |err| {
        writeStats("Error reading input: {}\n", .{err});
        std.process.exit(1);
    };
    defer allocator.free(input);

    if (opts.decode_mode) {
        // Decode mode - call core library
        if (opts.passthrough_mode) {
            writeStats("Warning: --passthrough ignored in decode mode\n", .{});
        }

        // Warn about spaces after newlines in spaces + strip-whitespace mode
        // (two consecutive spaces after newline suggests indentation being treated as data)
        if (opts.spaces_mode and opts.strip_whitespace) {
            var prev1: u8 = 0;
            var prev2: u8 = 0;
            for (input) |c| {
                if (c == ' ' and prev1 == ' ' and (prev2 == '\n' or prev2 == '\r')) {
                    writeStats("Warning: spaces after newline are treated as data in --spaces mode\n", .{});
                    break;
                }
                prev2 = prev1;
                prev1 = c;
            }
        }

        const decoded = pb.decode(allocator, input, .{
            .spaces = opts.spaces_mode,
            .strip_whitespace = opts.strip_whitespace,
        }) catch |err| {
            writeStats("Decode error: {}\n", .{err});
            std.process.exit(1);
        };
        defer allocator.free(decoded);

        writeStats("Decoding mode: Input size is {d} bytes\n", .{input.len});
        writeStats("Decoded result size: {d} bytes\n", .{decoded.len});
        try writeOutput(decoded, false);
    } else {
        // Encode mode - call core library
        if (opts.passthrough_mode) {
            try writeOutput(input, false);
        }

        const encoded = pb.encode(allocator, input, .{
            .spaces = opts.spaces_mode,
            .tabs = opts.tabs_mode,
            .crlf = opts.crlf_mode,
            .preserve_chars = opts.preserve_chars orelse &.{},
        }) catch |err| {
            writeStats("Encode error: {}\n", .{err});
            std.process.exit(1);
        };
        defer allocator.free(encoded);

        var output = encoded;
        var formatted: ?[]u8 = null;
        defer if (formatted) |f| allocator.free(f);

        if (opts.format_mode) {
            formatted = pb.format(allocator, encoded, .{
                .group_size = opts.format_group,
                .groups_per_line = opts.format_groups_per_line,
                .use_tabs = opts.spaces_mode,
            }) catch |err| {
                writeStats("Format error: {}\n", .{err});
                std.process.exit(1);
            };
            output = formatted.?;
        }

        writeStats("Encoded {d} bytes of input to {d} bytes\n", .{ input.len, output.len });
        try writeOutput(output, opts.passthrough_mode);
    }
}
