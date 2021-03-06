# Module:   server
# Date:     6th November 2008
# Author:   James Mills, prologic at shortcircuit dot net dot au

"""Web Server

This module implements the Web Server Component.
"""

import os
from socket import gethostname as _gethostname

from circuits.core import handler, BaseComponent

from circuits.net.sockets import TCPServer, Close

from http import HTTP
from wrappers import Request, Host
from constants import SERVER_VERSION
from dispatchers import Dispatcher

class BaseServer(BaseComponent):

    channel = "web"

    def __init__(self, bind, **kwargs):
        super(BaseServer, self).__init__(**kwargs)

        self.server = (TCPServer(bind, **kwargs) + HTTP())
        self += self.server

        Request.server = self
        Request.local = Host(self.server.bind[0], self.server.bind[1])
        Request.host = self.host

        print "%s listening on %s" % (self.version, self.base)

    @property
    def version(self):
        return SERVER_VERSION

    @property
    def address(self):
        return self.server.bind[0] if hasattr(self, "server") else None

    @property
    def port(self):
        return self.server.bind[1] if hasattr(self, "server") else None

    @property
    def ssl(self):
        return self.server.ssl if hasattr(self, "server") else None

    @property
    def scheme(self):
        return "https" if self.ssl else "http"

    @property
    def host(self):
        host = self.address

        if host in ("0.0.0.0", "::", ""):
            # 0.0.0.0 is INADDR_ANY and :: is IN6ADDR_ANY.
            # Look up the host name, which should be the
            # safest thing to spit out in a URL.
            host = _gethostname()

        ssl = self.ssl
        port = self.port

        if not ((ssl and port == 443) or (not ssl and port == 80)):
            host = "%s:%s" % (host, port)

        return host

    @property
    def base(self):
        host = self.host
        port = self.port
        scheme = self.scheme
        
        return "%s://%s" % (scheme, host)

    @handler("stopped", target="*")
    def stopped(self, manager):
        self.push(Close(), target=self.server)

class Server(BaseServer):

    def __init__(self, bind, **kwargs):
        super(Server, self).__init__(bind, **kwargs)

        Dispatcher().register(self)
