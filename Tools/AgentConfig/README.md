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

# Agent overlay validator

Run `python Tools/AgentConfig/validate_suite.py --root .` and `python Tools/AgentConfig/validate_suite.py --self-test` after extracting. Requires PyYAML (`pip install pyyaml`). Checks YAML, required roles, skill names, links and native Copilot aliases; it cannot prove that local provider models/MCP tools actually execute. Inspect and smoke-test the suite in VS Code.
