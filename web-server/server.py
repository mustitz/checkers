#!/usr/bin/env python3


from socketserver import ThreadingMixIn
from http.server import BaseHTTPRequestHandler, HTTPServer


class Handler(ThreadingMixIn, BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200, 'OK')
        self.send_header('Content-type', 'text/plain; charset=utf-8')
        self.end_headers()
        self.wfile.write('Not implemented!\n'.encode('utf-8'))
        with open(path, 'rb') as content:
            copyfileobj(content, self.wfile)


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    """Handle requests in a separate thread."""


address = ('0.0.0.0', 8000)
httpd = ThreadedHTTPServer(address, Handler)
httpd.serve_forever()
