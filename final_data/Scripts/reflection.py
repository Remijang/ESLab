import numpy as np
import re


a = 1.0011583795355363
b = 0.995068796668601
d = -1.6759465583366173
e = -1.537611331891522

DATA_FILE = "Datas/data.txt"
LABEL_FILE = "Datas/label.txt"

SAVE_DATA_FILE = "Datas/data_reflection.txt"
SAVE_LABEL_FILE = "Datas/label_reflection.txt"


def load_parameters(filepath):
    params = {}
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            matches = re.findall(r"([a-zA-Z]+)\s*=\s*([0-9\.\-]+)", f.read())
            for key, val in matches:
                params[key] = float(val)
    except FileNotFoundError:
        print(f"警告: 找不到 {filepath}")
    return params


def load_data(filepath):
    dataset = []
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            raw_segments = f.read().strip().split("data")
            for segment in raw_segments:
                if not segment.strip():
                    continue
                lines = segment.strip().split("\n")
                data_buffer = []
                for line in lines:
                    parts = line.strip().split(",")
                    if len(parts) >= 2:
                        try:
                            data_buffer.append([float(parts[0]), float(parts[1])])
                        except ValueError:
                            continue
                if data_buffer:
                    dataset.append(np.array(data_buffer))
    except FileNotFoundError:
        print(f"警告: 找不到 {filepath}")
    return dataset


def load_labels(filepath):
    labels = []
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            for line in f.readlines():
                if line.strip():
                    parts = line.strip().split(",")
                    if len(parts) >= 2:
                        labels.append((float(parts[0]), float(parts[1])))
    except FileNotFoundError:
        print(f"警告: 找不到 {filepath}")
    return labels


def main():
    dataset_raw = load_data(DATA_FILE)
    labels_vec = load_labels(LABEL_FILE)

    if not dataset_raw:
        print("Error: No data loaded.")
        return

    dataset_out = ""
    labels_out = ""

    def reverse(raw_x, raw_y, sign_x, sign_y, bias_x, bias_y):
        cal_x = (raw_x - d - bias_x) / a
        cal_y = (raw_y - e - bias_y) / b
        cal_x *= sign_x
        cal_y *= sign_y
        ret_x = cal_x * a + d + bias_x
        ret_y = cal_y * b + e + bias_y
        return int(ret_x), int(ret_y)

    def reflection(idx, flip_x: bool, flip_y: bool, data_out: str, label_out: str):
        sign_x = -1 if flip_x is True else 1
        sign_y = -1 if flip_y is True else 1
        data_out += "data\n"
        bias_x = sum([dataset_raw[idx][i][0] for i in range(10)]) / 10
        bias_y = sum([dataset_raw[idx][i][1] for i in range(10)]) / 10
        for data in dataset_raw[idx]:
            x, y = reverse(data[0], data[1], sign_x, sign_y, bias_x, bias_y)
            data_out += f"{x}, {y}\n"
        label_x = labels_vec[idx][0] * sign_y
        label_y = labels_vec[idx][1] * sign_x
        label_out += f"{label_x}, {label_y}\n"
        return data_out, label_out

    for i in range(len(dataset_raw)):
        dataset_out, labels_out = reflection(i, False, False, dataset_out, labels_out)
        dataset_out, labels_out = reflection(i, False, True, dataset_out, labels_out)
        dataset_out, labels_out = reflection(i, True, False, dataset_out, labels_out)
        dataset_out, labels_out = reflection(i, True, True, dataset_out, labels_out)

    with open(SAVE_DATA_FILE, "w", encoding="utf-8") as f:
        f.write(dataset_out)
        f.close()

    with open(SAVE_LABEL_FILE, "w", encoding="utf-8") as f:
        f.write(labels_out)
        f.close()


if __name__ == "__main__":
    main()
