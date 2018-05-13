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

def runGame(p1, p2):
    global options
    moveCount = 1
    fen = 'W:Wa1,a3,b2,c1,c3,d2,e1,e3,f2,g1,g3,h2:Ba7,b6,b8,c7,d6,d8,e7,f6,f8,g7,h6,h8'
    execute(p1, 'fen ' + fen)
    execute(p2, 'fen ' + fen)

    game = []

    while True:
        moves = execute(p1, 'move list')
        if not moves:
            return game, -1

        game.append(str(moveCount) + '.')

        move_indexes = {}
        for move in moves:
            parts = move.split()
            move_indexes[parts[1]] = parts[0]

        move = execute(p1, 'ai select')[0]
        game.append(move)

        execute(p2, 'move select ' + move_indexes[move])

        moves = execute(p2, 'move list')
        if not moves:
            return game, +1

        move_indexes = {}
        for move in moves:
            parts = move.split()
            move_indexes[parts[1]] = parts[0]

        move = execute(p2, 'ai select')[0]
        game.append(move)

        execute(p1, 'move select ' + move_indexes[move])

        moveCount = moveCount + 1
        if moveCount > options.max_moves:
            return game, 0

def multiGames(p1, id1, p2, id2, count):
    win = 0
    draw = 0
    loose = 0

    games = []
    result_dict = { -1: "0-2", 0: "1-1", +1: "2-0" }

    for i in range(1, count+1):
        pdn = []
        is_swap = (id1 != id2) and (i % 2 == 0)
        if not is_swap:
            pdn.append('[White "%s"]' % id1)
            pdn.append('[Black "%s"]' % id2)
            game, result = runGame(p1, p2)
            result_str = result_dict[result]
            pdn.append('[Result "%s"]' % result_str)
            game.append(result_str)
        else:
            pdn.append('[White "%s"]' % id2)
            pdn.append('[Black "%s"]' % id1)
            game, result = runGame(p2, p1)
            result_str = result_dict[result]
            pdn.append('[Result "%s"]' % result_str)
            game.append(result_str)
            result = -result
        win += result == +1
        loose += result == -1
        draw += result == 0
        pdn.append('')
        pdn.append(" ".join(game))
        games.append("\n".join(pdn))

    return games, win, loose, draw

p1, id1 = initProcess(rusCheckers, player1)
print("Player1:", id1)

p2, id2 = initProcess(rusCheckers, player2)
print("Player2:", id2)

games, win, loose, draw = multiGames(p1, id1, p2, id2, options.count)
if options.log:
    logFile = open(options.log, 'w')
    logFile.write("\n\n\n".join(games))
    logFile.close()

print('+%d -%d =%d 1s/move 1s/move ELO 0' % (win, loose, draw))

close(p1)
close(p2)

#Planned output
#id1 id2 +win -loose =draw 0.001s/move 0.1s/move ELO +/-500
