import json

#legge la sequenza di attacchi dal file preso in input
def load_attack_sequence(file_path):
    with open(file_path, 'r') as f:
        data = json.load(f)
    return [(attack["attack_name"], attack["ip"], attack["other_attribute"]) for attack in data["attack_sequence"]]



#stampa in modo ordinato il contenuto del file attack sequence
def print_attack_sequence(file_path):
    try:
        attack_sequence = load_attack_sequence(file_path)
        for i, (attack_name, ip, other_attribute) in enumerate(attack_sequence, start=1):
            print(f"STEP {i}:   attack_name: {attack_name}  IP: {ip}  other_attribute: {other_attribute if other_attribute else 'N/A'}\n")
        
    except FileNotFoundError:
        print(f"Errore: il file '{file_path}' non è stato trovato.")



