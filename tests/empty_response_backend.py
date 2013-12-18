#!/usr/bin/python

import BaseHTTPServer

class EmptyResponseHandler(BaseHTTPServer.BaseHTTPRequestHandler):

    def send_empty_response(self):
        self.send_response(204)
        self.end_headers()

    def do_GET(self):
        self.send_empty_response()

    def do_HEAD(self):
        self.send_empty_response()

    def do_POST(self):
        self.send_empty_response()

if __name__ == '__main__':
    BaseHTTPServer.test(EmptyResponseHandler)
