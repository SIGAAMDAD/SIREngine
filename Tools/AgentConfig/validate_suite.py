#!/usr/bin/env python3
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

"""Validate SIREngine Copilot agents/skills/instructions and reference integrity."""
import argparse
import re
import sys
from pathlib import Path
import yaml

ROLES_REQUIRED = {
    'gameplay','tools','ai','physics','d3d12','integration','d3d10','d3d11','d3d9','d3d8','d3d7',
    'vulkan','fmod','wwise','openal','opengl','alsa','pulseaudio','winaudio','xaudio','input','memory'
}
ALIAS = {'read','search','edit','execute','agent','web','todo','*'}
FRONT = re.compile(r'\A---\n(.*?)\n---\n(.*)\Z', re.S)
LINK = re.compile(r'\[[^\]]+\]\(([^)]+\.md)\)')


def parse_file(path):
    text = path.read_text(encoding='utf-8')
    m = FRONT.match(text)
    if not m: raise ValueError(f'{path}: missing or invalid YAML delimiters')
    # Reject duplicate YAML keys; safe_load silently overwrites them otherwise.
    class UniqueLoader(yaml.SafeLoader): pass
    def unique_map(loader,node):
        result={}
        for key_node,value_node in node.value:
            key=loader.construct_object(key_node)
            if key in result: raise ValueError(f'{path}: duplicate YAML key {key}')
            result[key]=loader.construct_object(value_node)
        return result
    UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_map)
    obj=yaml.load(m.group(1), Loader=UniqueLoader)
    if not isinstance(obj,dict): raise ValueError(f'{path}: frontmatter is not a mapping')
    return obj, m.group(2)


def validate(root):
    errors=[]
    agents=sorted((root/'.github/agents').glob('*.agent.md'))
    inst=sorted((root/'.github/instructions').glob('*.instructions.md'))
    skills=sorted((root/'.github/skills').glob('*/SKILL.md'))
    if not (root/'AGENTS.md').exists(): errors.append('AGENTS.md missing')
    if not (root/'Docs/Agents/ImprovementLoop.md').exists(): errors.append('ImprovementLoop.md missing')
    names=set()
    for p in agents+inst+skills:
        rel=p.relative_to(root)
        try: meta,body=parse_file(p)
        except Exception as e: errors.append(str(e));continue
        if not meta.get('name') or not meta.get('description'):
            errors.append(f'{rel}: missing name/description')
        if p in agents:
            if meta.get('target') != 'github-copilot': errors.append(f'{rel}: unsupported target')
            if meta['name'] in names: errors.append(f'{rel}: duplicate agent name')
            names.add(meta['name'])
            if any(x in meta for x in ('handoffs','agents','advancedOptions')): errors.append(f'{rel}: Local harness field')
            if not isinstance(meta.get('tools'),list): errors.append(f'{rel}: missing tool list')
            elif any(t not in ALIAS and '/' not in str(t) for t in meta['tools']): errors.append(f'{rel}: unknown tool')
            if p.name.endswith('-programmer.agent.md'):
                if meta.get('user-invocable') is not False: errors.append(f'{rel}: specialist must be hidden from picker')
                if 'ImprovementLoop.md' not in body: errors.append(f'{rel}: missing improvement reference')
        if p in inst and not isinstance(meta.get('applyTo'),str): errors.append(f'{rel}: missing applyTo')
        if p in skills:
            name=meta.get('name','')
            if not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*',name): errors.append(f'{rel}: invalid skill name')
            if p.parent.name != name: errors.append(f'{rel}: skill name does not match directory')
        for link in LINK.findall(body):
            if link.startswith(('http:','https:')): continue
            dest=(p.parent/link).resolve()
            if not dest.is_relative_to(root.resolve()) or not dest.is_file(): errors.append(f'{rel}: broken reference {link}')
    for slug in sorted(ROLES_REQUIRED):
        p=root/'.github/agents'/f'sir-{slug}-programmer.agent.md'
        if not p.is_file(): errors.append(f'required specialist missing: {slug}')
    if not (root/'.github/skills/sir-improve-system/SKILL.md').exists(): errors.append('improvement skill missing')
    director=root/'.github/agents/sir-director.agent.md'
    if director.exists():
        try:
            m,_=parse_file(director)
            if 'agent' not in m.get('tools',[]): errors.append('director lacks delegation tool')
        except Exception as e: errors.append(str(e))
    all_names=[str(x.relative_to(root)) for x in agents+inst+skills]
    return errors,len(agents),len(inst),len(skills)


def self_test():
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        p=Path(d)/'test.agent.md'
        p.write_text('---\nname: X\nname: Y\ndescription: Z\n---\nbody\n')
        try: parse_file(p); raise AssertionError('duplicate YAML key was accepted')
        except ValueError as e: assert 'duplicate' in str(e)
        p.write_text('---\nname: X\ndescription: Y\n--\n')
        try: parse_file(p); raise AssertionError('invalid closing marker accepted')
        except ValueError: pass
        p.write_text('---\nname: X\ndescription: Y\ntools: [read, search]\n---\nbody\n')
        m,b=parse_file(p); assert m['tools']==['read','search'] and b=='body\n'
    print('SELF TEST PASS: duplicate YAML, malformed delimiters, valid tool array')


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--root',type=Path,default=Path('.'))
    p.add_argument('--self-test',action='store_true')
    a=p.parse_args()
    if a.self_test: self_test();return 0
    errors,ag,it,sk=validate(a.root.resolve())
    print(f'Inventory: {ag} agents, {it} scoped instructions, {sk} skills')
    for e in errors: print('ERROR:',e)
    print('VALIDATION PASS' if not errors else f'VALIDATION FAIL: {len(errors)} error(s)')
    return int(bool(errors))
if __name__=='__main__': sys.exit(main())
