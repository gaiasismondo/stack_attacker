import time
import logging
import inspect
import ipaddress
import json

from contextlib import contextmanager

import sys
import os

import signal


# TIMEOUT MANAGEMENT
class TimeoutException(Exception):
    pass


@contextmanager
def time_limit(seconds):
    def signal_handler(signum, frame):
        raise TimeoutException("Timed out!")

    signal.signal(signal.SIGALRM, signal_handler)
    signal.alarm(seconds)
    try:
        yield
    finally:
        signal.alarm(0)


class SuppressErr:
    @staticmethod
    @contextmanager
    def suppress_stderr():
        with open(os.devnull, "w") as devnull:
            old_stderr = sys.stderr
            sys.stderr = devnull
            try:
                yield
            finally:
                sys.stderr = old_stderr
class Constants:
    LOG_FILE_NAME = str(time.time()).split(".")[0] + "_report.log"
    LOG_FILE_FORMAT = '[%(asctime)-15s] - %(module)s_%(class)s: %(message)s'
    LOG_FILE_DIR = 'logs/'


    COL_GREEN = '\033[92m'  # GREEN
    COL_YELLOW = '\033[93m'  # YELLOW
    COL_RED = '\033[91m'  # RED
    COL_RESET = '\033[0m'  # RESET COLOR

    db_file = open("attack_db.json", "r")
    db_string = db_file.read()
    db = json.loads(db_string)
    db_file.close()

    

    config_file=open("config.json","r")
    config=json.loads(config_file.read())
    config_file.close()

    TARGETS_DOCKERS=config["docker"]
    ATTACKER_VM = config["Attacker_VM"]
    DEFAULT_LPORT=config["DEFAULT_LPORT"]
    ATTACKER_CLIENT_PORT=config["ATTACKER_CLIENT_PORT"]
    BACKDOOR_CLIENT_PORT=config["BACKDOOR_CLIENT_PORT"]
    TOMCAT_VM=config["TOMCAT_VM"]
    SMTP_VM=config["SMTP_VM"]
    NETCAT_PORT=config["NETCAT_PORT"]
    ROUTE_PRINT="route print"
    ROUTE_ADD= "route add {} 255.255.255.0 {}"
    METERPRETER_PORT=config["METERPRETER_PORT"]
    METERPRETER_UPGRADE= "back\n set TARGET 0\n use post/multi/manage/shell_to_meterpreter\n  set LHOST {}\n set LPORT {}\n set SESSION {}\n run\n back\n"
    ADD_PORTFWD="portfwd add -R -L {} -l {} -p {}"
    
class Logger:
    # https://docs.python.org/3/library/logging.html#levels
    INFO = 20  # logging.info
    DEBUG = 10  # logging.debug
    WARNING = 30  # logging.warning
    ERROR = 20  # logging.error
    NOTSET = 0

    @staticmethod
    def init_logger():
        logging.basicConfig(filename=Constants.LOG_FILE_DIR + Constants.LOG_FILE_NAME, level=20)
        logging.basicConfig(format=Constants.LOG_FILE_FORMAT)

    @staticmethod
    def log(cls, message, level=logging.DEBUG):
        cls_name = cls.__class__.__name__
        mod_name = inspect.getfile(cls.__class__)
        # extra = {'module': mod_name, 'class': cls_name}
        logging.log(level, message)  # extra=extra)

    @staticmethod
    def turn_off(module):
        logging.getLogger(module).setLevel(Logger.NOTSET)

    


