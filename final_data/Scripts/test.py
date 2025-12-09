import torch
import torch.nn as nn
import numpy as np
import re
import matplotlib.pyplot as plt
import matplotlib
from collections import OrderedDict, deque
import itertools
from tqdm import tqdm
import argparse

# 全域常數 (對應您的 train.py)
A = 1.0011583795355363
B = 0.995068796668601
D = -1.6759465583366173
E = -1.537611331891522
INTERVAL = 0.01

PNG_SAVE_FILE = "Datas/test.png"
TRACE_FILE = "Datas/trace.txt"
RNN_CHECKPOINT = "checkpoint"
PARAMETER_FILE = "Datas/parameters.txt"
DATA_FILE = "Datas/data_reflection.txt"
LABEL_FILE = "Datas/label_reflection.txt"


# ==========================================
# 1. 基礎資料讀取
# ==========================================


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


# ==========================================
# 2. ZUPT 相關 (Grid Search & Solver)
# ==========================================


def get_calibrated_acc_metric(raw_val, params):
    """ZUPT 專用校正 (m/s^2)"""
    raw_val = np.array(raw_val)
    G_val = params.get("G", 1000)

    if raw_val.ndim == 1:
        raw_x, raw_y = raw_val[0], raw_val[1]
    else:
        raw_x, raw_y = raw_val[:, 0], raw_val[:, 1]

    cal_x = (raw_x - params.get("d", 0)) / params.get("a", 1)
    cal_y = (raw_y - params.get("e", 0)) / params.get("b", 1)

    acc_x_metric = (cal_x / G_val) * 9.81
    acc_y_metric = (cal_y / G_val) * 9.81

    if raw_val.ndim == 1:
        return np.array([acc_y_metric, -acc_x_metric])
    else:
        return np.stack([acc_y_metric, -acc_x_metric], axis=1)


def run_gpu_grid_search(acc_list, label_list, param_grid, device):
    num_samples = len(acc_list)
    if num_samples == 0:
        return {
            "window": 10,
            "threshold": 0.4,
            "threshold2": 0.0,
            "frames": 8,
            "alpha": 0.1,
        }, 0.0

    max_len = max([len(a) for a in acc_list])

    acc_tensor = torch.zeros(
        (num_samples, max_len, 2), device=device, dtype=torch.float32
    )
    label_tensor = torch.tensor(label_list, device=device, dtype=torch.float32)

    for i, acc in enumerate(acc_list):
        L = len(acc)
        acc_tensor[i, :L, :] = torch.tensor(acc, device=device, dtype=torch.float32)

    windows = param_grid["window"]
    thresholds = param_grid["threshold"]
    thresholds2 = param_grid["threshold2"]
    frames_list = param_grid["frames"]
    alphas = param_grid["alpha"]

    combos = list(itertools.product(thresholds, frames_list, alphas))
    combos_tensor = torch.tensor(combos, device=device, dtype=torch.float32)
    num_combos = len(combos)

    batch_size = num_samples * num_combos

    batch_acc = (
        acc_tensor.unsqueeze(1).repeat(1, num_combos, 1, 1).view(batch_size, max_len, 2)
    )

    batch_thresh = combos_tensor[:, 0].repeat(num_samples).view(batch_size, 1)
    batch_frame_limit = combos_tensor[:, 1].repeat(num_samples).view(batch_size, 1)
    batch_alpha = combos_tensor[:, 2].repeat(num_samples).view(batch_size, 1)

    batch_labels = (
        label_tensor.unsqueeze(1).repeat(1, num_combos, 1).view(batch_size, 2)
    )

    print(
        f"ZUPT Grid Search: Batch Size={batch_size}, Testing {len(windows)*len(thresholds2)*num_combos} combinations..."
    )

    best_loss = float("inf")
    best_config = None
    dt = 0.01

    total_iterations = len(windows) * len(thresholds2)
    with tqdm(total=total_iterations, desc="Grid Search", unit="cfg") as pbar:
        for w_size in windows:
            for threshold2 in thresholds2:
                batch_thresh2 = torch.tensor(
                    threshold2, device=device, dtype=torch.float32
                ).repeat(batch_size, 1)

                vel = torch.zeros((batch_size, 2), device=device)
                pos = torch.zeros((batch_size, 2), device=device)
                bias = torch.zeros((batch_size, 2), device=device)
                static_counter = torch.zeros((batch_size, 2), device=device)

                for t in range(max_len):
                    curr_acc = batch_acc[:, t, :]

                    start_idx = max(0, t - w_size + 1)
                    window_slice = batch_acc[:, start_idx : t + 1, :]

                    if window_slice.shape[1] > 1:
                        std_val = torch.std(window_slice, dim=1)
                        mean_val = torch.mean(window_slice, dim=1)
                    else:
                        std_val = torch.zeros((batch_size, 2), device=device)
                        mean_val = torch.zeros((batch_size, 2), device=device)

                    is_stable = (std_val < batch_thresh) & (
                        torch.abs(mean_val) < batch_thresh2 if threshold2 > 0 else True
                    )

                    static_counter = torch.where(
                        is_stable, static_counter + 1, torch.zeros_like(static_counter)
                    )

                    is_static = static_counter >= batch_frame_limit

                    new_bias = (1 - batch_alpha) * bias + batch_alpha * curr_acc
                    bias = torch.where(is_static, new_bias, bias)

                    acc_real = curr_acc - bias
                    new_vel = vel + acc_real * dt

                    vel = torch.where(is_static, torch.zeros_like(vel), new_vel)
                    pos = pos + vel * dt

                diff = (pos * 100) - batch_labels
                dist = torch.sqrt(torch.sum(diff**2, dim=1))

                dist_matrix = dist.view(num_samples, num_combos)

                total_loss_per_combo = torch.sum(dist_matrix, dim=0)

                min_w_loss, min_idx = torch.min(total_loss_per_combo, dim=0)

                if min_w_loss.item() < best_loss:
                    best_loss = min_w_loss.item()
                    best_c = combos[min_idx.item()]
                    best_config = {
                        "window": w_size,
                        "threshold": best_c[0].item(),
                        "threshold2": threshold2.item(),
                        "frames": int(best_c[1]),
                        "alpha": best_c[2],
                    }

                pbar.set_postfix(best_loss=f"{best_loss:.4f}")
                pbar.update(1)

    return best_config, best_loss


class FinalZUPT:
    def __init__(self, params):
        self.w = params["window"]
        self.th = params["threshold"]
        self.th2 = params["threshold2"]
        self.fr = params["frames"]
        self.alpha = params["alpha"]
        self.vx, self.vy = 0.0, 0.0
        self.px, self.py = 0.0, 0.0
        self.bx, self.by = 0.0, 0.0
        self.buff_x = deque(maxlen=self.w)
        self.buff_y = deque(maxlen=self.w)
        self.cx, self.cy = 0, 0

    def update(self, ax, ay, dt=0.01):
        self.buff_x.append(ax)
        self.buff_y.append(ay)
        sx = False
        if len(self.buff_x) == self.w:
            if np.std(self.buff_x) < self.th and np.mean(self.buff_x) < self.th2:
                self.cx += 1
            else:
                self.cx = 0
        else:
            self.cx += 1
        if self.cx >= self.fr:
            sx = True
            self.vx = 0.0
            self.bx = (1 - self.alpha) * self.bx + self.alpha * ax
        else:
            self.vx += (ax - self.bx) * dt
            self.px += self.vx * dt

        sy = False
        if len(self.buff_y) == self.w:
            if np.std(self.buff_y) < self.th and np.mean(self.buff_y) < self.th2:
                self.cy += 1
            else:
                self.cy = 0
        else:
            self.cy += 1
        if self.cy >= self.fr:
            sy = True
            self.vy = 0.0
            self.by = (1 - self.alpha) * self.by + self.alpha * ay
        else:
            self.vy += (ay - self.by) * dt
            self.py += self.vy * dt
        return [self.px * 100, self.py * 100], [self.vx * 100, self.vy * 100], [sx, sy]


# ==========================================
# 3. RNN 模型 (依據您提供的 code)
# ==========================================


class SimpleRNN(nn.Module):
    def __init__(self, input_size, hidden_size, output_size, num_layers=1):
        super(SimpleRNN, self).__init__()

        self.hidden_size = hidden_size
        self.num_layers = num_layers

        # 強制使用 batch_first=True 以方便處理 shape
        # 雖然您的訓練 code 沒寫，但 inputs 通常是 (1, Seq, 2)
        self.rnn = nn.GRU(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
            batch_first=True,
        )

        self.output = nn.Linear(hidden_size, output_size)
        # Alpha 是一個可訓練參數
        self.alpha = nn.Parameter(torch.tensor(1.0))

    def forward(self, x):
        # x shape: (batch, seq_len, input_size)
        # h0 initialized to 0 inside RNN if not provided, matching C code
        out, hn = self.rnn(x)
        # out: (batch, seq, hidden)

        out = self.output(out)  # (batch, seq, output_size) -> Velocity
        return out


# ==========================================
# 3. 主程式
# ==========================================


def main_merged(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Using device: {device}")

    # 1. 載入資料
    params_file = load_parameters(PARAMETER_FILE)
    dataset_raw = load_data(DATA_FILE)
    labels_vec = load_labels(LABEL_FILE)

    if not dataset_raw:
        print("Error: No data loaded.")
        return

    # 2. ZUPT Grid Search
    print("--- ZUPT Processing ---")
    acc_metric_list = [get_calibrated_acc_metric(d, params_file) for d in dataset_raw]
    param_grid = {
        "window": [i for i in range(10, 11)],  # best 10
        "threshold": np.arange(0.2, 0.21, 0.01),  # best 0.2
        "threshold2": np.arange(0.6, 0.7, 0.1),  # best 0.6
        "frames": [i for i in range(3, 4)],  # best 3
        "alpha": [0.01 * i for i in range(7, 8)],  # best 0.07
    }
    if args.grid_search:
        best_params, min_err = run_gpu_grid_search(
            acc_metric_list, labels_vec, param_grid, device
        )
        print(f"Best ZUPT Params: {best_params} (Loss: {min_err:.2f})")
    else:
        best_params = {
            "window": 12,
            "threshold": 0.21,
            "threshold2": 3,
            "frames": 3,
            "alpha": 0.07,
        }
        print(f"Use ZUPT Params: {best_params}")

    # 3. RNN 模型載入
    print("--- RNN Processing ---")
    # 定義模型 (2層, Hidden 20)
    rnn_model = SimpleRNN(input_size=2, hidden_size=20, output_size=2, num_layers=3).to(
        device
    )

    rnn_model.load_state_dict(torch.load(RNN_CHECKPOINT))

    # 4. 產生軌跡
    plot_data = []
    all_x, all_y = [], []

    for i, raw_segment in enumerate(dataset_raw):
        # A. ZUPT (紫色)
        acc_metric = acc_metric_list[i]
        zupt_solver = FinalZUPT(best_params)
        trace_zupt = {"px": [], "py": [], "vx": [], "vy": [], "sx": [], "sy": []}
        for ax, ay in acc_metric:
            p, v, s = zupt_solver.update(ax, ay, INTERVAL)
            trace_zupt["px"].append(p[0])
            trace_zupt["py"].append(p[1])
            trace_zupt["vx"].append(v[0])
            trace_zupt["vy"].append(v[1])
            trace_zupt["sx"].append(s[0])
            trace_zupt["sy"].append(s[1])

        # B. RNN (橘色) - 模擬訓練過程中的 Forward
        trace_rnn = {"px": [], "py": []}
        if rnn_model:
            # 數據預處理 (完全依照 RNNDataset logic)
            # x = (nums[1] - E) / B; y = -(nums[0] - D) / A
            raw_x_in = raw_segment[:, 0]
            raw_y_in = raw_segment[:, 1]

            # 注意: train.py 的 logic 是
            # x_in = (raw_y - E) / B  (nums[1] is y from file)
            # y_in = -(raw_x - D) / A (nums[0] is x from file)
            x_in = (raw_y_in - E) / B
            y_in = -(raw_x_in - D) / A

            # Stack into (1, Seq, 2)
            inp_tensor = (
                torch.stack(
                    [
                        torch.tensor(x_in, dtype=torch.float32),
                        torch.tensor(y_in, dtype=torch.float32),
                    ],
                    dim=1,
                )
                .unsqueeze(0)
                .to(device)
            )  # (1, Seq, 2)

            with torch.no_grad():
                # Forward
                # out: (1, seq, 2) -> Velocity
                y_pred = rnn_model(inp_tensor).squeeze(0)  # (seq, 2)

                # Integration Logic (from train.py)
                # p += y_pred[i] * INTERVAL
                # final = p * alpha

                vel_np = y_pred.cpu().numpy()
                pos_np = np.cumsum(vel_np * INTERVAL, axis=0)

                # Apply Alpha scaling
                alpha_val = rnn_model.alpha.item()
                final_pos = pos_np * alpha_val

                trace_rnn["px"] = final_pos[:, 0]
                trace_rnn["py"] = final_pos[:, 1]

        plot_data.append(
            {
                "acc": acc_metric,
                "zupt": trace_zupt,
                "rnn": trace_rnn,
                "label": labels_vec[i] if i < len(labels_vec) else (0, 0),
            }
        )

        all_x.extend(trace_zupt["px"])
        all_x.extend(trace_rnn["px"])
        all_y.extend(trace_zupt["py"])
        all_y.extend(trace_rnn["py"])

    # ==========================================
    # 5. 繪圖 (修改版：統一 Scale + 誤差標註)
    # ==========================================
    print("Plotting with unified scales and error annotations...")

    # --- 步驟 A: 計算全域範圍 (Global Limits) ---

    # 1. 加速度範圍
    all_acc_values = np.concatenate([d["acc"].flatten() for d in plot_data])
    acc_min, acc_max = np.min(all_acc_values), np.max(all_acc_values)
    acc_margin = (acc_max - acc_min) * 0.1
    global_acc_lim = (acc_min - acc_margin, acc_max + acc_margin)

    # 2. 速度範圍
    all_vel_values = []
    for d in plot_data:
        all_vel_values.extend(d["zupt"]["vx"])
        all_vel_values.extend(d["zupt"]["vy"])
    if not all_vel_values:
        all_vel_values = [0]
    vel_min, vel_max = np.min(all_vel_values), np.max(all_vel_values)
    vel_margin = (vel_max - vel_min) * 0.1
    global_vel_lim = (min(vel_min, -0.1) - vel_margin, max(vel_max, 0.1) + vel_margin)

    # 3. 位置範圍
    all_pos_x, all_pos_y = [], []
    for d in plot_data:
        all_pos_x.extend(d["zupt"]["px"])
        all_pos_y.extend(d["zupt"]["py"])
        lx, ly = d["label"]
        all_pos_x.extend([0, lx])
        all_pos_y.extend([0, ly])

    if not all_pos_x:
        all_pos_x = [0]
    if not all_pos_y:
        all_pos_y = [0]

    # 計算最大正方形範圍
    x_min, x_max = np.min(all_pos_x), np.max(all_pos_x)
    y_min, y_max = np.min(all_pos_y), np.max(all_pos_y)
    center_x = (x_min + x_max) / 2
    center_y = (y_min + y_max) / 2
    max_span = max(x_max - x_min, y_max - y_min)
    half_span = (max_span * 1.1) / 2
    if half_span == 0:
        half_span = 1.0

    global_pos_xlim = (center_x - half_span, center_x + half_span)
    global_pos_ylim = (center_y - half_span, center_y + half_span)

    # --- 步驟 B: 開始繪圖 ---
    rows = len(plot_data)
    fig = plt.figure(figsize=(18, 4 * rows), dpi=80)
    plt.subplots_adjust(hspace=0.4, wspace=0.25)

    for i in range(rows):
        d = plot_data[i]
        t = np.arange(len(d["acc"])) * INTERVAL
        lx, ly = d["label"]  # Ground Truth

        # --- 計算誤差 (Euclidean Distance) ---
        # ZUPT Error
        z_end_x, z_end_y = d["zupt"]["px"][-1], d["zupt"]["py"][-1]
        z_err = np.sqrt((z_end_x - lx) ** 2 + (z_end_y - ly) ** 2)

        # RNN Error
        r_err = None
        if len(d["rnn"]["px"]) > 0:
            r_end_x, r_end_y = d["rnn"]["px"][-1], d["rnn"]["py"][-1]
            r_err = np.sqrt((r_end_x - lx) ** 2 + (r_end_y - ly) ** 2)

        # --- 左圖: Acc ---
        ax1 = plt.subplot(rows, 3, i * 3 + 1)
        ax1.plot(t, d["acc"][:, 0], "r", lw=1, alpha=0.6, label="Acc X")
        ax1.plot(t, d["acc"][:, 1], "b", lw=1, alpha=0.6, label="Acc Y")
        ax1.fill_between(
            t,
            global_acc_lim[0],
            global_acc_lim[1],
            where=d["zupt"]["sx"],
            color="red",
            alpha=0.1,
        )
        ax1.fill_between(
            t,
            global_acc_lim[0],
            global_acc_lim[1],
            where=d["zupt"]["sy"],
            color="blue",
            alpha=0.1,
        )
        ax1.set_ylim(global_acc_lim)
        ax1.set_title(f"S{i+1}: Acc ($m/s^2$)")
        ax1.legend(loc="upper right", fontsize="small")
        ax1.grid(True, alpha=0.3)

        # --- 中圖: Velocity ---
        ax2 = plt.subplot(rows, 3, i * 3 + 2)
        ax2.plot(t, d["zupt"]["vx"], "purple", lw=1.5, alpha=0.8, label="ZUPT Vx")
        ax2.plot(t, d["zupt"]["vy"], "mediumpurple", lw=1.5, alpha=0.8, label="ZUPT Vy")
        ax2.axhline(0, c="k", ls="--", lw=0.5)
        ax2.fill_between(
            t,
            global_vel_lim[0],
            global_vel_lim[1],
            where=d["zupt"]["sx"],
            color="red",
            alpha=0.1,
        )
        ax2.fill_between(
            t,
            global_vel_lim[0],
            global_vel_lim[1],
            where=d["zupt"]["sy"],
            color="blue",
            alpha=0.1,
        )
        ax2.set_ylim(global_vel_lim)
        ax2.set_title(f"S{i+1}: ZUPT Vel (cm/s)")
        ax2.legend(loc="upper right", fontsize="small")
        ax2.grid(True, alpha=0.3)

        # --- 右圖: Trajectory ---
        ax3 = plt.subplot(rows, 3, i * 3 + 3)

        # 1. Label (虛線 + 三角形)
        ax3.plot(
            [0, lx],
            [0, ly],
            color="red",
            linestyle="--",
            linewidth=1.5,
            label="Label",
            zorder=5,
        )
        ax3.scatter(lx, ly, color="red", marker="^", s=80, zorder=6)

        # 2. ZUPT (紫色) - Label 顯示誤差
        label_z = f"ZUPT (Err:{z_err:.1f})"
        ax3.plot(
            d["zupt"]["px"],
            d["zupt"]["py"],
            color="purple",
            lw=2,
            alpha=0.6,
            label=label_z,
        )
        ax3.scatter(
            d["zupt"]["px"][-1], d["zupt"]["py"][-1], c="purple", marker="x", s=60
        )
        with open(TRACE_FILE, "a") as f:
            print(f"data {i + 1}", file=f)
            N = len(d["zupt"]["vx"])
            for n in range(N):
                print(f"{d['zupt']['vx'][n]}, {d['zupt']['vy'][n]}", file=f)

        # 3. RNN (橘色) - Label 顯示誤差
        if r_err is not None:
            label_r = f"RNN (Err:{r_err:.1f})"
            ax3.plot(
                d["rnn"]["px"], d["rnn"]["py"], color="tab:orange", lw=2, label=label_r
            )
            ax3.scatter(
                d["rnn"]["px"][-1],
                d["rnn"]["py"][-1],
                c="tab:orange",
                marker="*",
                s=100,
                zorder=7,
            )

        # 統一設定
        ax3.set_xlim(global_pos_xlim)
        ax3.set_ylim(global_pos_ylim)
        ax3.set_aspect("equal")

        ax3.set_title(f"S{i+1}: Trajectory")
        # 圖例放在右下角，字體縮小以避免遮擋
        ax3.legend(loc="lower right", fontsize="small")
        ax3.grid(True, linestyle="--")

    plt.suptitle(
        "Validation: ZUPT vs SimpleRNN (Unified Scale & Error Metrics)", fontsize=16
    )
    plt.tight_layout(rect=[0, 0.03, 1, 0.97])
    plt.savefig(PNG_SAVE_FILE)
    print("Saved to test.png")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train SimpleRNN on custom dataset")
    parser.add_argument("--grid_search", action="store_true", default=False)
    args = parser.parse_args()
    matplotlib.use("Agg")
    main_merged(args)
