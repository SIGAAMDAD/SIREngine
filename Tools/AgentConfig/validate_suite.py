#!/usr/bin/env python3
"""Static checks for the SIREngine VS Code Copilot agent overlay (Python 3.9+)."""
from __future__ import annotations
import argparse
import re
import sys
import tempfile
from pathlib import Path

try:
    import yaml
except ImportError:
    yaml = None

SUPPORTED_TOOLS = {'execute', 'read', 'edit', 'search', 'agent', 'web', 'todo', '*'}
LOCAL_ONLY_AGENT_FIELDS = {'handoffs', 'agents', 'argument-hint', 'advancedOptions'}
SKILL_NAME = re.compile(r'^[a-z0-9]+(?:-[a-z0-9]+)*$')


def frontmatter(path: Path):
    text = path.read_text(encoding='utf-8')
    if not text.startswith('---\n'):
        return None, 'missing YAML frontmatter'
    pieces = text.split('\n---\n', 1)
    if len(pieces) != 2:
        return None, 'unterminated YAML frontmatter'
    raw = pieces[0][4:]
    if yaml is not None:
        try:
            obj = yaml.safe_load(raw)
        except Exception as exc:
            return None, f'invalid YAML: {exc}'
    else:
        # Small, dependency-free subset sufficient for this suite. Does not
        # pretend to validate every possible YAML construct.
        obj = {}
        for line in raw.splitlines():
            m = re.fullmatch(r'([a-zA-Z][a-zA-Z0-9_-]*):\s*(.*?)\s*', line)
            if not m:
                return None, 'unsupported YAML syntax without PyYAML; install pyyaml'
            key, value = m.groups()
            if key in obj:
                return None, f'duplicate key {key}'
            if value.startswith('[') and value.endswith(']'):
                obj[key] = [s.strip().strip("'\"") for s in value[1:-1].split(',') if s.strip()]
            elif value in ('true', 'false'):
                obj[key] = value == 'true'
            else:
                obj[key] = value.strip("'\"")
    if not isinstance(obj, dict):
        return None, 'YAML header must be an object'
    return obj, None


def validate(root: Path):
    errors = []
    warnings = []
    required = [root / 'AGENTS.md', root / 'Docs/Agents/README.md']
    for p in required:
        if not p.exists():
            errors.append(f'{p}: required file missing')
    agents = sorted((root / '.github/agents').glob('*.agent.md'))
    instructions = sorted((root / '.github/instructions').glob('*.instructions.md'))
    skills = sorted((root / '.github/skills').glob('*/SKILL.md'))
    if len(agents) < 5 or len(instructions) < 5 or len(skills) < 4:
        errors.append('incomplete suite: require >=5 agents, >=5 instructions, >=4 skills')
    names = {}
    for p in agents + instructions + skills:
        meta, error = frontmatter(p)
        if error:
            errors.append(f'{p.relative_to(root)}: {error}')
            continue
        path_name = str(p.relative_to(root))
        if p in agents:
            if meta.get('target') != 'github-copilot':
                errors.append(f'{path_name}: target must be github-copilot')
            if not meta.get('name') or not meta.get('description'):
                errors.append(f'{path_name}: agent requires name/description')
            if LOCAL_ONLY_AGENT_FIELDS.intersection(meta):
                errors.append(f'{path_name}: Local-only frontmatter: {LOCAL_ONLY_AGENT_FIELDS.intersection(meta)}')
            tools = meta.get('tools', [])
            if not isinstance(tools, list):
                errors.append(f'{path_name}: tools must be a list')
            else:
                for tool in tools:
                    if tool not in SUPPORTED_TOOLS and '/' not in str(tool):
                        errors.append(f'{path_name}: unsupported tool alias {tool}')
            if meta.get('name') in names:
                errors.append(f'{path_name}: duplicate agent name with {names[meta["name"]]}')
            names[meta.get('name')] = path_name
            if meta.get('user-invocable') is False and meta.get('disable-model-invocation') is True:
                warnings.append(f'{path_name}: hidden worker cannot be automatically delegated')
        elif p in instructions:
            if not meta.get('applyTo') or not isinstance(meta['applyTo'], str):
                errors.append(f'{path_name}: missing string applyTo')
            if not meta.get('name'):
                errors.append(f'{path_name}: missing name')
        else:
            if not meta.get('name') or not meta.get('description'):
                errors.append(f'{path_name}: skill requires name/description')
                continue
            if not SKILL_NAME.fullmatch(meta['name']) or len(meta['name']) > 64:
                errors.append(f'{path_name}: invalid skill name')
            if p.parent.name != meta['name']:
                errors.append(f'{path_name}: skill name must equal parent directory')
            if meta['name'] in names:
                errors.append(f'{path_name}: duplicate name with {names[meta["name"]]}')
            names[meta['name']] = path_name
            if meta.get('context') not in (None, 'fork'):
                errors.append(f'{path_name}: unsupported skill context')
        body = p.read_text(encoding='utf-8').split('\n---\n', 1)[-1]
        for target in re.findall(r'\[[^\]]+\]\(([^)]+\.md)\)', body):
            if re.match(r'^https?://', target):
                continue
            linked = (p.parent / target).resolve()
            try:
                linked.relative_to(root.resolve())
            except ValueError:
                errors.append(f'{path_name}: link escapes suite: {target}')
                continue
            if not linked.is_file():
                errors.append(f'{path_name}: broken relative link: {target}')
    # Boundary rules: the corpus must never instruct workers to branch by default.
    main_text = (root / 'AGENTS.md').read_text(encoding='utf-8') if (root / 'AGENTS.md').exists() else ''
    checks = {
        'C++23 primary': r'C\+\+23',
        'C++17 fallback': r'C\+\+17',
        'branch authority': r'No branch creation unless',
        'FASTBuild': r'FASTBuild',
        'allocator routing': r'allocator',
        'subagents': r'Delegate',
    }
    for label, pattern in checks.items():
        if not re.search(pattern, main_text, re.IGNORECASE):
            errors.append(f'AGENTS.md missing required policy: {label}')
    for p in agents:
        if p.name == 'sir-director.agent.md':
            m, _ = frontmatter(p)
            if m and 'agent' not in m.get('tools', []):
                errors.append('director missing native Copilot subagent alias agent')
    return errors, warnings, len(agents), len(instructions), len(skills)


def self_test():
    with tempfile.TemporaryDirectory() as temp:
        d = Path(temp)
        f = d / 'invalid.agent.md'
        f.write_text('---\nname: Broken\ntarget: github-copilot\ntools: [read]\nagents: [Other]\n--\n', encoding='utf-8')
        _, error = frontmatter(f)
        assert error, 'malformed separator should fail'
        f.write_text('---\nname: Valid\ndescription: Test\ntools: [read, search]\n---\nBody\n', encoding='utf-8')
        meta, error = frontmatter(f)
        assert error is None and meta['name'] == 'Valid'
        assert meta['tools'] == ['read', 'search']
        assert SKILL_NAME.fullmatch('sir-verify') and not SKILL_NAME.fullmatch('Sir_Verify')
        assert LOCAL_ONLY_AGENT_FIELDS.intersection({'agents': ['Other']})
    print('SELF TEST PASS: parser, malformed frontmatter, naming, Local-only gate')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', default='.', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    errors, warnings, n_agents, n_instructions, n_skills = validate(args.root.resolve())
    print(f'Inventory: {n_agents} agents, {n_instructions} scoped instruction files, {n_skills} skills')
    for w in warnings:
        print('WARNING:', w)
    for e in errors:
        print('ERROR:', e)
    print('VALIDATION PASS' if not errors else f'VALIDATION FAIL ({len(errors)} errors)')
    if yaml is None:
        print('NOTICE: PyYAML not installed; only the suite-supported YAML subset was checked')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
