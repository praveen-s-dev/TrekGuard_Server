from flask import Flask, render_template, request, jsonify
from flask_cors import CORS
from datetime import datetime
from collections import deque # Optimized for performance

app = Flask(__name__)
CORS(app)

# Performance: Disable JSON key sorting to speed up response time
app.config['JSON_SORT_KEYS'] = False 

# --- GLOBAL DATA STORAGE ---
data_store = {
    "trekker_gps": {"lat": 12.9716, "lon": 77.5946, "last_sync": "Never"},
    "sensors": {"temp": 0, "pres": 0, "alt": 0, "gas": 0, "status": "STABLE"},
    "history": deque(maxlen=60),  # O(1) performance for adding/removing items
    "alerts": deque(maxlen=20),   # Automatically discards old alerts
    "hq_command": "",   
    "buzzer_active": False
}

@app.route('/admin')
def admin_dashboard():
    return render_template('admin.html')

@app.route('/user')
def user_app():
    return render_template('user.html')

@app.route('/api/update_gps', methods=['POST'])
def update_gps():
    data = request.json
    if data:
        data_store['trekker_gps'].update({
            "lat": data.get('lat'),
            "lon": data.get('lon'),
            "last_sync": datetime.now().strftime("%I:%M:%S %p")
        })
        return jsonify({"status": "GPS_SYNCED"}), 200
    return jsonify({"status": "ERROR"}), 400

@app.route('/api/sos', methods=['POST'])
def receive_sos():
    data = request.json
    if data:
        now_time = datetime.now().strftime("%I:%M:%S %p")
        data['time'] = now_time
        
        # Update master GPS coordinates
        if 'lat' in data:
            data_store['trekker_gps'].update({"lat": data['lat'], "lon": data['lon']})

        # Update sensors and alert list
        data_store['sensors'].update({"status": data.get('type', "EMERGENCY")})
        data_store['alerts'].append(data)
        return jsonify({"status": "SOS_LOGGED"}), 200
    return jsonify({"status": "INVALID_DATA"}), 400

@app.route('/api/sensors', methods=['POST'])
def update_sensors():
    data = request.get_json(silent=True) # Faster JSON parsing
    if not data:
        return jsonify({"status": "no_data"}), 400

    now_time = datetime.now().strftime("%I:%M:%S %p")
    
    # Direct update for speed
    data_store['sensors'] = {
        "temp": data.get('temp', 0),
        "pres": data.get('pres', 0),
        "alt": data.get('alt', 0),
        "gas": data.get('gas', 0),
        "status": data.get('status', 'STABLE')
    }

    # Automatically managed by deque maxlen
    data_store['history'].append({"t": now_time, "temp": data.get('temp'), "alt": data.get('alt')})
    
    # Auto-Trigger Alert
    if data.get('status') != "STABLE":
        if not data_store['alerts'] or data_store['alerts'][-1]['type'] != data.get('status'):
            data_store['alerts'].append({
                "type": data.get('status'),
                "time": now_time,
                "lat": data_store['trekker_gps']['lat'],
                "lon": data_store['trekker_gps']['lon']
            })

    return jsonify({"status": "updated"}), 200

@app.route('/api/data', methods=['GET'])
def get_master_data():
    # Convert deques back to lists for JSON serialization
    response_data = data_store.copy()
    response_data['history'] = list(data_store['history'])
    response_data['alerts'] = list(data_store['alerts'])
    return jsonify(response_data)

@app.route('/api/send_command', methods=['POST'])
def send_command():
    data = request.json
    data_store['hq_command'] = data.get('msg', "")
    data_store['buzzer_active'] = data.get('buzz', False)
    return jsonify({"status": "COMMAND_QUEUED"}), 200

if __name__ == '__main__':
    # Use threaded=True to handle multiple connections (ESP32 + Admin + User) simultaneously
    app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)