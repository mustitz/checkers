#!/usr/bin/env python3

import configargparse
import math
import os
import select
import subprocess
import sys
import time

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

shmArg = {
    'help' : 'Use SHM file for ETB.',
    'action' : 'store_true',
    'default' : False,
}

etbArg = {
    'help': 'ETB directory.',
    'default': None,
}

dirArg = {
    'help': 'directory with rus-draught application.',
    'default': '',
}

priorityArg = {
    'help': 'Game engine\'s process priority.',
    'type': int,
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
p.add('--shm', **shmArg)
p.add('-d', '--dir', **dirArg)
p.add('-p', '--priority', **priorityArg)
p.add('player1', **player1Arg)
p.add('player2', **player2Arg)

options = p.parse_args()

if len(options.player1) != 1:
    print('Wrong options.player1:', options.player1)
    sys.exit(1)
player1 = options.player1[0]

if len(options.player2) != 1:
    print('Wrong options.player2:', options.player2)
    sys.exit(1)
player2 = options.player2[0]

processes = {}


def print_output(output, po, n):
    if po:
        for line in output:
            print(f'{n}>  {line}')
    return output

def execute(p, cmd, po=False):
    global processes
    pinfo = processes[p.pid]
    pinfo.setdefault('stdin', []).append(cmd)
    n = pinfo['n']
    elf = pinfo['elf']

    cmd = cmd + '\nping\n'
    p.stdin.write(cmd.encode('utf-8'))
    p.stdin.flush()

    result = []
    for b in iter(p.stdout.readline, b''):
        line = b.decode('utf-8').rstrip()
        if line.lstrip() == 'pong':
            return print_output(result, po, n)
        result.append(line)

    print('Engine {n}:{elf} Command is not processed correctly.')
    print('Command is:', '\\n'.join(cmd.split('\n')))
    print('STDIN for process:')
    for line in pinfo.get('stdin', []):
        print(f'    {line}')
    raise AssertionError(f'Engine Command “{cmd}” is not processed correctly.')

def close(p):
    p.stdin.write(b'exit\n')
    p.stdin.close()
    p.stdout.close()
    p.wait()

def getId(p):
    lines = execute(p, 'ai info')
    for line in lines:
        parts = line.split()
        if parts[0] == 'id':
            return parts[1]
    return ''

def get_elf(lines):
    if len(lines) == 0:
        return ''
    head = lines[0]
    if len(head) == 0:
        return ''
    ch, tail = head[0], head[1:]
    if ch == '#':
        lines = lines[1:]
        return tail
    return ''

def initProcess(player, n):
    global options
    global processes

    with open(player, 'r') as f:
        setup = f.read()
    lines = setup.split('\n')
    elf = get_elf(lines)
    if not elf:
        if options.dir:
            elf = options.dir + '/rus-checkers'
        else:
            elf = 'rus-checkers'

    popenArgs = {
        'stdin': subprocess.PIPE,
        'stdout': subprocess.PIPE,
        'stderr': sys.stderr,
    }

    elf = os.path.expandvars(elf)
    p = subprocess.Popen(elf, **popenArgs)
    if not p:
        print('Popen fails for player', settings)
        sys.exit(1)
    processes.setdefault(p.pid, {})['elf'] = elf
    processes.setdefault(p.pid, {})['n'] = n

    if not options.priority is None:
        execute(p, 'set priority ' + str(options.priority), True)

    if options.shm:
        execute(p, 'etb shm use', True)
    elif options.etb:
        execute(p, 'etb load ' + options.etb, True)

    for cmd in lines:
        execute(p, cmd, True)

    return p, getId(p)

class MoveStats:
    def __init__(self):
        self.move_count = 0
        self.time = 0.0

    def add_move(self, time):
        self.move_count = self.move_count + 1
        self.time += time

    def value(self):
        if self.move_count == 0:
            return 'N/A'
        value = self.time / self.move_count
        return "%.6fs/mv" % value

def aiSelect(p, game):
    start = time.time()
    move = execute(p, 'ai select')[0]
    end = time.time()

    game.append(move)
    return move, end - start

def runGame(p1, moveStats1, p2, moveStats2):
    global options
    moveCount = 1
    fen = 'W:Wa1,a3,b2,c1,c3,d2,e1,e3,f2,g1,g3,h2:Ba7,b6,b8,c7,d6,d8,e7,f6,f8,g7,h6,h8'
    execute(p1, 'fen ' + fen)
    execute(p2, 'fen ' + fen)

    game = []

    while True:
        moves = execute(p1, 'move list')
        if not moves:
            return game, -1,

        game.append(str(moveCount) + '.')

        move_indexes = {}
        for move in moves:
            parts = move.split()
            move_indexes[parts[1]] = parts[0]

        move, time = aiSelect(p1, game)
        moveStats1.add_move(time)

        execute(p2, 'move select ' + move_indexes[move])

        moves = execute(p2, 'move list')
        if not moves:
            return game, +1

        move_indexes = {}
        for move in moves:
            parts = move.split()
            move_indexes[parts[1]] = parts[0]

        move, time = aiSelect(p2, game)
        moveStats2.add_move(time)

        execute(p1, 'move select ' + move_indexes[move])

        moveCount = moveCount + 1
        if moveCount > options.max_moves:
            return game, 0

def multiGames(p1, id1, p2, id2, count):
    win = 0
    draw = 0
    loose = 0

    moveStats1 = MoveStats()
    moveStats2 = MoveStats()

    games = []
    result_dict = { -1: "0-2", 0: "1-1", +1: "2-0" }

    for i in range(1, count+1):
        pdn = []
        is_swap = (id1 != id2) and (i % 2 == 0)
        if not is_swap:
            pdn.append('[White "%s"]' % id1)
            pdn.append('[Black "%s"]' % id2)
            game, result = runGame(p1, moveStats1, p2, moveStats2)
            result_str = result_dict[result]
            pdn.append('[Result "%s"]' % result_str)
            game.append(result_str)
        else:
            pdn.append('[White "%s"]' % id2)
            pdn.append('[Black "%s"]' % id1)
            game, result = runGame(p2, moveStats2, p1, moveStats1)
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

    return games, win, loose, draw, moveStats1, moveStats2

p1, id1 = initProcess(player1, 1)
print("Player 1:", id1)

p2, id2 = initProcess(player2, 2)
print("Player 2:", id2)

games, win, loose, draw, ms1, ms2 = multiGames(p1, id1, p2, id2, options.count)
if options.log:
    with open(options.log, 'w') as logFile:
        logFile.write("\n\n\n".join(games))
        logFile.write("\n")

isSmashing = False
if (loose == 0) and (draw == 0):
    isSmashing = True
    elo = '+INF'
if (win == 0) and (draw == 0):
    isSmashing = True
    elo = '-INF'
if not isSmashing:
    result1 = (win + 0.5*draw) / (win + draw + loose)
    delta = 400 * math.log10(result1/(1-result1))
    if delta == 0:
        elo = "0"
    else:
        if delta > 0.0:
            sign = '+'
        if delta < 0.0:
            sign = '-'
            delta = -delta
        elo = sign + "%.2f" % delta

args = win, loose, draw, ms1.value(), ms2.value(), elo
print('+%d -%d =%d %s %s ELO %s' % args)

close(p1)
close(p2)
