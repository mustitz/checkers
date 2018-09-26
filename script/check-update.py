#!/usr/bin/env python3

from configargparse import ArgParser
from subprocess import check_output

import os
import sys
import yaml


count_arg = {
    'help': 'Amount of game to play.',
    'type': int,
    'default': 2,
}

parser_args = {
    'add_config_file_help': False,
    'add_env_var_help': False,
    'auto_env_var_prefix': 'CHECKERS_',
}

check_dir_arg = {
    'help': 'Tournament directory.',
    'nargs': '?',
    'default' : '.',
}


p = ArgParser(**parser_args)
p.add('-c', '--count', **count_arg)
p.add('check_dir', **check_dir_arg)
options = p.parse_args()
os.chdir(options.check_dir)

M_set       = [ '1', '6', '21' ]
multipliers = [ 10,   4,    1  ]
C_set       = [ '1.1', '1.3', '1.5' ]

for M, multiplier in zip(M_set, multipliers):
    for C in C_set:
        script = [
            'set ai mcts',
            'set ai.use_etb 6',
            'set ai.C ' + C,
            'set ai.mnodes ' + M,
        '' ]
        new_script = ['#./new/rus-checkers'] + script + ['set ai.smooth 1', '']
        old_script = ['#./old/rus-checkers'] + script
        with open('new.player', 'w') as f:
            f.write('\n'.join(new_script))
        with open('old.player', 'w') as f:
            f.write('\n'.join(old_script))

        name = 'c' + C + '-n' + M + 'M'
        fmt = 'play.py new.player old.player --shm -p19 -c{} -l {}.pdn >>log.txt 2>>errors.txt'
        cmd = fmt.format(multiplier * options.count, name)
        check_output(cmd, shell=True)
