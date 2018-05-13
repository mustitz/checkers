#!/usr/bin/env python3

import configargparse
import os
import select
import subprocess

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
    'default': '',
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

if options.dir:
    rusCheckers = os.path.expandvars(options.dir + '/rus-checkers')
else:
    rusCheckers = 'rus-checkers'

if len(options.player1) != 1:
    print('Wrong options.player1:', options.player1)
    sys.exit(1)
player1 = options.player1[0]

if len(options.player2) != 1:
    print('Wrong options.player2:', options.player2)
    sys.exit(1)
player2 = options.player2[0]



def execute(p, cmd):
    cmd = cmd + '\nping\n'
    p.stdin.write(cmd.encode('utf-8'))
    p.stdin.flush()

    result = []
    for b in iter(p.stdout.readline, b''):
        line = b.decode('utf-8').strip()
        if line == 'pong':
            return result
        result.append(line)

    result.append('???')
    return result

def close(p):
    p.stdin.write(b'exit\n')
    p.stdin.close()
    p.wait()

def getId(p):
    lines = execute(p, 'ai info')
    for line in lines:
        parts = line.split()
        if parts[0] == 'id':
            return parts[1]
    return ''

def initProcess(cmd, player):
    popenArgs = {
        'stdin': subprocess.PIPE,
        'stdout': subprocess.PIPE,
    }

    p = subprocess.Popen(rusCheckers, **popenArgs)
    if not p:
        print('Popen fails for player', settings)
        sys.exit(1)

    global options
    if options.etb:
        execute(p, 'etb load ' + options.etb)

    setup = open(player).read()
    execute(p, setup)

    return p, getId(p)

p1, id1 = initProcess(rusCheckers, player1)
print("Player1:", id1)

p2, id2 = initProcess(rusCheckers, player2)
print("Player2:", id2)

close(p1)
close(p2)

#Planned output
#id1 id2 +win -loose =draw 0.001s/move 0.1s/move ELO +/-500
