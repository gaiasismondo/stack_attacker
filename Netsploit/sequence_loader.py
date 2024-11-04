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
