import os
import docker
import json
from pathlib import Path
import sys 

CONFIG_FILE = "./config.json"

f=open(CONFIG_FILE)
with f:
    config = json.load(f)

registry_port=config['ports']['registry_port']
registry_IP=config['IP_addresses'][config['general_services']['registry_service']]
REG=registry_IP+":"+registry_port

tag_list = ['/lab/tomcat', 
'/lab/sslproxy',
'/lab/mail_server',
'/lab/metasploit_new',
'/lab/exploitcves',
'/lab/client',
'/lab/server',
'/lab/tomcat_graph',
'/analisi',
'/kafkacat',
'/opensearchproject/opensearch-dashboards',
'/opensearchproject/opensearch',
'/docker.elastic.co/logstash/logstash-oss',
'/confluentinc/cp-kafka',
'/confluentinc/cp-zookeeper'
]

def load_docker_images():
    client = docker.from_env()
    path="images/"
    directory=Path(path)
    if(not directory.is_dir()):
        print(f'{path} not present, skipping images loading')
        sys.exit()
    for filename in os.listdir(path):
        if filename.endswith('.tar.gz'):
            with open(path+filename, 'rb') as f:
                print(f"loading {filename}")
                client.images.load(f.read())

def modify_docker_image_tags():
    client = docker.from_env()
    images = client.images.list()
    for image in images:
        # Estrai il nome dell'immagine
        if not image.tags:
            continue
        for tag in image.tags:
            if (tag != "registry:2"):
                image_portpath=tag.rsplit(":")[1]
                image_tag=tag.rsplit(":")[2]
                image_path='/'+image_portpath.split("/",1)[1]
                # Se il nome dell'immagine è presente nel tag mapping, modificalo
                if image_path in tag_list:
                    new_tag = REG+image_path+":"+image_tag
                    print(f"modifying tag for image {tag} to {new_tag}")
                    image.tag(new_tag)
                    if(tag != new_tag):
                        os.system("sudo docker image rmi "+tag)

load_docker_images()
modify_docker_image_tags()
