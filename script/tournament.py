#!/usr/bin/env python3

from glob import glob

import configargparse
import os
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


def sparring(p1, p2):
    name1, data1 = p1[0], p1[1]
    name2, data2 = p2[0], p2[1]
    print('Not implemented: sparring', name1, 'VS', name2)
    sys.exit(1)


def split_by_rated(players):
    is_unrated = lambda pair: pair[1].get('elo') is None
    is_rated = lambda pair: not is_unrated(pair)
    rated = [ element for element in players.items() if is_rated(element) ]
    unrated = [ element for element in players.items() if is_unrated(element) ]
    return rated, unrated


def all_unrated(players, unrated):
    order = lambda pair: pair[1]['assume_rank']
    unrated = sorted(unrated, key=order)
    p1, unrated = unrated[0], unrated[1:]

    opponents = [ element for element in unrated if element[1]['qgames'] <= RATE_TRY_COUNT ]
    if len(opponents) == 0:
        print('It looks like it is impossible to rated player', p1[0])
        sys.exit(1)

    p2 = opponents[0]
    sparring(p1, p2)
    sparring(p2, p1)


def all_rated(players, rated):
    print('Not implemented: all rated')
    sys.exit(1)


def some_unrated(players, rated, unrated):
    print('Not implemented: some unrated')
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
