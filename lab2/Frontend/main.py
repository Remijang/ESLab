import io
import time

import matplotlib.pyplot as plt
import numpy as np
import yaml
import json
from flask import Flask, Response, jsonify, render_template, request

with open("config.yaml", "r") as f:
    config = yaml.safe_load(f)

app = Flask(__name__, template_folder="template/")
received_data = np.zeros((3, 1))
latest_data = np.zeros((3, 1))
latest_event = None


@app.route("/", methods=["POST"])
def receive_data():
    global latest_data, received_data
    try:
        payload = request.get_json()
        if payload is None:
            return jsonify({"error": "Invalid JSON"}), 400

        latest_data += np.array([payload["X"], payload["Y"], payload["Z"]]).reshape(
            (3, 1)
        )

        received_data = np.concatenate((received_data, latest_data), axis=-1)
        return jsonify({"status": "ok"}), 200

    except Exception as e:
        print(str(e))
        return jsonify({"error": str(e)}), 500


@app.route("/latest", methods=["GET"])
def get_latest():
    """Optional: return the latest data for a frontend"""
    return jsonify(latest_data)


@app.route("/plot.png")
def plot_png():
    x, y, z = received_data[0], received_data[1], received_data[2]
    fig = plt.figure()

    ax = fig.add_subplot(111, projection="3d")

    if x.shape[0] == 1:
        output = io.BytesIO()
        fig.savefig(output, format="png")
        plt.close(fig)
        output.seek(0)
        return Response(output.getvalue(), mimetype="image/png")

    ax.set_xlim([x.min(), x.max()])
    ax.set_ylim([y.min(), y.max()])
    ax.set_zlim([z.min(), z.max()])
    ax.scatter(x, y, z, c="r", marker="o")
    n = x.shape[0]
    for i in range(n - 1):
        ax.quiver(
            x[i],
            y[i],
            z[i],
            x[i + 1] - x[i],
            y[i + 1] - y[i],
            z[i + 1] - z[i],
            arrow_length_ratio=0.01,
            color="black",
        )
    output = io.BytesIO()
    fig.savefig(output, format="png")
    plt.close(fig)
    output.seek(0)
    return Response(output.getvalue(), mimetype="image/png")


@app.route("/", methods=["GET"])
def render():
    return render_template("index.html")


@app.route("/events")
def events():
    def stream():
        last_sent = None
        while True:
            global latest_event
            if latest_event != last_sent and latest_event is not None:
                yield f"data: {json.dumps(latest_event)}\n\n"
                last_sent = latest_event
            time.sleep(1)

    return Response(stream(), mimetype="text/event-stream")


@app.route("/motion", methods=["POST"])
def motion():
    global latest_event
    latest_event = {
        "time": time.strftime("%Y-%m-%d %H:%M:%S"),
        "message": "Significant motion detected!",
    }
    print(f"[IOT] {latest_event}")
    return jsonify({"status": "ok"})


if __name__ == "__main__":
    app.run(host=config["server"]["host"], port=config["server"]["port"], debug=True)
