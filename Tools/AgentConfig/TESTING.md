/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

# Validator negative-case coverage

The static checker accepts a root directory and returns nonzero on missing suite members, malformed frontmatter, wrong skill directory/name, missing `applyTo`, duplicate agent names, Local-only agent fields, unsupported tool aliases, broken relative links and missing mandatory root contracts.

Use `--self-test` to exercise the parser. The full suite check also validates all packaged files. Live Copilot harness loading, subagent invocation, model/tool access and actual repository compile correctness require runtime checks in `Docs/Agents/Installation.md`.
