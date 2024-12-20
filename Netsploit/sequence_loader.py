import json

#legge la sequenza di attacchi dal file preso in input
def load_attack_sequence(file_path):
    with open(file_path, 'r') as f:
        data = json.load(f)
    return [(attack["attack_name"], attack["IP"], attack["additional_attribute"]) for attack in data["attack_sequence"]]



#stampa in modo ordinato il contenuto del file attack sequence
def print_attack_sequence(file_path):
    try:
        attack_sequence = load_attack_sequence(file_path)
        for i, (attack_name, ip, additional_attribute) in enumerate(attack_sequence, start=1):
            print(f"STEP {i}:\n\tattack_name: {attack_name}\n\tIP: {ip}\n\tadditional_attribute: {additional_attribute}")
        
    except FileNotFoundError:
        print(f"Error: file '{file_path}' not found.")



