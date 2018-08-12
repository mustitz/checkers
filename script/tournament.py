#!/usr/bin/env python3

from glob import glob

import configargparse
import os
import sys


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


players = read_players()
load_assume_rank(players)
