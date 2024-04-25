import argparse
import os
import json

def parse_args():

    parser = argparse.ArgumentParser(
                    prog='setupAttacks',
                    description='It sets up container IPs in attacks')

    parser.add_argument ('pwd', action='store', help='the working directory', type = str)

    return parser.parse_args()

def serviceIP(config):
    service_ips={}
    for service in config["containers"].keys():
        service_ips[service]=config["IP_addresses"]["IP_"+config["containers"][service]]
    for service in config["other_targets"].keys():
        service_ips[service]=config["IP_addresses"]["IP_"+config["other_targets"][service]]
    return service_ips


#Main function
pwd=parse_args().pwd
home =pwd.split("Netsploit")[0]
path_escape=home+"stack/data/attacker/custom_attacks/docker_escape/"
path_tar=home+"stack/lab/ExploitsCVEs/CVEs/"
path_breakout=path_tar+"CVE201914271/exploit/"
infilenames = ["templates/CVE-2019-14271_PostExploitScript.sh","templates/config_rc.json","templates/breakout","templates/config.json","templates/MMSproxyAttack.sh"]
outfilenames = [path_escape+"CVE-2019-14271_PostExploitScript.sh",path_escape+"config_rc.json",path_breakout+"breakout",home+"Netsploit/config.json",home+"Netsploit/MMSproxy/MMSproxyAttack.sh"]

CONFIG_FILE="../stack/config_all.json"

with open(CONFIG_FILE) as f: 
    config=json.load(f)
service_ips=serviceIP(config)

containers=False
            
for (infile,outfile) in zip(infilenames, outfilenames):
    with open(infile) as template:
        out=[]
        for line in template:
            for service in service_ips.keys():
                if line.find("IP"+service)>0:
                    line=line.replace("IP"+service,service_ips[service])
            out.append(line)
    with open(outfile,'w') as dest:
        for line in out:
            dest.write(line)

os.system("tar cf "+path_tar+"CVE201914271.tar -C "+path_tar+" CVE201914271")
