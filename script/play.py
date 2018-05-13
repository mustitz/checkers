#!/usr/bin/env python3

import configargparse

parserArgs = {
    'add_config_file_help': False,
    'add_env_var_help': False,
    'auto_env_var_prefix': 'CHECKERS_',
    'config_file_parser_class': configargparse.YAMLConfigFileParser,
    'default_config_files': [ '/etc/checkers.yaml', '~/.config/checkers.yaml' ],
}

countArg = {
    'help': 'Amount of game to play.',
    'type': int,
    'default': 1,
}

swapArg = {
    'help': 'Swap colors after each game.',
    'action': 'store_true',
    'default': True,
}

noSwapArg = {
    'help': 'Play all games with fixed colors.',
    'dest': 'swap',
    'action': 'store_false',
}

logArg = {
    'help': 'File to save games played.',
    'default': '',
}

maxMovesArg = {
    'help': 'Maximum game length.',
    'type': int,
    'default': 100,
}

etbArg = {
    'help': 'ETB directory.',
    'default': '.',
}

dirArg = {
    'help': 'directory with rus-draught application.',
    'default': '.',
}

player1Arg = {
    'help': 'File with first player settings.',
    'nargs': 1,
}

player2Arg = {
    'help': 'File with second player settings.',
    'nargs': 1,
}

p = configargparse.ArgParser(**parserArgs)
p.add('-c', '--count', **countArg)
p.add('-s', '--swap', **swapArg)
p.add('--no-swap', **noSwapArg)
p.add('-l', '--log', **logArg)
p.add('-m', '--max-moves', **maxMovesArg)
p.add('-e', '--etb', **etbArg)
p.add('-d', '--dir', **dirArg)
p.add('player1', **player1Arg)
p.add('player2', **player2Arg)

options = p.parse_args()

#Planned output
#id1 id2 +win -loose =draw 0.001s/move 0.1s/move ELO +/-500
