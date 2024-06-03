from abc import ABC, abstractmethod
import re
from time import sleep
from contextlib import contextmanager
import signal
import json
from util import time_limit
from util import Logger


# Classe astratta che rappresenta un attacco
class Attack(ABC):
    def __init__(self, name, instructions, wait_time, attack_type=None):
        self.attack = name
        self.instruction = instructions
        self.wait_time = wait_time
        self.attack_type = attack_type

    # Metodo astratto per eseguire l'attacco
    def execute(self):
        raise NotImplementedError("Use specific attacks!")

    # Metodo astratto per verificare l'esito dell'attacco
    def check(self):
        raise NotImplementedError("Use specific attacks!")


# Classe che estende Attack e implementa attacchi Metasploit e ResourceAttack (is_resource=True)
class MetasploitAttack(Attack):
    LONG_SLEEP_TIME = 15
    SHORT_SLEEP_TIME = 5

    def __init__(self, name, instructions, wait_time=10, client=None, is_resource=False):
        super().__init__(name, instructions, wait_time=wait_time)
        self.client = client
        self.output = []
        self.is_resource = is_resource

    # Esegue l'attacco
    def execute(self):
        old_sess = self.client.get_active_sessions()
        instr_list = self.instruction.split("\n")

        if self.is_resource:
            self.execute_resource(instr_list)
        else:
            settings = self.getSettings(instr_list)
            if "resource" in instr_list:
                return
            payload = self._prepare_payload(settings)
            exploit = self._prepare_exploit(settings)
            self.output = self.client.client.consoles.console(self.client.cid).run_module_with_output(exploit, payload=payload)
        
        return self.check(old_sess)

    # Esegue l'attacco usando un resource script
    def execute_resource(self, instr_list):
        _ = self.client.client.consoles.console(self.client.cid).read()
        for i in instr_list:
            self.client.client.consoles.console(self.client.cid).write(i)
            sleep_time = self.LONG_SLEEP_TIME if ("resource" in i or "exploit" in i or "run" in i) else self.SHORT_SLEEP_TIME
            sleep(sleep_time)
            out = self.client.client.consoles.console(self.client.cid).read()
            self.output.append(out["data"])

        with time_limit(300):
            while self.client.client.consoles.console(self.client.cid).is_busy():
                sleep(1)
        out = self.client.client.consoles.console(self.client.cid).read()
        self.output.append(out["data"])

    
    def _parse_settings(self, instr_list, keyword):
        settings = {}
        found = False
        for i in instr_list:
            val = i.partition("setg")[2].strip().partition(" ")
            if val[0]:
                settings[val[0]] = val[2]
            if not found:
                exploit_req = [m.start() for m in re.finditer(keyword, i)]
                if exploit_req:
                    found = True
                    settings[keyword] = i.partition("use")[2].strip().partition(" ")[0][len(keyword) + 1:]
        return settings


    def getSettings(self, instr_list):
        return self._parse_settings(instr_list, "exploit")


    def getSettingScan(self, instr_list):
        return self._parse_settings(instr_list, "auxiliary")


    def _prepare_payload(self, settings):
        if "payload" in settings:
            payload = self.client.client.modules.use('payload', settings["payload"])
            Logger.log(self, f"using payload - {settings['payload']}", level=Logger.INFO)
            if "LPORT" in settings:
                payload.runoptions["LPORT"] = settings["LPORT"]
            return payload
        return None

    def _prepare_exploit(self, settings):
        exploit = {}
        if "exploit" in settings:
            exploit = self.client.client.modules.use("exploit", settings["exploit"])
            Logger.log(self, f"using exploit - {settings['exploit']}", level=Logger.INFO)

        for key in settings.keys():
            if key in ["payload", "exploit", "LPORT"]:
                continue
            exploit[key] = settings[key]
        return exploit

    def scan(self):
        instr_list = self.instruction.split("\n")
        settings = self.getSettingScan(instr_list)
        payload = self._prepare_payload(settings)
        exploit = self._prepare_auxiliary(settings)
        # Execute the scan without reading the output to avoid blocking
        exploit.execute()
        sleep(2)

    

    def _prepare_auxiliary(self, settings):
        if "auxiliary" in settings:
            auxiliary = self.client.client.modules.use("auxiliary", settings["auxiliary"])
            Logger.log(self, f"using scan - {settings['auxiliary']}", level=Logger.INFO)
            for key in settings.keys():
                if key in ["payload", "auxiliary", "LPORT"]:
                    continue
                auxiliary[key] = settings[key]
            return auxiliary
        return None

    def check(self, old_sess):
        session = {}
        new_sess = self.client.get_active_sessions()
        diff = set(new_sess) - set(old_sess)

        if diff:
            session["id_sess"] = diff.pop()
            Logger.log(self, f"session created - {session['id_sess']}", level=Logger.INFO)
            obtained_session = new_sess[session["id_sess"]]
            session["obtained_session"] = obtained_session
            session["session_host"] = obtained_session["session_host"]
            return session
        else:
            Logger.log(self, f"unable to create session", level=Logger.INFO)
            return session
        



class SshAttack(Attack):
    SLEEP_TIME = 5

    def __init__(self, name, instructions, ip, session, time_waitwait=None, client=None):
        time_wait = (len(instructions) * SshAttack.SLEEP_TIME) + 10
        super().__init__(name, instructions, wait_time=time_wait)
        self.client = client
        self.instructions = instructions
        self.session = session
        self.ip = ip

        if type(self.session) == str:
            raise TypeError

    def execute(self):
        for c in self.instructions:
            self.client.client.sessions.session.write(c)
            sleep(SshAttack.SLEEP_TIME)
            y = self.client.client.sessions.session.read()
            self.out.append(y)
        return self.check(self.client.get_active_sessions())

    def check(self, old_sess):
        session = {}
        new_sess = self.client.get_active_sessions()
        diff = set(new_sess) - set(old_sess)
        if diff:
            session["id_sess"] = diff.pop()
            Logger.log(self, f"session created - {session['id_sess']}", level=Logger.INFO)
            obtained_session = new_sess[session["id_sess"]]
            session["obtained_session"] = obtained_session
            session["session_host"] = obtained_session["session_host"]
            return session
        else:
            Logger.log(self, f"unable to create session", level=Logger.INFO)
            return session





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


    def create_scan(self, scan, nmap_target, attacker_ip):
        
        scan_name= self.scans_dict[scan].attack
        scan_instr=self.scans_dict[scan].instruction.format(nmap_target, attacker_ip)
        scan_type=self.scans_dict[scan].attack_type
        scan_wait=self.scans_dict[scan].wait_time

        scan_obj=MetasploitAttack(scan_name,scan_instr,scan_wait,self.metaClient)

        return scan_obj
    

    def create_attack(self, attack, target_ip, attacker_ip, LPORT):
    
        attack_name=self.attack_dict[attack].attack
        attack_instr=self.attack_dict[attack].instruction.format(target_ip,attacker_ip,LPORT=LPORT)
        attack_type=self.attack_dict[attack].attack_type
        attack_wait=self.attack_dict[attack].wait_time

        if(attack_type=="ResourceAttack"):
                attack_obj=MetasploitAttack(attack_name, attack_instr, attack_wait, self.metaClient, is_resource=True)
        elif(attack_type=="SshAttack"): 
            attack_obj=SshAttack(attack_name, attack_instr, attacker_ip, self.OOBsession, attack_wait, self.metaClient)
        else:
            attack_obj=MetasploitAttack(attack_name,attack_instr,attack_wait, self.metaClient)

        return attack_obj



