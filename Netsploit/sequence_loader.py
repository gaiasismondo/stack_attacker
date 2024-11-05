import json

#legge la sequenza di attacchi dal file preso in input
def load_attack_sequence(file_path):
    try:
        with open(file_path, 'r') as f:
            data = json.load(f)
            attack_sequence = data.get("attack_sequence", [])
            return attack_sequence
    except (FileNotFoundError, json.JSONDecodeError) as e:
        print(f"Errore nel caricamento del file di sequenza di attacco: {e}")
        return []


#stampa in modo ordinato il contenuto del file attack sequence
def print_attack_sequence(file_path):
    try:
        attack_sequence = load_attack_sequence(file_path)
        
        print("\nATTACK SEQUENCE: ")
        for i, (attack_name, ip, generic_attribute) in enumerate(attack_sequence, start=1):
            print(f"Step {i}:")
            print(f"  Attack name: {attack_name}")
            print(f"  IP: {ip}")
            print(f" Other_attribute: {generic_attribute if generic_attribute else 'N/A'}")
          
    except FileNotFoundError:
        print(f"Errore: il file '{file_path}' non è stato trovato.")



