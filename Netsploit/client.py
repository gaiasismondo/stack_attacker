import time
import copy
from util import Constants as C

from util import Logger
from pymetasploit3.msfrpc import MsfRpcClient

from requests.exceptions import ConnectionError


class MetasploitWrapper:
    """
    Abstraction layer for Metasploit Client interactions
    """
    # Delay in seconds before checking for open sessions
    GET_SESSIONS_DELAY = 1
    READ_CONSOLE_DELAY = 1
    READ_CONSOLE_BUSY_ATTEMPTS = 5

    def __init__(self, password, port=55553, server="0.0.0.0", ssl=True):
        try:
            self.client = MsfRpcClient(password, port=port, server=server, ssl=ssl)
        except ConnectionError as e:
            print(f"{C.COL_RED}[-] Can't connect to msf rpc server @{server}:{port} with password: {password}{C.COL_RESET}")
            Logger.log(self, f"RPC client connection error - {port=}, {server=}, {ssl=}", level=Logger.ERROR)
            exit(1)

        Logger.log(self, f"RPC client connected - {port=}, {server=}, {ssl=}", level=Logger.INFO)

        self.cid = self.client.consoles.console().cid
        Logger.log(self, f"Console created - {self.cid=}", level=Logger.INFO)

        # flush the banner
        self.client.consoles.console(self.cid).read()
        

    def get_active_sessions(self, sleep=True):
        """
        Returns a list of open sessions associated with the client
        """
        if sleep:
            time.sleep(MetasploitWrapper.GET_SESSIONS_DELAY)
        if self.client:
            return copy.deepcopy(self.client.sessions.list)
    

    #print route for debugging 
    def route_print(self):
        self.client.consoles.console(self.cid).write(C.ROUTE_PRINT)
        routes=self.client.consoles.console(self.cid).read()
        #print(routes)
    

    def route_add(self,sess,target_ip):
        #print(C.ROUTE_ADD.format(target_ip,sess))
        self.client.consoles.console(self.cid).write(C.ROUTE_ADD.format(target_ip,sess))
        routes=self.client.consoles.console(self.cid).read()
        #print(routes)
    def route_flush(self):
        self.client.consoles.console(self.cid).write("route flush")
        routes=self.client.consoles.console(self.cid).read()
        #print(routes)
    #crea un port forwarding sulla shell della sessione passata
    def add_portfwd(self,sess, cmd):
        self.route_flush()
        self.client.sessions.session(sess).write(cmd)
        portfwd=self.client.sessions.session(sess).read()
        #print(portfwd)
    
