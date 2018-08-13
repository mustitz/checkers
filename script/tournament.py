#!/usr/bin/env python3

from glob import glob
from math import log10
from subprocess import check_output

import configargparse
import numpy as np
import os
import re
import sys
import yaml


parserArgs = {
    'add_config_file_help': False,
    'add_env_var_help': False,
    'auto_env_var_prefix': 'CHECKERS_',
}

rateTryCountArg = {
    'help': 'Maximum attempt count to rate unrated.',
    'type': int,
    'default': 10,
}

tournamentDirArg = {
    'help': 'Tournament directory.',
    'nargs': '?',
    'default' : '.',
}


p = configargparse.ArgParser(**parserArgs)
p.add('-t', '--rate-try-count', **rateTryCountArg)
p.add('tournamentDir', **tournamentDirArg)


options = p.parse_args()
RATE_TRY_COUNT = options.rate_try_count
tournamentDir = options.tournamentDir


os.chdir(tournamentDir)


def read_assume_rank():
    try:
        result = open('assume-rank.txt').read()
    except:
        print('Warning: file assume-rank.txt is not found.')
        return {}

    result = result.split('\n')
    result = map(str.strip, result)
    result = [x for x in result if x]
    result = dict(zip(result, range(len(result))))
    return result


def get_assume_rank(name):
    return assume_rank.get(name, len(assume_rank))


def read_players():
    files = glob('players/*.player')
    if len(files) < 2:
        print('No players')
        sys.exit(1)
    names = map(os.path.basename, files)
    names = map(lambda name: os.path.splitext(name)[0], names)
    return dict(zip(names, map(lambda path: {'path':path}, files)))


def load_assume_rank(players):
    global assume_rank
    assume_rank = read_assume_rank()
    for key in players.keys():
        players[key]['assume_rank'] = get_assume_rank(key)


ZERO_STATS = { '1': 0, '0': 0, '½': 0 }


def parse_stat(raw):
    raw = raw.strip()
    result = ZERO_STATS.copy()
    for ch in raw:
        assert ch in '10½'
        result[ch] += 1
    result['raw'] = raw
    return result


def add_stats(s1, s2, reverse=False):
    s1['1'] += s2['1'] if not reverse else s2['0']
    s1['0'] += s2['0'] if not reverse else s2['1']
    s1['½'] += s2['½']


sum_games = lambda d: d['1'] + d['0'] + d['½']


def load_stats(players):
    for name in players.keys():
        player = players[name]
        path = 'stats/{}.stat'.format(name)
        try:
            with open(path, 'r') as stream:
                stats = (yaml.load(stream))
        except FileNotFoundError:
            player['qgames'] = 0
            player['stats'] = {}
            continue
        player['qgames'] = stats.get('qgames', 0)
        elo = stats.get('elo', None)
        if not elo is None:
            player['elo'] = elo
        player['stats'] = {}
        for k, v in stats.get('stats', {}).items():
            player['stats'][k] = parse_stat(v)


def save_stats(players):
    for name, player in players.items():
        to_save = player.copy()
        for k, v in player['stats'].items():
            to_save['stats'][k] = v['raw']
        path = 'stats/{}.stat'.format(name)
        with open(path, 'w') as outfile:
            yaml.dump(to_save, outfile, default_flow_style=False, allow_unicode=True)


SPARRING_REGEX = re.compile('^ *[+]([0-9]+) *[-]([0-9]+) *[=]([0-9]+).*ELO')
SPARRING_RESULTS = {
    '100': ( {'1':1,'0':0,'½':0}, '1-0' ),
    '010': ( {'1':0,'0':1,'½':0}, '0-1' ),
    '001': ( {'1':0,'0':0,'½':1}, '½-½' ),
}

def run_sparring(cmd):
    output = check_output(cmd, shell=True)
    for line in output.decode('utf-8').split('\n'):
        m = SPARRING_REGEX.match(line)
        if m is None:
            continue
        s = m.group(1) + m.group(2) + m.group(3)
        r = SPARRING_RESULTS.get(s)
        if not r is None:
            return r
    raise AssertionError('No matched strings in output')


def sparring(p1, p2):
    name1, player1 = p1[0], p1[1]
    name2, player2 = p2[0], p2[1]

    cmd = 'play.py {} {} --shm -c1 -l tmp.pdn'
    cmd = cmd.format(player1['path'], player2['path'])
    result, out = run_sparring(cmd)

    stat1 = player1['stats'].get(name2, ZERO_STATS).copy()
    stat2 = player2['stats'].get(name1, ZERO_STATS).copy()

    add_stats(stat1, result)
    add_stats(stat2, result, reverse=True)

    stat1['raw'] = stat1.get('raw', '') + out[0]
    stat2['raw'] = stat2.get('raw', '') + out[2]

    player1['stats'][name2] = stat1
    player2['stats'][name1] = stat2

    player1['qgames'] += 1
    player2['qgames'] += 1

    ws = ' ' * (40 - len(name1) - len(name2))
    stat = '+{} -{} ={}'.format(stat1['1'], stat1['0'], stat1['½'])
    print(out, ' '*5, name1, 'VS', name2, ws, stat)

    cmd1 = 'cat tmp.pdn >> games/{}.pdn'.format(name1)
    cmd2 = 'cat tmp.pdn >> games/{}.pdn'.format(name2)
    check_output(cmd1, shell=True)
    check_output(cmd2, shell=True)

    cmd1 = 'echo '' >> games/{}.pdn'.format(name1)
    cmd2 = 'echo '' >> games/{}.pdn'.format(name2)
    check_output(cmd1, shell=True)
    check_output(cmd2, shell=True)


def split_by_rated(players):
    is_unrated = lambda pair: pair[1].get('elo') is None
    is_rated = lambda pair: not is_unrated(pair)
    rated = [ element for element in players.items() if is_rated(element) ]
    unrated = [ element for element in players.items() if is_unrated(element) ]
    return rated, unrated


may_rated = lambda stat: stat['½'] != 0 or stat['1'] * stat['0'] != 0
def update_ratings(players):
    estimate_list = []
    for k1, v1 in players.items():
        all_stat = ZERO_STATS.copy()
        for k2, v2 in v1['stats'].items():
            add_stats(all_stat, v2)
        if may_rated(all_stat):
            estimate_list.append(k1)

    if len(estimate_list) < 2:
        return

    names = list(map(lambda name: name, estimate_list))

    def fetch_stats(name):
        player = players[name]
        result = { 'name' : name, 'qgames' : 0, '1' : 0, '0' : 0, '½' : 0, 'stats' : {} }
        for opponent_name in names:
            stats = player['stats'].get(opponent_name, ZERO_STATS).copy()
            add_stats(result, stats)
            stats['qgames'] = sum_games(stats)
            result['qgames'] += stats['qgames']
            result['stats'][opponent_name] = stats
        result['score'] = 1.0 * result['1'] + 0.5 * result['½']
        result['nscore'] = result['score'] / result['qgames']
        return result

    estimate_list = list(map(fetch_stats, estimate_list))

    n = len(estimate_list)
    dim = n, n
    A = np.zeros(dim)
    B = np.zeros(n)

    elo_diff = lambda e: -400 * log10(1/e-1)
    for player in estimate_list:
        i1 = names.index(player['name'])
        for k, v in player['stats'].items():
            i2 = names.index(k)
            A[i1, i2] = -v['qgames'] / player['qgames']
        A[i1, i1] = 1.0
        B[i1] = elo_diff(player['nscore'])

    X = np.linalg.lstsq(A, B, rcond=None)
    R = X[0]
    R = list(R-min(R))

    for name, elo in zip(names, R):
        players[name]['elo'] = int(round(elo))


def all_unrated(players, unrated):
    order = lambda pair: pair[1]['assume_rank']
    unrated = sorted(unrated, key=order)
    p1, unrated = unrated[0], unrated[1:]
    name1, player1 = p1[0], p1[1]

    opponents = [ element for element in unrated if element[1]['qgames'] <= RATE_TRY_COUNT ]
    if len(opponents) == 0:
        print('It looks like it is impossible to rate player', name1)
        sys.exit(1)

    p2 = opponents[0]
    name2, player2 = p2[0], p2[1]
    sparring(p1, p2)
    sparring(p2, p1)


def all_rated(players, rated):
    print('Not implemented: all rated')
    sys.exit(1)


def some_unrated(players, rated, unrated):
    order = lambda pair: pair[1]['assume_rank']
    unrated = sorted(unrated, key=order)
    order = lambda pair: pair[1]['elo']
    for p2 in unrated:
        get_qgames = lambda player : len(player[1].get('stats', {}).get(p2[0], {}).get('raw', ''))
        opponents = [ player for player in rated if get_qgames(player) < RATE_TRY_COUNT ]
        if len(opponents) == 0:
            continue
        opponents = sorted(opponents, key=order)
        p1 = opponents[-1]
        sparring(p1, p2)
        sparring(p2, p1)
        return
    unrated_names = map(lambda player : player[0], unrated)
    print('It looks like it is impossible to rate unrated players:', list(unrated_names))
    sys.exit(1)


players = read_players()
load_assume_rank(players)
load_stats(players)
rated, unrated = split_by_rated(players)

if len(rated) == 0:
    all_unrated(players, unrated)
elif len(unrated) == 0:
    all_rated(players, rated)
else:
    some_unrated(players, rated, unrated)

update_ratings(players)
save_stats(players)
