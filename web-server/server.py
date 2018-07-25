#!/usr/bin/env python3


import hashlib
import json
import re
import subprocess
import sys

from email.utils import formatdate
from http.server import BaseHTTPRequestHandler, HTTPServer
from os import stat
from os.path import abspath
from socketserver import ThreadingMixIn
from shutil import copyfileobj
from stat import S_ISREG
from threading import Lock
from time import time


class Cfg:
    @classmethod
    def init(cls):
        cls.content_types = {
            'html' : 'text/html; charset=utf-8',
            'js'   : 'application/javascript',
            'ico'  : 'image/x-icon',
            'png'  : 'image/png',
        }

        cls.base_path = abspath('.')
        if cls.base_path[-1] != '/':
            cls.base_path += '/'

        cls.start_fen = 'W:Wa1,a3,b2,c1,c3,d2,e1,e3,f2,g1,g3,h2:Ba7,b6,b8,c7,d6,d8,e7,f6,f8,g7,h6,h8';


def safe_cast(val, to_type, default=None):
    try:
        return to_type(val)
    except (ValueError, TypeError):
        return default


class SessionManager:
    @classmethod
    def init(cls):
        cls.lock = Lock()
        cls.sessions = {}
        pass

    @classmethod
    def get(cls, sid):
        cls.lock.acquire()
        try:
            obj = cls.sessions.get(sid)
            if type(obj) == dict:
                obj['last_access'] = time()
                cls.sessions[sid] = obj
            return obj
        finally:
            cls.lock.release()

    @classmethod
    def set(cls, sid, obj):
        obj['last_access'] = time()
        cls.lock.acquire()
        try:
            cls.sessions[sid] = obj
        finally:
            cls.lock.release()


class RusCheckers:
    @classmethod
    def init(cls):
        cls.lock = Lock()

        popenArgs = {
            'stdin': subprocess.PIPE,
            'stdout': subprocess.PIPE,
            'stderr': sys.stderr,
        }

        cls.p = subprocess.Popen('rus-checkers', **popenArgs)
        if not cls.p:
            print('Popen fails for player', settings)
            sys.exit(1)

        cmd = 'etb shm use'
        log = cls.execute(cmd)
        print(cmd)
        for line in log:
            print(line)

        cls.execute('set ai mcts')
        cls.execute('ai set simul_count 250000')
        cls.execute('ai set use_etb 6')

        cls.id_lines = cls.execute('ai info')
        cls.ai_id = ''
        for line in cls.id_lines:
            print(line)
            parts = line.split()
            if parts[0] == 'id':
                cls.ai_id = parts[1]

    @classmethod
    def execute(cls, cmd):
        cmd = cmd + '\nping\n'
        cls.p.stdin.write(cmd.encode('utf-8'))
        cls.p.stdin.flush()

        result = []
        for b in iter(cls.p.stdout.readline, b''):
            line = b.decode('utf-8').rstrip()
            if line.lstrip() == 'pong':
                return result
            result.append(line)

        result.append('???')
        return result

    @classmethod
    def check(cls, old_fen, move, new_fen):
        cls.lock.acquire()
        try:
            output = cls.execute('fen ' + new_fen)
            if len(output) != 0:
                return False;

            output = cls.execute('fen')
            if len(output) != 1:
                return False
            new_fen = output[0].strip()

            output = cls.execute('fen ' + old_fen)
            if len(output) != 0:
                return False

            output = cls.execute('move list')
            if len(output) == 0:
                return False

            moves = {}
            for line in output:
               parts = line.split()
               n = int(parts[0])
               possible_move = parts[1]
               if possible_move == move:
                   moves = { possible_move : n }
                   break
               moves[possible_move] = n

            for move, num in moves.items():
                output = cls.execute('move select ' + str(num))
                if len(output) != 0:
                    return False
                output = cls.execute('fen')
                if len(output) != 1:
                    return False
                fen = output[0].strip()
                if fen == new_fen:
                    return True
                output = cls.execute('fen ' + old_fen)
                if len(output) != 0:
                    return False

            return False

        finally:
            cls.lock.release()

    @classmethod
    def play(cls, fen):
        cls.lock.acquire()
        try:
            output = cls.execute('fen ' + fen)
            if len(output) != 0:
                return '', 'error', 'Invalid FEN'

            active = fen[0]
            output = cls.execute('move list')
            if len(output) == 0:
                return fen, 'pass', '1-0' if active == 'B' else '0-1'

            output = cls.execute('ai select')
            if len(output) != 1:
                return '', 'error', 'Wrong AI output'
            move = output[0].strip()

            output = cls.execute('fen')
            if len(output) != 1:
                return '', 'error', 'No FEN'
            fen = output[0].strip()

            output = cls.execute('move list')
            if len(output) == 0:
                result = '1-0' if active == 'W' else '0-1'
            else:
                result = '*'

            return fen, move, result

        finally:
            cls.lock.release()


class Handler(ThreadingMixIn, BaseHTTPRequestHandler):
    def error_404(self):
        self.send_response(404, 'Not Found')
        self.end_headers()

    def do_GET(self):
        self.protocol_version = 'HTTP/1.1'
        if self.path == '/':
            self.path = '/index.html'
        path = abspath(Cfg.base_path + '/' + self.path)
        if not path.startswith(Cfg.base_path):
            return self.error_404()

        extension = self.path.split('.')[-1]
        content_type = Cfg.content_types.get(extension, '')
        if not content_type:
            return self.error_404()

        st = stat(path)
        if not S_ISREG(st.st_mode):
            return self.error_404()
        content_length = st.st_size

        last_modified = formatdate(st.st_mtime)

        if extension == 'html':
            last_modified = formatdate()
            host, port = self.client_address
            hash_items = [ path, str(host), str(port), last_modified]
            hash_text = ':'.join(hash_items)
            hash_data = hash_text.encode('utf-8')
            sid = hashlib.sha512(hash_data).hexdigest()
            SessionManager.set(sid, {'status':'NEW'})

            with open(path, 'r') as f:
                content = f.read()
            content = re.sub(r'\$SID\b', sid, content)
            content = content.encode('utf-8')
            content_length = len(content)

        self.send_response(200, 'OK')
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', content_length)
        self.send_header('Last-Modified', last_modified)
        self.end_headers()

        if extension == 'html':
            self.wfile.write(content)
        else:
            with open(path, 'rb') as content:
                copyfileobj(content, self.wfile)

    def out_json(self, obj, qid = ''):
        if qid:
            obj['id'] = qid
        json_text = json.dumps(obj) + '\n'
        content = json_text.encode('utf-8')
        self.send_response(200, 'OK')
        self.send_header('Content-Type', 'appllication/json')
        self.send_header('Content-Length', len(content))
        self.end_headers()
        self.wfile.write(content)

    def do_POST(self):
        self.protocol_version = 'HTTP/1.1'
        if self.path != '/':
            return error_404(self)

        content_length_str = self.headers.get('Content-Length', '-1')
        content_length = safe_cast(content_length_str, int, -1)
        if content_length <= 0:
            return self.out_json({'error': 'Wrong Content-Length.'})

        content_type = self.headers.get('Content-Type', '')
        if content_type != 'application/json':
            return self.out_json({'error': 'Wrong Content-Type.'})

        json_bytes = self.rfile.read(content_length)
        json_text = json_bytes.decode('utf-8')
        data = json.loads(json_text)
        qid = data.get('id', '')
        if type(qid) != str:
            return self.out_json({'error': 'Wrong id type, string or empty expected.'})

        sid = data.get('sid')
        if type(sid) != str:
            return self.out_json({'error': 'Wrong sid type, string expected.'}, qid)

        session = SessionManager.get(sid)
        if type(session) != dict:
            return self.out_json({'error': 'Sid not found, please refresh a page.'}, qid)

        status = session.get('status')
        old_fen = session.get('fen', Cfg.start_fen)
        new = data.get('new')
        if status == 'NEW' and type(new) != str:
            return self.out_json({'error': 'Wrong new type, string expected.'}, qid)

        if type(new) == str:
            old_fen = Cfg.start_fen
            if new == 'WHITE':
                status = 'WAIT'
            if new == 'BLACK':
                status = 'SKIP_WAIT'
                new_fen = old_fen
            if status == 'NEW':
                return self.out_json({'error': 'Wrong new type, WHITE or BLACK expected.'}, qid)

        if status == 'WAIT':
            move = data.get('move')
            if type(move) != str:
                return self.out_json({'error': 'Wrong move type, string expected.'}, qid)

            new_fen = data.get('fen')
            if type(move) != str:
                return self.out_json({'error': 'Wrong fen type, string expected.'}, qid)

            if not RusCheckers.check(old_fen, move, new_fen):
                return self.out_json({'error': 'Wrong move & fen.'}, qid)

        fen, move, result = RusCheckers.play(new_fen)
        if move == 'error':
            return self.out_json({'error': result}, qid)

        if result == '*':
            status = 'WAIT'
        else:
            status = 'NEW'

        session['status'] = status
        session['fen'] = fen

        answer = {}
        if qid:
            answer['id'] = qid
        answer['fen'] = fen
        answer['move'] = move
        answer['result'] = result

        SessionManager.set(sid, session)
        return self.out_json(answer, qid)


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    """Handle requests in a separate thread."""


Cfg.init()
SessionManager.init()
RusCheckers.init()

address = ('0.0.0.0', 8000)
httpd = ThreadedHTTPServer(address, Handler)
httpd.serve_forever()
