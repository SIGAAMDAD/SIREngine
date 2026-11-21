#!/usr/bin/env python3
"""Windows/MSVC driver-mode replay and portable gate regression."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/Standards'))
from compdb import analysis_command, command_arguments, msvc_style
from sir_clang_policy import run_ast

compiler = sys.argv[1] if len(sys.argv) > 1 else 'clang++-23'
with tempfile.TemporaryDirectory(prefix='sir-crossplatform-') as location:
    root = Path(location)
    source = root / 'SIRSimple.cpp'
    header = root / 'SIRSimple.h'
    source.write_text('class SIRSimple { public: unsigned Get() const; };\n')
    header.write_text('struct SIRValue { unsigned nValue; };\n')
    windows = ['cl.exe', '/nologo', '/TP', '/std:c++23', '/c', str(source), '/Fo' + str(root/'SIRSimple.obj')]
    assert msvc_style(windows)
    cmd = analysis_command(compiler, windows, source)
    assert '--driver-mode=cl' in cmd or 'clang-cl' in Path(compiler).name
    assert '/Fo' + str(root/'SIRSimple.obj') not in cmd
    assert '/c' not in cmd
    parsed = run_ast(compiler, source, windows, root, 20, 16 * 1024 * 1024)
    assert parsed['kind'] == 'TranslationUnitDecl'
    header_command = analysis_command(compiler, windows, source, header=True)
    assert str(source) not in header_command[:-1]
    assert header_command[-1] == str(source)
    assert '/TP' in header_command and '/Zs' in header_command
    posix = ['clang++-23', '-std=c++23', '-c', str(source), '-o', str(root/'file.o'), '-MF', 'file.d']
    posix_cmd = analysis_command(compiler, posix, source)
    assert '-fsyntax-only' in posix_cmd
    assert '-o' not in posix_cmd and '-MF' not in posix_cmd
    assert command_arguments({'arguments':windows}) == windows
    if os.name == 'nt':
        # Real CommandLineToArgvW parsing: spaces and backslashes are preserved.
        cmdline = subprocess.list2cmdline(windows)
        assert command_arguments({'command': cmdline}) == windows
print('Cross-platform compile command and AST regression PASS')
