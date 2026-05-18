const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // =========================================================================
    // Core Library Module (for use by other Zig packages)
    // =========================================================================
    const lib_mod = b.addModule("printable_binary", .{
        .root_source_file = b.path("src/zig/printable_binary.zig"),
        .target = target,
        .optimize = optimize,
    });

    // =========================================================================
    // Static Library with C ABI (for FFI consumers like Cosmopolitan)
    // =========================================================================
    const static_lib_mod = b.createModule(.{
        .root_source_file = b.path("src/zig/printable_binary.zig"),
        .target = target,
        .optimize = optimize,
    });

    const static_lib = b.addLibrary(.{
        .name = "printable_binary",
        .linkage = .static,
        .root_module = static_lib_mod,
    });

    // Install the static library
    b.installArtifact(static_lib);

    // Also install the header for convenience
    b.installFile("src/printable_binary.h", "include/printable_binary.h");

    // =========================================================================
    // CLI Executable (Zig 0.15 style with root_module)
    // =========================================================================
    // CLI calls std.posix.system.write; on Windows std.c requires libc.
    // On Linux/macOS we use direct syscalls, so libc is unnecessary and
    // dynamic-linking it breaks reproducible Nix builds.
    const need_libc = target.result.os.tag == .windows;
    const cli_mod = b.createModule(.{
        .root_source_file = b.path("src/zig/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = need_libc,
        .imports = &.{
            .{ .name = "printable_binary", .module = lib_mod },
        },
    });

    const exe = b.addExecutable(.{
        .name = "printable-binary-zig",
        .root_module = cli_mod,
    });

    b.installArtifact(exe);

    // =========================================================================
    // Run Command
    // =========================================================================
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the CLI");
    run_step.dependOn(&run_cmd.step);

    // =========================================================================
    // Unit Tests (Zig 0.15 style with root_module)
    // =========================================================================
    const test_mod = b.createModule(.{
        .root_source_file = b.path("src/zig/printable_binary.zig"),
        .target = target,
        .optimize = optimize,
    });

    const lib_tests = b.addTest(.{
        .root_module = test_mod,
    });

    const run_lib_tests = b.addRunArtifact(lib_tests);
    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_lib_tests.step);

    // =========================================================================
    // Library-only step (for Makefile integration)
    // =========================================================================
    const lib_step = b.step("lib", "Build only the static library");
    lib_step.dependOn(&static_lib.step);
}
