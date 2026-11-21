#!/usr/bin/env python3
"""Header policy regression: guards, PCH, and non-forced header methods."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/Standards"))
from sir_clang_policy import Checker

clang = sys.argv[1] if len(sys.argv) > 1 else "clang++-23"
with tempfile.TemporaryDirectory(prefix="sir-headers-") as directory:
    repo = Path(directory)
    (repo / "Source/Core").mkdir(parents=True)
    (repo / "Source/Core/SIRPCH.h").write_text("#ifndef __SIR_PCH_H__\n#define __SIR_PCH_H__\n#endif\n")
    (repo / "Source/SIRBadHeader.h").write_text("#ifndef WRONG_GUARD\n#define WRONG_GUARD\n#endif\n")
    checker = Checker(repo, repo / "Source/SIRBadHeader.h")
    checker.scan_headers()
    rules = {e["rule"] for e in checker.errors}
    assert {"SIR040", "SIR041"} <= rules, rules
    (repo / "Source/SIRBadHeader.h").unlink()

    header = repo / "Source/SIRWidget.h"
    header.write_text('''#ifndef __SIR_WIDGET_H__
#define __SIR_WIDGET_H__
#include <Core/SIRPCH.h>
#include <cstdint>
class SIRWidget
{
public:
    explicit SIRWidget( void ) = default;
    explicit SIRWidget( const SIRWidget& ) = delete;
    explicit SIRWidget( SIRWidget&& ) = delete;
    ~SIRWidget() = default;
    SIRWidget& operator=( const SIRWidget& ) = delete;
    SIRWidget& operator=( SIRWidget&& ) = delete;
    uint32_t GetValue( void ) const noexcept;
};
uint32_t SIRWidget::GetValue( void ) const noexcept
{
    return 1;
}
#endif
''')
    source = repo / "Source/SIRWidget.cpp"
    source.write_text('#include "SIRWidget.h"\n')
    cmd = [clang, "-std=c++23", "-I", str(repo / "Source"), "-fsyntax-only",
           "-Xclang", "-ast-dump=json", str(source)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    checker = Checker(repo, source)
    checker.walk(json.loads(result.stdout))
    rules = {e["rule"] for e in checker.errors}
    assert "SIR038" in rules, sorted(rules)

print("Header policy fixtures PASS")
