# ADR-0002: CMake entrypoint with Godot SCons bridge

- **Date:** 2026-11-02
- **Status:** accepted as an intermediate build contract

## Decision

Require CMake 4.4.5 for SIR-owned C++17 code, dependency resolution, tests and build presets. Retain Godot's verified SCons graph under an explicit CMake custom target for editor compilation; pin SCons 4.11.1 in CI.

## Why

A wholesale conversion of Godot's generator and platform-specific compilation logic would be an independent large engineering project. Treating a script wrapper as a native CMake conversion would hide missing build closure. The bridge allows reliable shared entrypoints while retaining the current Godot build semantics.

## Exit criteria

A future full native CMake port must provide equivalent source/feature configurations, generated files, shader tools, per-OS platform checks, toolchain flags, SDK packaging and compile/link parity. Remove the bridge only when that equivalence is demonstrated.
