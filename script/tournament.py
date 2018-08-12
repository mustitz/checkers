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

tournamentDirArg = {
    'help': 'Tournament directory.',
    'nargs': '?',
    'default' : '.',
}


p = configargparse.ArgParser(**parserArgs)
p.add('tournamentDir', **tournamentDirArg)


options = p.parse_args()
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


def split_by_rated(players):
    is_unrated = lambda pair: pair[1].get('elo') is None
    is_rated = lambda pair: not is_unrated(pair)
    rated = [ element for element in players.items() if is_rated(element) ]
    unrated = [ element for element in players.items() if is_unrated(element) ]
    return rated, unrated


def all_unrated(players, unrated):
    print('Not implemented all unrated')
    sys.exit(1)


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
