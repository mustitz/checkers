#!/usr/bin/env python3

from configargparse import ArgParser
from glob import glob
from itertools import count
from math import log10

import numpy as np

import os
import sys
import yaml


parser_args = {
    'add_config_file_help': False,
    'add_env_var_help': False,
    'auto_env_var_prefix': 'CHECKERS_',
}

tournament_dir_arg = {
    'help': 'Tournament directory.',
    'nargs': '?',
    'default' : '.',
}


p = ArgParser(**parser_args)
p.add('tournament_dir', **tournament_dir_arg)
options = p.parse_args()
os.chdir(options.tournament_dir)


SCORE = { '1': 1.0, '½': 0.5, '0': 0.0 }
def score(s):
    result = 0.0
    for ch in s:
        result += SCORE[ch]
    return result


def load_stats():
    files = glob('stats/*.stat')
    if len(files) < 2:
        print('No players')
        sys.exit(1)
    names = map(os.path.basename, files)
    names = map(lambda name: os.path.splitext(name)[0], names)
    stats = dict(zip(names, map(lambda path: {'path':path}, files)))

    for k, v in stats.items():
        with open(v['path'], 'r') as stream:
            stats[k]['stats'] = yaml.load(stream)['stats']

    max_len = 0
    for name, data in stats.items():
        data['score'] = 0
        data['qgames'] = 0
        for k, v in data['stats'].items():
            if not k in stats.keys():
                continue
            qgames = len(v)
            max_len = max_len if max_len >= qgames else qgames
            data['qgames'] += qgames
            data['score'] += score(v)
        data['nscore'] = data['score'] / data['qgames']

    return stats, max_len


def calc_elo(stats):
    names = list(stats.keys())
    n = len(names)
    dim = n, n
    A = np.zeros(dim)
    B = np.zeros(n)

    elo_diff = lambda e: -400 * log10(1/e-1)
    for name in names:
        stat = stats[name]
        i1 = names.index(name)
        for k, v in stat['stats'].items():
            i2 = names.index(k)
            A[i1, i2] = -len(v) / stat['qgames']
        A[i1, i1] = 1.0
        B[i1] = elo_diff(stat['nscore'])

    X = np.linalg.lstsq(A, B, rcond=None)
    R = X[0]
    R = list(R-min(R))

    for name, elo in zip(names, R):
        stats[name]['elo'] = int(round(elo))


def print_table(stats, max_len):
    get_elo = lambda name: stats[name]['elo']
    names = sorted(stats.keys(), key=get_elo, reverse=True)
    name_len = len(max(names, key=lambda name: len(name)))

    for n, name in zip(count(1), names):
        items = []
        items.append(str(n).rjust(2) + '.')
        items.append(name.ljust(name_len))
        for opponent in names:
            if opponent == name:
                items.append('.' * max_len)
            else:
                result = stats[name]['stats'].get(opponent, '')
                items.append(result.ljust(max_len))
        items.append(str(round(stats[name]['score'], 1)).rjust(6))
        items.append(str(stats[name]['elo']).rjust(6))
        print(' '.join(items))

stats, max_len = load_stats()
calc_elo(stats)
print_table(stats, max_len)
