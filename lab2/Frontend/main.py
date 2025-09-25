from flask import Flask, render_template, request, jsonify
import yaml
import logging


with open("config.yaml", "r") as f:
    config = yaml.safe_load(f)

app = Flask(__name__, template_folder="template/")
latest_data = None


@app.route("/", methods=["POST"])
def receive_data():
    global latest_data
    try:
        payload = request.get_json()
        if payload is None:
            return jsonify({"error": "Invalid JSON"}), 400

        latest_data = payload
        print("Received data:", latest_data)
        return jsonify({"status": "ok"}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/latest", methods=["GET"])
def get_latest():
    """Optional: return the latest data for a frontend"""
    return jsonify(latest_data)


if __name__ == "__main__":
    app.run(host=config["server"]["host"], port=config["server"]["port"], debug=True)
