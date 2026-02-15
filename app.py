from flask import Flask, request, jsonify, send_file
import json
import os
from datetime import datetime

app = Flask(__name__)

DATA_FILE = 'bpm_data.json'

# Inicjalizacja pliku danych
if not os.path.exists(DATA_FILE):
    with open(DATA_FILE, 'w') as f:
        json.dump([], f)

@app.route('/api/bpm', methods=['POST'])
def receive_bpm():
    """Odbiera dane BPM z ESP8266"""
    data = request.get_json()
    if not data or 'bpm' not in data:
        return 'Brak danych BPM', 400

    record = {
        'timestamp': datetime.now().isoformat(),
        'bpm': data['bpm']
    }

    with open(DATA_FILE, 'r') as f:
        records = json.load(f)

    records.append(record)

    with open(DATA_FILE, 'w') as f:
        json.dump(records, f, indent=2)

    return 'OK', 200

@app.route('/api/data', methods=['GET'])
def get_data():
    """Zwraca wszystkie zapisane dane"""
    with open(DATA_FILE, 'r') as f:
        records = json.load(f)
    return jsonify(records)

@app.route('/')
def index():
    """Serwuje stronę główną"""
    return send_file('index.html')

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
