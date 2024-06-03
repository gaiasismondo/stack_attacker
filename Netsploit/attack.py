from abc import ABC
from abc import abstractmethod
import re
from time import sleep
from contextlib import contextmanager
import signal
from util import time_limit
import json
from util import Logger


#Classe astratta
#Un attacco ha proprietà: nome, istruzioni, tempo di attesa e tipo
#Le sottoclassi dovranno implementare i metodi execute (esecuzione dell'attacco) e check (verifica dell'esito dell'attacco)
class Attack(ABC):
    def __init__(self,name,instructions,wait_time,attack_type=None):
        self.attack=name
        self.instruction=instructions
        self.wait_time=wait_time
        self.attack_type=attack_type
        
    def execute(self):
        raise NotImplementedError("Use specific attacks!")

    def check(self):
        raise NotImplementedError("Use specific attacks!")
    

#Estende la classe Attack e fornisce l'implementazione specifica per eseguire un attacco usando il framework Metasploit
class Metasploit_Attack(Attack):
    def __init__(self, name, instructions, wait_time=10, client=None):
        
        super().__init__(name, instructions, wait_time=wait_time)
        self.client=client
        self.output=""

    #instr_list è un array con le istruzioni recuperate da attacK_db.json (separate per \n)
    #se l'istruzione contiene setg salva nome parametro e valore nel dizionario settings
    #se l'istruzione contiene use salva chiave "exploit" e valore nel dizionario settings 
    #viene restiruito settings
    def getSettings(self,instr_list):
        
        settings={}
        found=False        
        for i in instr_list:
            val=i.partition("setg")[2].strip().partition(" ")
            if(val[0]):
                settings[val[0]]=val[2]
            if(not found):
                exploit_req=[m.start() for m in re.finditer('use', i)]
                if(exploit_req): 
                    found=True
                    settings["exploit"]=i.partition("use")[2].strip().partition(" ")[0][8:]
        return settings

    #instr_list è un array con le istruzioni recuperate da attacK_db.json (separate per \n)
    #se l'istruzione contiene setg salva nome parametro e valore nel dizionario settings
    #se l'istruzione contiene use salva chiave "auxiliary" e valore nel dizionario settings 
    #viene restiruito settings
    def getSettingScan(self,instr_list):
        
        settings={}
        found=False        
        for i in instr_list:
            val=i.partition("setg")[2].strip().partition(" ")
            if(val[0]):
                settings[val[0]]=val[2]
            if(not found):
                exploit_req=[m.start() for m in re.finditer('use', i)]
                if(exploit_req): 
                    found=True
                    settings["auxiliary"]=i.partition("use")[2].strip().partition(" ")[0][10:]
        return settings

    #Viene recuperato l'elenco delle sessioni attive prima di eseguire le istruzioni(old_sess)
    #Viene richiamato il metodo getSettings e viene recuperato il dizionario con le impostazioni(settings)
    #Se si tratta di un resource script si interrompe l'esecuzione
    #Vengono settati payload(con eventuale porta) ed exploit se presenti in settings
    #Vengono settate eventuali impostazioni dell'exploit presenti in settings
    #Viene eseguito l'exploit con il payload specificato e il risultato viene salvato in output
    def execute(self):
        """
        Executes each instruction of an IB-attack
        """
        old_sess=self.client.get_active_sessions()
        instr_str=self.instruction
        instr_list=instr_str.split("\n")
        settings=self.getSettings(instr_list)
        
        if("resource" in instr_list): return
        if("payload" in settings):
            payload=self.client.client.modules.use('payload', settings["payload"])
            Logger.log(self, f"using payload - {settings['payload']}", level=Logger.INFO)
            if("LPORT" in settings):
                payload.runoptions["LPORT"]=settings["LPORT"]
        exploit={}
        if("exploit" in settings):
            exploit=self.client.client.modules.use("exploit",settings["exploit"])
            Logger.log(self, f"using exploit - {settings['exploit']}", level=Logger.INFO)
        
        for key in settings.keys():
            if(key=="payload" or key=="exploit" or key=="LPORT"):
                continue
            exploit[key]=settings[key]
         
        self.output=self.client.client.consoles.console(self.client.cid).run_module_with_output(exploit, payload=payload)
        
        return self.check(old_sess)
       
    #Verifica se siano state create nuove sessioni (attacco andato a buon fine)
    #Se si vengono inserite nel dizionario session le informazioni sulla nuova sessione e viene restituito
    #Altrimenti viene restituito il dizionario vuoto
    def check(self,old_sess):
    
        session={}
        new_sess=self.client.get_active_sessions()
        diff=set(new_sess)-set(old_sess)
        
        if diff:
            session["id_sess"]=diff.pop()
            Logger.log(self, f"session created - {session['id_sess']}", level=Logger.INFO)
            obtained_session = new_sess[session["id_sess"]]
            session["obtained_session"]=obtained_session
            session["session_host"]=obtained_session["session_host"]
            return session

        else:
            Logger.log(self, f"unable to create session", level=Logger.INFO)
            return session
    
    #Viene richiamato il metodo getSettings e viene recuperato il dizionario con le impostazioni(settings)
    #Vengono settati payload(con eventuale porta) ed exploit auxiliary se presenti in settings
    #Vengono settate eventuali impostazioni dell'exploit auxiliary presenti in settings
    #Viene eseguita la scansione ma il risultato viene ignorato
    def scan(self):
        instr_str=self.instruction
        instr_list=instr_str.split("\n")
        settings=self.getSettingScan(instr_list)
        if("payload" in settings):
            payload=self.client.client.modules.use('payload', settings["payload"])
            Logger.log(self, f"using payload - {settings['payload']}", level=Logger.INFO)
            if("LPORT" in settings):
                payload.runoptions["LPORT"]=settings["LPORT"]
        exploit={}
        if("auxiliary" in settings):
            exploit=self.client.client.modules.use("auxiliary",settings["auxiliary"])
            Logger.log(self, f"using scan - {settings['auxiliary']}", level=Logger.INFO)
        
        for key in settings.keys():
            if(key=="payload" or key=="auxiliary" or key=="LPORT"):
                continue
            exploit[key]=settings[key]
        #poiché non ci interessa l'output della scansione la facciamo partire ma non leggiamo nulla, non blocca l'esecuzione
        #degli exploit successivi poiché la console non diventa busy
        exploit.execute()
        sleep(2)
        return


#Estende la classe Metasploit_Attack e fornisce l'implementazione specifica per eseguire un moduli che utilizzano un resource script 
class ResourceAttack(Metasploit_Attack):
    LONG_SLEEP_TIME=15
    SHORT_SLEEP_TIME=5
    def __init__(self, name, instructions, time_wait=10,client=None):
        super().__init__(name,instructions, wait_time=time_wait)
        self.client=client
        self.output=[]

    #Viene recuperato l'elenco delle sessioni attive prima di eseguire le istruzioni(old_sess)
    #Vengono recuperate le istruzioni da eseguire e vengono scritte sulla console Metasploit
    #L'output di ogni istruzione viene salvato nell'array out
    #Pone un limite di 300 secondi allo svolgimento dell'attacco
    #Verifica se siano state create nuove sessioni (attacco andato a buon fine)
    def execute(self):
        old_sess=self.client.get_active_sessions()
        instr_str=self.instruction
        instr_list=instr_str.split("\n")
        
        _ = self.client.client.consoles.console(self.client.cid).read()

        for i in instr_list:
            self.client.client.consoles.console(self.client.cid).write(i)
            if("resource" in i or "exploit" in i or "run" in i):
                sleep(self.LONG_SLEEP_TIME)
            else:
                sleep(self.SHORT_SLEEP_TIME)
                   
            out=self.client.client.consoles.console(self.client.cid).read()
            #print(out)
            self.output.append(out["data"])
        
        with time_limit(300):
            while self.client.client.consoles.console(self.client.cid).is_busy():
                sleep(1)
                
        out = self.client.client.consoles.console(self.client.cid).read()
        
        self.output.append(out["data"])
        
        return self.check(old_sess)
    
    #Verifica se siano state create nuove sessioni (attacco andato a buon fine)
    #Se si vengono inserite nel dizionario session le informazioni sulla nuova sessione e viene restituito
    #Altrimenti viene restituito il dizionario vuoto
    def check(self,old_sess):
        
        session={}
        new_sess=self.client.get_active_sessions()
        diff=set(new_sess)-set(old_sess)
        
        
        if diff:
            session["id_sess"]=diff.pop()
            Logger.log(self, f"session created - {session['id_sess']}", level=Logger.INFO)
            obtained_session = new_sess[session["id_sess"]]
            session["obtained_session"]=obtained_session
            
            session["session_host"]=obtained_session["session_host"]
            return session

        else:
            Logger.log(self, f"unable to create session", level=Logger.INFO)
            return session
        

#Estende la classe Attack e fornisce l'implementazione specifica per eseguire un attacco da un client compromesso  
class VictimAttack(Attack):
    """
    Abstract class defining Out-Of-Band attacks from a compromised client.
    """
    def __init__(self, name, instructions, time_wait=10):
        super().__init__(name,instructions, wait_time=time_wait)
        self.out = []

    @abstractmethod
    def check(self):
        pass

    @abstractmethod
    def execute(self):
        pass


#Estende la classe Attack e fornisce l'implementazione per attacchi SSH Out Of Band
class SshAttack(VictimAttack):
    """
    SSH Login OOB attack implementation through console interaction.
    For attacks that won't work properly in a standard execution context.
    """
    SLEEP_TIME = 5

    def __init__(self,name, instructions, ip, session, time_wait=None,client=None):
        # the execute function waits SLEEP_TIME seconds for each command executed
        # 10 seconds are just to consider delays
        time_wait = (len(instructions) * SshAttack.SLEEP_TIME) + 10
        super().__init__(name,instructions, time_wait=time_wait)
        self.out = []
        self.client=client
        self.instructions = instructions
        self.session = session
        self.ip = ip
        self.is_non_standard = True

        if type(self.session) == str:
            raise TypeError

    #Verifica e restituisce se l'attacco ha avuto successo
    def checkSSH(self):
        """
        Checks if the attack succesfully logged in.
        The check is done on the exit status of the ssh connection command.
        """
        # Checks if exit code of ssh login is 0 (succesful login)
        if self.out[-1:] == ['0\n']:
            return True
        else:
            return False

    #Itera attraverso le istruzioni e le esegue sulla sessione meterpreter
    def execute(self):
        """
        Executes each instruction of an attack on a console.
        Sleeps after each instruction to be able to capture possible outputs.
        """
        # Run a shell command within a meterpreter session
        
        for c in self.instructions:
            self.client.client.sessions.session.write(c) 
            sleep(SshAttack.SLEEP_TIME)
            y = self.client.client.sessions.session.read()
            self.out.append(y)

        return self.check
    
    #Verifica se siano state create nuove sessioni (attacco andato a buon fine)
    #Se si vengono inserite nel dizionario session le informazioni sulla nuova sessione e viene restituito
    #Altrimenti viene restituito il dizionario vuoto
    def check(self,old_sess):
        session={}
        new_sess=self.client.get_active_sessions()
        diff=set(new_sess)-set(old_sess)       
        if diff:
            session["id_sess"]=diff.pop()
            Logger.log(self, f"session created - {session['id_sess']}", level=Logger.INFO)
            obtained_session = new_sess[session["id_sess"]]
            session["obtained_session"]=obtained_session
            
            session["session_host"]=obtained_session["session_host"]
            return session
        else:
            Logger.log(self, f"unable to create session", level=Logger.INFO)
            return session

"""

#Carica i dati da attack_db.json
#Crea un dizionario degli attacchi e per ogni attacco crea un istanza della classe Attack 
#Crea un dizionario delle scansioni e per ogni scansione crea un istanza della classe Attack 
#Crea un dizionario delle scansioni stealth e per ogni scansione stealth crea un istanza della classe Attack 
#Crea un dizionario delgli attacchi stealth e per ogni attacco stealth crea un istanza della classe Attack
#Crea un dizionario degli infect e per ogni infect crea un istanza della classe Attack 
class Attack_DB:

    with open("attack_db.json") as db:
        db_string=json.load(db)
    attack=db_string["storage"]["attacks"]
    scans=db_string["storage"]["scans"]
    stealth_scans=db_string["storage"]["stealth_scans"]
    stealth_attack=db_string["storage"]["stealth_attacks"]
    infect_attack=db_string["storage"]["infect"]

    attack_dict = {}
    for i_k in attack.keys():
        attack_dict[i_k] = Attack(i_k,attack[i_k]["instructions"],int(attack[i_k]["wait_time"]),attack[i_k]["attack_type"])

    
    scans_dict = {}
    for i_k in scans.keys():
        scans_dict[i_k] = Attack(i_k,scans[i_k]["instructions"],int(scans[i_k]["wait_time"]),scans[i_k]["attack_type"])

    stealth_scans_dict = {}
    for i_k in stealth_scans.keys():
        stealth_scans_dict[i_k] = Attack(i_k,stealth_scans[i_k]["instructions"],int(stealth_scans[i_k]["wait_time"]),stealth_scans[i_k]["attack_type"])

    # not used at the moment
    stealth_attack_dict = {}
    for i_k in stealth_attack.keys():
        stealth_attack_dict[i_k] = Attack(i_k,stealth_attack[i_k]["instructions"],int(stealth_attack[i_k]["wait_time"]),stealth_attack[i_k]["attack_type"])

    infect_dict={}
    for i_k in infect_attack.keys():
        infect_dict[i_k]=Attack(i_k,infect_attack[i_k]["instructions"],int(infect_attack[i_k]["wait_time"]))
    
"""

"""

class Attack_DB:

    def __init__(self, metaClient, attacker_ip, OOBsession, db_path="attack_db.json"):

        self.metaClient = metaClient
        self.attacker_ip = attacker_ip
        self.OOBsession = OOBsession

        with open(db_path) as db:
            db_string = json.load(db)
        
        self.attack_dict = self.build_dict(db_string["storage"]["attacks"])
        self.scans_dict = self.build_dict(db_string["storage"]["scans"])
        self.stealth_scans_dict = self.build_dict(db_string["storage"]["stealth_scans"])
        self.stealth_attack_dict = self.build_dict(db_string["storage"]["stealth_attacks"])
        self.infect_dict = self.build_dict(db_string["storage"]["infect"], True)


    def build_dict(self, data, infect=False):
        dict = {}
        for i_k in data.keys():
            if(infect == True):
                dict[i_k] = Attack(i_k,data[i_k]["instructions"],int(data[i_k]["wait_time"]))
            else:
                dict[i_k] = Attack(i_k,data[i_k]["instructions"],int(data[i_k]["wait_time"]),data[i_k]["attack_type"])
            
        return dict

    
    
    

NUOVA VERSIONE CAPIRE SE FORMATO DELLE ISTRUZIONI È GESTIBILE
IDEA: CREARE UN METODO CHE SI ADATTA ALLA CREAZIONE DI QUALSIASI TIPO DI ATTACCO FACENDO L'IF E TOGLIENDOLO DAL MAIN 
ED ESEGUE ANCHE LA FORMATTAZIONE DELLE ISTRUZIONI
NEL MAIN ABBIAMO SOLO I DIZIONARI POI ALL'OCCORRENZA SI CHIAMA QUESTO NUOVO METODO BUILD-ATTACK CHE PRENDE UN ATTACCO E RESTITUISCE L'ATTACCO SPECIFICO

GESTIRE INFECT IN METACLIENT
POI RIMUOVERE COMMENTI ATTACKD AL POSTO DI attackDB NEL MAIN
"""

class Attack_DB:

    def __init__(self, metaClient, attacker_ip, OOBsession, db_path="attack_db.json"):

        self.metaClient = metaClient
        self.attacker_ip = attacker_ip
        self.OOBsession = OOBsession

        with open(db_path) as db:
            db_string = json.load(db)
        
        self.attack_dict = self.build_dict(db_string["storage"]["attacks"])
        self.scans_dict = self.build_dict(db_string["storage"]["scans"])
        self.stealth_scans_dict = self.build_dict(db_string["storage"]["stealth_scans"])
        self.stealth_attack_dict = self.build_dict(db_string["storage"]["stealth_attacks"])
        self.infect_dict = self.build_dict(db_string["storage"]["infect"], True)



    def build_dict(self, data, infect=False):
        dict = {}
        for i_k in data.keys():

            if(infect == True):
                dict[i_k] = Attack(i_k,data[i_k]["instructions"],int(data[i_k]["wait_time"]))
            else:
                dict[i_k] = Attack(i_k,data[i_k]["instructions"],int(data[i_k]["wait_time"]),self.metaClient)
                
        return dict


    def create_scan(self, scan, nmap_target, attacker_ip):
        
        scan_name= self.scans_dict[scan].attack
        scan_instr=self.scans_dict[scan].instruction.format(nmap_target, attacker_ip)
        scan_type=self.scans_dict[scan].attack_type
        scan_wait=self.scans_dict[scan].wait_time

        scan_obj=self.Metasploit_Attack(scan_name,scan_instr,scan_wait,self.metaClient)

        return scan_obj
    

    def create_attack(self, attack, target_ip, attacker_ip, LPORT):
    
        attack_name=self.attack_dict[attack].attack
        attack_instr=self.attack_dict[attack].instruction.format(target_ip,attacker_ip,LPORT=LPORT)
        attack_type=self.attack_dict[attack].attack_type
        print(attack_type)
        attack_wait=self.attack_dict[attack].wait_time

        if(attack_type=="ResourceAttack"):
                attack_obj=ResourceAttack(attack_name, attack_instr, attack_wait, self.metaClient)
        elif(attack_type=="SshAttack"): 
            attack_obj=SshAttack(attack_name, attack_instr, attacker_ip, self.OOBSession, attack_wait, self.metaClient)
        else:
            attack_obj=Metasploit_Attack(attack_name,attack_instr,attack_wait, self.metaClient)
        
        print(type(attack_obj))
        return attack_obj



