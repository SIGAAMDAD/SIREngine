#!/usr/bin/env python3
"""Parse/normalize real CMake compilation commands for POSIX and Windows.

MSVC-style commands must be replayed in clang-cl driver mode. The helper
removes only output/dependency generation flags, never build defines/includes.
"""
from __future__ import annotations

import os
from pathlib import Path
import shlex


def split_command(command: str) -> list[str]:
    if os.name != "nt":
        return shlex.split(command)
    # shlex is a POSIX shell lexer and destroys unquoted C:\ paths.
    import ctypes
    from ctypes import wintypes
    shell = ctypes.WinDLL("shell32", use_last_error=True)
    shell.CommandLineToArgvW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_int)]
    shell.CommandLineToArgvW.restype = ctypes.POINTER(wintypes.LPWSTR)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.LocalFree.argtypes = [ctypes.c_void_p]
    kernel.LocalFree.restype = ctypes.c_void_p
    count = ctypes.c_int()
    ptr = shell.CommandLineToArgvW(command, ctypes.byref(count))
    if not ptr:
        raise OSError(ctypes.get_last_error(), "CommandLineToArgvW failed")
    try:
        return [ptr[i] for i in range(count.value)]
    finally:
        kernel.LocalFree(ctypes.cast(ptr, ctypes.c_void_p))


def command_arguments(entry: dict) -> list[str]:
    arguments = entry.get("arguments")
    if arguments is not None:
        return list(arguments)
    return split_command(entry["command"])


def msvc_style(arguments: list[str]) -> bool:
    if not arguments:
        return False
    driver = Path(arguments[0].replace("\\", "/")).name.lower()
    if driver in ("cl", "cl.exe", "clang-cl", "clang-cl.exe"):
        return True
    return any(arg.startswith(("/Fo", "/Fd", "/showIncludes", "/std:")) for arg in arguments[1:])


def analysis_command(clang: str, original: list[str], source: Path, header: bool = False) -> list[str]:
    if not original:
        raise ValueError("compilation command is empty")
    msvc = msvc_style(original)
    cleaned = []
    skip_next = False
    for argument in original[1:]:
        if skip_next:
            skip_next = False
            continue
        if argument in ("-o", "-MF", "-MT", "-MQ", "-MJ", "/Fo", "/Fd", "/Fe", "/Fp"):
            skip_next = True
            continue
        if argument in ("-c", "-MD", "-MMD", "-MP", "-MG", "/c", "/showIncludes", "/showIncludes:user"):
            continue
        if argument.startswith(("-o", "-MF", "-MT", "-MQ", "-MJ")) and len(argument) > 3:
            continue
        if argument.startswith(("/Fo", "/Fd", "/Fe", "/Fp")) and len(argument) > 3:
            continue
        if header and not argument.startswith("-") and not argument.startswith("/I") and not argument.startswith("/D"):
            if Path(argument.replace("\\", "/")).name.lower() == source.name.lower():
                continue
        cleaned.append(argument)
    result = [clang]
    clang_name = Path(clang.replace("\\", "/")).name.lower()
    if msvc and "clang-cl" not in clang_name:
        result.append("--driver-mode=cl")
    result.extend(cleaned)
    if header:
        if msvc:
            result.extend(["/TP", "/Zs", str(source)])
        else:
            result.extend(["-x", "c++-header", "-fsyntax-only", str(source)])
    else:
        result.append("/Zs" if msvc else "-fsyntax-only")
        result.extend(["-Xclang", "-ast-dump=json"])
    return result
