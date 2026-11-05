# P5 — Typed configuration and archive integration

**Branch:** `feature/console-cvars-commands` · [issue #282](https://github.com/SIGAAMDAD/SIREngine/issues/282) · **Status:** native Windows/Linux source acceptance PASS; canonical user:/ VFS integration remains OPEN (#282, #283).

## Existing source authority and implementation

On the inspected develop baseline, `Source/Core/Config` contained `SIRConfigBudget`, `SIRConfigTypes` and a CSV/TSV delimited codec, but **no INI/CFG parser**. `Source/Core/Platform/SIRPlatformFile.h` exposes buffered-free open/read/write/flush/close/renameReplace, but there is **no unified VFS or user:/ mount resolver**. P5 therefore adds one INI codec under the native Config format ownership rather than a second console-local parser. A caller-owned file-read callback and native atomic-file sink expose the future VFS seam. Direct `user:/` URL resolution remains a separate integration gate and is not asserted as complete.

- `Source/Core/Config/Formats/INI/SIRIni.{h,cpp}`: bounded, allocation-free parse into caller-owned entry/value storage; `sirConfigInput_t` and diagnostics, case-insensitive duplicate key detection, quoted UTF-8 escapes and strict malformed/length rejection. Writer emits canonical INI with fully preflighted output byte limits and a `sirConfigSink_t`.
- `Source/Core/CVars/SIRCVarValueCodec.{h,cpp}`: one strict type/number/UTF-8 value interpretation and scalar formatting implementation shared with the console backend. Handles int32/64, uint32/64, bool, enum, finite float32/64 and UTF-8 string; no dynamic storage.
- `Source/Core/CVars/SIRCVarConfigAdapter.{h,cpp}`: resolves each normalized key through the P2 catalog, maps to supplied P3 CVar handles, converts every typed value, and calls P3 `prepareBatch`/`commitBatch` only after complete parse/admission. Invalid syntax, duplicate, missing name, bad type, permission and full event/override budget cannot publish half a file. Archive export enumerates only ARCHIVE entries eligible for ordinary user reloading, excludes transient/read-only and unauthorized sensitive data, uses caller-owned string snapshots, sorts by canonical name and passes through `SIRIniCodec::write`.
- `Source/Core/Config/SIRConfigAtomicFile.{h,cpp}`: file-level atomic writer using an exclusive caller-provided same-directory temporary file, bounded writes, `flush`, `close`, then `SIRPlatformFile::renameReplace`. Failure/cancel removes only the temporary file and preserves existing destination. A bounded `read` consumes native platform file operations. Caller handles path authorization and same-directory selection.
- `Source/Core/Console/SIRConsoleConfigAdapter.{h,cpp}`: validated `exec name.ini` bridge using a caller-owned file-read resolver, caller-owned file data slab and Config's INI codec. Rejects absolute/traversal/non-INI names before calling the resolver; the entire input is validated into P3 writes before commit. `SIRConsoleService` now dispatches its `exec` builtin through this optional callback; no raw CFG tokenizer is invented.

## Verified local checks and pending gates

Local GCC 14 C++17 strict-warning syntax checks and the ASan+UBSan INI fixture: PASS. Pinned clang-format 23.1.5: PASS. Native real PlatformFile and linked SIRCore engine tests: [CI #41956547368](https://github.com/SIGAAMDAD/SIREngine/actions/runs/41956547368) at exact source SHA `9175f0ef45be95e1f0896cdea2439ce889acbff0`: Linux **103/103 CTests** and Windows **98/98 CTests** passed, including `core.config.ini`, `core.cvars.config`, and `core.console.service`. Exact source allocator sanitizer, scheduler sanitizer and GRDG coverage workflows also passed; source-based console P7 coverage is not claimed.

Added native tests: `core.config.ini` covers malformed/overlong/duplicate/UTF-8 input, quoted escapes, bounded output, actual file atomic replacement and failure. `core.cvars.config` covers real SIRS/SIRC cooked definitions, typed set/restore, full-file rollback, read-only policy, reproducible archive, archive atomic replace and Config-mediated `exec`. External user:/ VFS mount provider and Valve CFG codec are absent in this exact source snapshot; their integration remains OPEN rather than represented as implemented.

## Runtime contract

Only the owner thread calls CVar adapter apply/export. File resolvers and file URI policy are caller-owned; all parser working memory, typed write spans and output buffers use supplied fixed capacities. `exec` currently accepts `*.ini` files through the callback, not Valve CFG command scripts. Callback input buffers remain borrowed until the adapter returns. File output is an on-disk transactional replace, not an in-memory registry transaction. The console's existing pump and error output own request diagnostics.

## Final P5 source repair and remaining gate

The native queue-to-`exec` integration fixture identified a missing P4 builtin callback dispatch on commit `b20a09e1560acc5394ad4b933bc90976aec97d77`. `b664aef18d49cb908429804cb7f4877fd362fa8a` connected `exec` to `pfnExec` when installed; `9175f0ef45be95e1f0896cdea2439ce889acbff0` restored the P4 help fixture expectations and exact clang-format. The final SHA was tested on both native platforms. Absent callback remains a rejected `exec` request.

P5's **native Config codec, archive and platform-file work is verified**. Full milestone completion is blocked on the canonical VFS `user:/` mount provider, path ownership and mount-safe atomic replacement, tracked by [dependency #283](https://github.com/SIGAAMDAD/SIREngine/issues/283). No VFS code was fabricated or silently substituted. Valve CFG execution is deferred until a real corresponding codec exists. The P5 tracking [issue #282](https://github.com/SIGAAMDAD/SIREngine/issues/282) remains open.
