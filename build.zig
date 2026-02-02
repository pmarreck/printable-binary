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
    // CLI Executable (Zig 0.15 style with root_module)
    // =========================================================================
    const cli_mod = b.createModule(.{
        .root_source_file = b.path("src/zig/main.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "printable_binary", .module = lib_mod },
        },
    });

    const exe = b.addExecutable(.{
        .name = "printable_binary_zig",
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
}
