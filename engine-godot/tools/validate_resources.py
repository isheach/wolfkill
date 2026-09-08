#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Validate the Godot resource JSON contract exported from the C engine."""

import json
import os
import re
import sys
from collections import Counter

DEFAULT_JSON = os.path.abspath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'data', 'ksg_resources.json'))

EXPECTED_KINDS = {'skill': 468, 'np': 247, 'item': 43}
EXPECTED_RESOURCES = 758
EXPECTED_EFFECTS = 1433
EXPECTED_ONE_EFFECT = 406
EXPECTED_MULTI_EFFECT = 352

SKILL_TYPES = {
    'KS_T_CLASS', 'KS_T_TALENT', 'KS_T_TECHNIQUE', 'KS_T_BLESS',
    'KS_T_CROWN', 'KS_T_WEAPON', 'KS_T_MAGIC',
}
NP_TYPES = {
    'KS_NP_HUMAN', 'KS_NP_ARMORY', 'KS_NP_CASTLE', 'KS_NP_WORLD',
    'KS_NP_BOUND', 'KS_NP_MAKLESS',
}
FOCUSES = {
    'FC_DECISIVE', 'FC_INSTAKILL', 'FC_SWORD', 'FC_DEFENSE', 'FC_OFFENSE',
    'FC_BUFF', 'FC_SUMMON', 'FC_STATUS', 'FC_SUPPLY', 'FC_SPECIAL',
    'FC_ANTITRAIT',
}
WHENS = {'passive', 'act', 'battle_start', 'proc', 'any'}
RANKS = {'-', 'E', 'D', 'C', 'B', 'A', 'EX'}
FEATURES = {
    'KS_F_MAIN', 'KS_F_SUPPORT', 'KS_F_SERVANT', 'KS_F_REAR', 'KS_F_ASSIST',
    'KS_F_COUNTER', 'KS_F_RIDE', 'KS_F_BURST_READY', 'KS_F_PIERCE',
    'KS_F_INV_PIERCE', 'KS_F_ENERGY', 'KS_F_UNIQUE',
}
FLAGS = {
    'EF_NONE', 'EF_ATTR_UP', 'EF_ATTR_DOWN', 'EF_ATTR_UP_CONST',
    'EF_ATTR_DOWN_CONST', 'EF_WIN_UP', 'EF_WIN_DOWN', 'EF_FINAL_WIN_UP',
    'EF_FINAL_WIN_DOWN', 'EF_FLOOR_UP', 'EF_FLOOR_PEN', 'EF_HIT_UP',
    'EF_HIT_FINAL_UP', 'EF_HIT_PEN', 'EF_RES_UP', 'EF_RES_DOWN',
    'EF_STATE_RES', 'EF_STATE_IM', 'EF_EFFECT_IM', 'EF_MANA_UP',
    'EF_MANA_DOWN', 'EF_STATUS_GIVE', 'EF_STATUS_REMOVE', 'EF_BURN_BLOW',
    'EF_ELECTRIC_BLOW', 'EF_POISON_BLOW', 'EF_RECAST', 'EF_RECAST_LOSE',
    'EF_FP_UP', 'EF_FP_DOWN', 'EF_TP_FP', 'EF_DEATH', 'EF_BOUND_DEATH',
    'EF_PIERCE', 'EF_INV_PIERCE', 'EF_EVADE', 'EF_PROTECT',
    'EF_INVINCIBLE', 'EF_RETALIATE', 'EF_INFO', 'EF_CS', 'EF_SUMMON',
    'EF_OTHER', 'EF_PLEDGE_DECL', 'EF_TICK_PROC', 'EF_TICK_ROUND', 'EF_RANDOM_GIVE',
    'EF_ON_KILL', 'EF_ON_WIN', 'EF_CHARGE', 'EF_GRANT_CS', 'EF_GRANT_BADGE', 'EF_SEAL', 'EF_REGEN',
}
CONDS = {
    'KC_NONE', 'KC_OWN_MAIN', 'KC_OWN_SUPPORT', 'KC_OWN_REAR',
    'KC_TARGET_TRAIT', 'KC_TARGET_NOT_TRAIT', 'KC_TARGET_STATUS_EQ',
    'KC_TARGET_STATUS_GE', 'KC_STATUS_NOT', 'KC_FRIEND_STATUS',
    'KC_TARGET_LUCK_GE', 'KC_TARGET_LEVEL_GE', 'KC_LEVEL_DIFF', 'KC_DAY',
    'KC_NIGHT', 'KC_FIRST_ENCOUNTER', 'KC_ENEMY_MAIN_MASTER', 'KC_SELF_HP',
    'KC_SELF_MP', 'KC_FRIEND_IN_BATTLE', 'KC_ENEMY_IN_BATTLE', 'KC_SELF_TRAIT',
    'KC_ENEMY_TACTIC', 'KC_SELF_TACTIC', 'KC_TACTIC_NOT_PAIRED',
    'KC_ENEMY_IS_SERVANT', 'KC_ENEMY_IS_SUMMON', 'KC_SELF_IS_MASTER',
    'KC_SELF_IS_SERVANT', 'KC_HAS_CS', 'KC_MP_UNDER', 'KC_TARGET_AGI_LT',
    'KC_TARGET_AGI_GE',
}
ATTRS = {'A_STR', 'A_END', 'A_AGI', 'A_MAG', 'A_LUK', 'A_NP', 'A_CIRCUIT'}
STATUSES = {
    'S_EVADE', 'S_INVINCIBLE', 'S_PROTECT', 'S_RESUP', 'S_STATE_RES',
    'S_STATE_IM', 'S_EFFECT_IM', 'S_TIRED', 'S_CRIPPLED', 'S_LAG', 'S_CURSE',
    'S_SEAL', 'S_SKILL_SEAL', 'S_NP_SEAL', 'S_RESDOWN', 'S_POISON', 'S_BURN',
    'S_FREEZE', 'S_ELECTRIC', 'S_STONE', 'S_STUN', 'S_CHARM', 'S_CONFUSE',
    'S_FEAR', 'S_RES_BREAK', 'S_TRAIT_GIVE', 'S_CHARGE', 'S_BADGE',
}
TRAITS = {'TR_HUMAN', 'TR_DIVINITY', 'TR_DRAGON', 'TR_DEMONIC', 'TR_BEAST', 'TR_GOLEM'}
EFFECT_FIELDS = {
    'flag', 'attr', 'value', 'status', 'layers', 'rank', 'target', 'times',
    'cond', 'cond_arg', 'cond_arg2', 'chance', 'chance_attr_base', 'chance_neg',
    'luck_halve', 'cap', 'desc',
}


def validate(path):
    errors = []
    warnings = []

    try:
        with open(path, encoding='utf-8') as stream:
            root = json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        return [f'cannot read JSON: {exc}'], warnings, None

    if root.get('schema_version') != 1:
        errors.append(f"schema_version must be 1, got {root.get('schema_version')!r}")
    if root.get('generated_from') != ['ksg_data.c', 'ksg_db.c']:
        errors.append('generated_from must list ksg_data.c then ksg_db.c')

    resources = root.get('resources')
    if not isinstance(resources, list):
        return errors + ['resources must be an array'], warnings, None
    if len(resources) != EXPECTED_RESOURCES:
        errors.append(f'resource count must be {EXPECTED_RESOURCES}, got {len(resources)}')

    kind_counts = Counter(r.get('kind') for r in resources if isinstance(r, dict))
    if dict(kind_counts) != EXPECTED_KINDS:
        errors.append(f'kind counts mismatch: {dict(kind_counts)}')

    ids = [r.get('id') for r in resources if isinstance(r, dict)]
    if ids != list(range(1, len(resources) + 1)):
        errors.append('resource ids must be unique and sequential from 1')

    effects = []
    one_effect = 0
    multi_effect = 0
    for index, resource in enumerate(resources, 1):
        label = f'resource[{index}]'
        if not isinstance(resource, dict):
            errors.append(f'{label} must be an object')
            continue
        name = resource.get('name')
        label = f'resource[{index}] {name!r}'
        if not isinstance(name, str) or not name.strip():
            errors.append(f'{label}: name must be non-empty')
        kind = resource.get('kind')
        rtype = resource.get('type')
        focus = resource.get('focus')
        if kind == 'skill' and rtype not in SKILL_TYPES:
            errors.append(f'{label}: invalid skill type {rtype!r}')
        elif kind == 'np':
            if rtype not in NP_TYPES:
                errors.append(f'{label}: invalid np type {rtype!r}')
            if focus not in FOCUSES:
                errors.append(f'{label}: invalid np focus {focus!r}')
        elif kind == 'item':
            if rtype not in {'KS_T_MAGIC', 'KS_T_WEAPON'}:
                errors.append(f'{label}: invalid item type {rtype!r}')
            if focus != '':
                errors.append(f'{label}: item focus must be empty')
        elif kind not in EXPECTED_KINDS:
            errors.append(f'{label}: invalid kind {kind!r}')

        if resource.get('when') not in WHENS:
            errors.append(f"{label}: invalid when {resource.get('when')!r}")
        if resource.get('rank') not in RANKS:
            errors.append(f"{label}: invalid rank {resource.get('rank')!r}")
        feat = resource.get('feat')
        if not isinstance(feat, list) or any(f not in FEATURES for f in feat):
            errors.append(f'{label}: invalid feat list {feat!r}')
        for field in ('cost', 'recast', 'count_per_round', 'reserve', 'reserve_max', 'unique'):
            if not isinstance(resource.get(field), int):
                errors.append(f'{label}: {field} must be an integer')
        if resource.get('reserve_max') != resource.get('reserve'):
            errors.append(f'{label}: reserve_max must equal the C definition reserve')
        if not isinstance(resource.get('text'), str):
            errors.append(f'{label}: text must be a string')

        item_effects = resource.get('effects')
        if not isinstance(item_effects, list) or not item_effects:
            errors.append(f'{label}: effects must contain at least one item')
            continue
        one_effect += len(item_effects) == 1
        multi_effect += len(item_effects) > 1
        for effect_index, effect in enumerate(item_effects, 1):
            validate_effect(effect, f'{label} effect[{effect_index}]', errors)
            effects.append(effect)

    if len(effects) != EXPECTED_EFFECTS:
        errors.append(f'effect count must be {EXPECTED_EFFECTS}, got {len(effects)}')
    if one_effect != EXPECTED_ONE_EFFECT or multi_effect != EXPECTED_MULTI_EFFECT:
        errors.append(
            f'effect distribution mismatch: one={one_effect}, multi={multi_effect}; '
            f'expected {EXPECTED_ONE_EFFECT}/{EXPECTED_MULTI_EFFECT}')

    duplicate_names = sorted(name for name, count in Counter(
        r.get('name') for r in resources if isinstance(r, dict)).items() if count > 1)
    if duplicate_names:
        warnings.append('duplicate resource names retained from C data: ' + ', '.join(duplicate_names))

    return errors, warnings, {
        'resources': len(resources),
        'effects': len(effects),
        'kinds': dict(kind_counts),
        'one': one_effect,
        'multi': multi_effect,
    }


def validate_effect(effect, label, errors):
    if not isinstance(effect, dict):
        errors.append(f'{label}: must be an object')
        return
    missing = EFFECT_FIELDS - set(effect)
    if missing:
        errors.append(f'{label}: missing fields {sorted(missing)}')
        return
    if effect['flag'] not in FLAGS:
        errors.append(f"{label}: invalid flag {effect['flag']!r}")
    if effect['attr'] not in ATTRS | {''}:
        errors.append(f"{label}: invalid attr {effect['attr']!r}")
    if effect['cond'] not in CONDS:
        errors.append(f"{label}: invalid cond {effect['cond']!r}")
    if not isinstance(effect['target'], str) or not re.fullmatch(
            r'(self|ally_all|enemy_all|enemy_[1-9][0-9]*)', effect['target']):
        errors.append(f"{label}: invalid target {effect['target']!r}")
    if not isinstance(effect['times'], int) or effect['times'] < 1:
        errors.append(f'{label}: times must be an integer >= 1')
    if not isinstance(effect['chance'], int) or not 0 <= effect['chance'] <= 100:
        errors.append(f'{label}: chance must be an integer from 0 to 100')
    if not isinstance(effect['cap'], int):
        errors.append(f'{label}: cap must be an integer')
    if not isinstance(effect['chance_neg'], bool) or not isinstance(effect['luck_halve'], bool):
        errors.append(f'{label}: chance_neg/luck_halve must be booleans')
    if not isinstance(effect['desc'], str):
        errors.append(f'{label}: desc must be a string')

    status = effect['status']
    if status and status not in STATUSES | {'KS_SLOT_SERVANT'}:
        errors.append(f'{label}: invalid status symbol {status!r}')
    if isinstance(effect['layers'], str) and effect['layers'] not in STATUSES:
        errors.append(f"{label}: invalid symbolic layers {effect['layers']!r}")
    if isinstance(effect['value'], str) and effect['value'] not in STATUSES:
        errors.append(f"{label}: invalid symbolic value {effect['value']!r}")
    if isinstance(effect['chance_attr_base'], str) and effect['chance_attr_base'] not in ATTRS:
        errors.append(f"{label}: invalid chance_attr_base {effect['chance_attr_base']!r}")
    for field in ('cond_arg', 'cond_arg2'):
        value = effect[field]
        if isinstance(value, str) and value not in STATUSES | TRAITS:
            errors.append(f'{label}: invalid symbolic {field} {value!r}')


def main(argv):
    path = os.path.abspath(argv[1]) if len(argv) > 1 else DEFAULT_JSON
    errors, warnings, summary = validate(path)
    for warning in warnings:
        print('WARNING:', warning)
    if errors:
        for error in errors:
            print('ERROR:', error, file=sys.stderr)
        print(f'FAILED: {len(errors)} validation error(s)', file=sys.stderr)
        return 1
    print(
        'OK: resources={resources}, effects={effects}, kinds={kinds}, '
        'one_effect={one}, multi_effect={multi}'.format(**summary))
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
