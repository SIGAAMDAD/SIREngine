## Approved change contract

GitHub issue and linked child issue(s):

Approved architectural contract or owner decision (when applicable):

Target branch, exact HEAD, base and merge-base:

## Source and behavior

- [ ] Changed paths belong to the approved scope and owning system.
- [ ] Shared ABI, lifetime, threading, serialization and build contracts are preserved or have explicit owner approval.
- [ ] Actual declarations, definitions, consumers, registrations and error-path tests are included.
- [ ] No generated validation JSON, reports, SDK redistributables, build outputs or provisional architecture documents are committed.

## Verification

- [ ] Changed eligible C/C++ sources were formatted and checked with clang-format 23.1.5.
- [ ] Affected C++17 sources compiled and linked through CMake 4.4.5 / FASTBuild 1.20 (Ninja for LLVM tooling).
- [ ] Relevant CTest/GoogleTest selections ran: nonempty selection, zero skipped/failed, with JUnit evidence.
- [ ] Linux GCC, Linux Clang and Windows MSVC native tests passed on the proposed candidate.
- [ ] Stable required status check sir/required is green for the frozen PR candidate SHA.
- [ ] Required LLVM 23.1.5 line/branch/function/region coverage, ASan/UBSan and enabled native backend checks passed where required.
- [ ] Performance-sensitive changes have comparable hardware/compiler/workload/source-SHA measurements.

## Integration

- [ ] Compared existing branch and target against the real merge base, preserving unfinished work.
- [ ] Qualified the combined source tree before integrating and checked the new remote SHA.
- [ ] Linked GitHub Issues contain source-bound evidence, acceptance decisions and unresolved blockers.

Exact SHA, commands, Actions URLs, test totals, coverage counters, benchmark identity and required approvals:
