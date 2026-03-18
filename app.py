from flask import Flask, render_template, request, jsonify
from flask_cors import CORS
import datetime

app = Flask(__name__)
CORS(app)

alerts_db = []

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/sos', methods=['POST'])
def receive_sos():
    data = request.json
    if data:
        data['time'] = datetime.datetime.now().strftime("%H:%M:%S")
        alerts_db.append(data)
        return jsonify({"status": "Success"}), 200
    return jsonify({"status": "No Data"}), 400

@app.route('/api/get_alerts', methods=['GET'])
def get_alerts():
    return jsonify(alerts_db[-10:])

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)