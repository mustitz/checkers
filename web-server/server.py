#!/usr/bin/env python3


from email.utils import formatdate
from http.server import BaseHTTPRequestHandler, HTTPServer
from os import stat
from os.path import abspath
from socketserver import ThreadingMixIn
from shutil import copyfileobj
from stat import S_ISREG


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

        self.send_response(200, 'OK')
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', st.st_size)
        self.send_header('Last-Modified', formatdate(st.st_mtime))
        self.end_headers()
        with open(path, 'rb') as content:
            copyfileobj(content, self.wfile)


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    """Handle requests in a separate thread."""


Cfg.init()

address = ('0.0.0.0', 8000)
httpd = ThreadedHTTPServer(address, Handler)
httpd.serve_forever()
