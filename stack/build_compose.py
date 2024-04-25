import argparse
import json

import yaml
RED = "\033[0;31m"
GREEN = "\033[0;32m"
YELLOW = "\033[1;33m"
END = "\033[0m"

CONFIG_FILE="./config_all.json"


def remove_nested_keys(dictionary, keys_to_remove):
    #rimuove le chiavi specificate dalla radice del dizionario (file json di configurazione)
    for key in keys_to_remove:
        if key in dictionary:
            del dictionary[key]
    #scorre il dizionario per trovare le chiavi nidificate se presenti e richiama la funzione ricorsivamente
    for value in dictionary.values():
        if isinstance(value, dict):
            remove_nested_keys(value, keys_to_remove)

    return dictionary

def serviceIP(config):
    service_ips={}
    for service in config["containers"].keys():
        service_ips[service]=config["IP_addresses"]["IP_"+config["containers"][service]]
    for service in config["general_services"].keys():
        service_ips[service]=config["IP_addresses"]["IP_"+config["general_services"][service]]
    return service_ips

def build(args):
    
    print(f"{YELLOW}[*] this script will use {args.config_file} as a template config file {END}")
    try:
        f = open(args.config_file)
    except FileNotFoundError as e:
        print(f"{RED}[*] {args.config_file} not found! please check the presence of the file either in the current directory or inside the specified path{END}")
        exit(-1)
    else:
        with f:
            data = json.load(f)
    with open(CONFIG_FILE) as f:
        config=json.load(f)
    print(f"{YELLOW}[*] this script will use {CONFIG_FILE} as a config file {END}")
    #data["services"]["client"]["environment"]["SERVER_IP"]= data["services"]["client"]["environment"]["SERVER_IP"].format(SERVERIP=f"{config['MMSserverIP']}")
    #data["services"]["client"]["command"]= data["services"]["client"]["comand"].format(SERVERIP=f"{config['MMSserverIP']}")
    service_ips=serviceIP(config)

    for k in data["services"].keys():
        data["services"][k]["image"]= data["services"][k]["image"].format(REGISTRY=f"{service_ips['registry_service']}:{config['ports']['registry_port']}")
        if(k in config["containers"]):       
            if k in data["services"]:
                constraint=[]
                constraint.append(data["services"][k]["deploy"]["placement"]["constraints"][0].format(PLACEMENT=config["containers"][k]))
                data["services"][k]["deploy"]["placement"]["constraints"]=constraint
    print(f"{GREEN}[+] config file loaded with success {END}")
    if(args.stack):
        print(f"{YELLOW}[*] this script will create a docker-compose file for stack deployment{END}")
        compose_f=remove_nested_keys(data,["build"])
        with open("stack-compose.yml","w") as newconf:
            yaml.dump(compose_f, newconf, default_flow_style=False)     
        
    else:
        print(f"{YELLOW}[*] this script will create a docker-compose file{END}")
        #rimuoviamo la chiave deploy che non è utilizzabile nei docker-compose
        #rimuoviamo la sezione ports perché dovendo creare in locale tutti i servizi per creare le immagini non vogliamo che dei conflitti di porta 
        #non permettano la loro creazione
        compose_f=remove_nested_keys(data,["deploy","ports"])
        with open("docker-compose.yml","w") as newconf:
            yaml.dump(compose_f, newconf, default_flow_style=False)     
            

    print(f"{GREEN}[+] done{END}") 



if __name__=='__main__':
    parser = argparse.ArgumentParser(description='Used to create docker-compose file either for stack or compose deployment')
    parser.add_argument('-stack', dest='stack', help='if present create a compose file for stack deployment, otherwise create a docker-compose.yml',action='store_true')
    parser.add_argument('--config-file',nargs='?', const="./compose_config.json",dest='config_file',default="./compose_config.json", help='path where the template config file is located, if not defined it will use the default "compose_config.json" file from the same directory')
    args=parser.parse_args()
    build(args)
