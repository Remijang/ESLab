import argparse
import random
import torch
from torch import nn
from tqdm.auto import tqdm

A = 1.0011583795355363
B = 0.995068796668601
C = 1
D = -1.6759465583366173
E = -1.537611331891522
F = 8.423808603551663
G = 1003.5581817562975

DATA_FILE = "Datas/data.txt"
LABEL_FILE = "Datas/label.txt"
DATA_AUG_FILE = "Datas/data_reflection.txt"
LABEL_AUG_FILE = "Datas/label_reflection.txt"
REG_DATA_FILE = "Datas/reg_data.txt"
REG_LABEL_FILE = "Datas/reg_label.txt"
MASK_FILE = "Datas/mask.txt"

MEAN = torch.tensor([0.0, 0.0])
STD = torch.tensor([2.5, 2.5])
AUGMENTED_RATIO = 1
DEVICE = "cuda"


class RNNDataset:
    def __init__(self, use_reflection=False):
        self.data = self._load_file(DATA_FILE, maskfile=MASK_FILE)
        if use_reflection:
            self.data = self._load_file(DATA_AUG_FILE)
        self.label = self._load_label(LABEL_FILE, maskfile=MASK_FILE)
        if use_reflection:
            self.label = self._load_label(LABEL_AUG_FILE)
        self.reg_data = self._load_file(REG_DATA_FILE)
        self.reg_label = self._load_reglabel(REG_LABEL_FILE)

    def _load_file(self, filename, maskfile=""):
        result = []
        augmented_result = []
        count = -1
        masked = []
        if maskfile != "":
            with open(maskfile, "r") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    masked.append(int(line))

        with open(filename, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                if line.startswith("data"):
                    result.append([])
                    count += 1
                    continue

                nums = list(map(float, line.replace(" ", "").split(",")))
                x = (nums[1] - E) / B
                y = -(nums[0] - D) / A
                nums[0] = x
                nums[1] = y
                nums = torch.tensor(nums).unsqueeze(dim=0)
                result[count].append(nums)
        for i in range(count + 1):
            if i in masked:
                continue
            result[i] = torch.cat(result[i])
            for _ in range(AUGMENTED_RATIO - 1):
                tmp = result[i].clone()
                for k in range(len(result[i])):
                    if random.random() < 0.1:
                        tmp[k] += torch.normal(MEAN, STD)
                augmented_result.append(tmp.to(DEVICE))
            augmented_result.append(result[i].to(DEVICE))
        return augmented_result

    def _load_reglabel(self, filename):
        result = []
        augmented_result = []
        count = -1

        with open(filename, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                if line.startswith("data"):
                    result.append([])
                    count += 1
                    continue

                nums = list(map(float, line.replace(" ", "").split(",")))
                nums = torch.tensor(nums).unsqueeze(dim=0)
                result[count].append(nums)
        for i in range(count + 1):
            result[i] = torch.cat(result[i])
            for _ in range(AUGMENTED_RATIO):
                augmented_result.append(result[i].clone().to(DEVICE))
        return augmented_result

    def _load_label(self, filename, maskfile=""):
        result = []
        augmented_result = []
        masked = []
        if maskfile != "":
            with open(maskfile, "r") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    masked.append(int(line))
        with open(filename, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                nums = torch.tensor(list(map(float, line.replace(" ", "").split(","))))
                result.append(nums)
        for i in range(len(result)):
            if i in masked:
                continue
            for _ in range(AUGMENTED_RATIO):
                augmented_result.append(result[i].clone().to(DEVICE))

        return augmented_result

    def __getitem__(self, key):
        return self.data[key], self.label[key]

    def __len__(self):
        return len(self.data)

    def keys(self):
        return self.data.keys()


class SimpleRNN(nn.Module):
    def __init__(self, input_size, hidden_size, output_size, num_layers=1):
        super(SimpleRNN, self).__init__()

        self.hidden_size = hidden_size
        self.num_layers = num_layers

        # You can replace nn.RNN with nn.GRU or nn.LSTM
        self.rnn = nn.GRU(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
        )

        self.output = nn.Linear(hidden_size, output_size)
        self.alpha = nn.Parameter(torch.tensor(1.0))

    def forward(self, x):
        # x shape: (batch, seq_len, input_size)
        h0 = torch.zeros(self.num_layers, self.hidden_size, device=DEVICE)
        out, hn = self.rnn(x, h0)
        hidden = out
        out = self.output(out)  # (batch, output_size)

        return out, hidden


INTERVAL = 0.01
BATCH_SIZE = 16


def main(args):
    dataset = RNNDataset(use_reflection=args.use_reflection)
    model = SimpleRNN(
        input_size=2, hidden_size=args.hidden_size, output_size=2, num_layers=args.layer
    ).to(DEVICE)
    if args.load_path != "":
        model.load_state_dict(torch.load(args.load_path))
    criterion = nn.MSELoss(reduction="sum")
    gelu = nn.GELU()
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=0.01)
    N = len(dataset)
    M = len(dataset.reg_data)
    shuffle = [i for i in range(N)]
    pbar = tqdm(range(args.epochs))
    print(N, M)
    loss_history = []
    for _ in pbar:
        model.train()
        random.shuffle(shuffle)
        epoch_train_loss, epoch_reg_loss, epoch_drift_loss = 0.0, 0.0, 0.0
        for b in range(0, N, BATCH_SIZE):
            optimizer.zero_grad()
            train_loss = torch.zeros((1,), device=DEVICE)
            reg_loss = torch.zeros((1,), device=DEVICE)
            drift_loss = torch.zeros((1,), device=DEVICE)
            for n in range(b, min(N, b + BATCH_SIZE)):
                x, y = dataset[shuffle[n]]
                p = torch.tensor([0.0, 0.0], device=DEVICE)
                L = len(x)
                y_pred, _ = model(x)
                for i in range(L):
                    p += y_pred[i] * INTERVAL
                    if i > 0:
                        acc_drift = (y_pred[i] - y_pred[i - 1]) - (
                            x[i] + x[i - 1]
                        ) * INTERVAL / 2
                        drift_loss += gelu(
                            acc_drift[0] ** 2 + acc_drift[1] ** 2 - args.thresh
                        )
                train_loss += criterion(p * model.alpha, y)
            train_loss /= min(N, b + BATCH_SIZE) - b
            drift_loss /= min(N, b + BATCH_SIZE) - b

            for n in range(M):
                x, y = dataset.reg_data[n], dataset.reg_label[n]
                p = torch.tensor([0.0, 0.0], device=DEVICE)
                L = len(x)
                y_pred, _ = model(x)
                y_pred *= model.alpha
                reg_loss += criterion(y_pred, y) * INTERVAL
            reg_loss /= M
            total_loss = (
                args.p_weight * train_loss
                + args.v_weight * reg_loss
                + args.a_weight * drift_loss
            )
            epoch_train_loss += (train_loss.item()) * (min(N, b + BATCH_SIZE) - b)
            epoch_reg_loss += reg_loss.item()
            epoch_drift_loss += (drift_loss.item()) * (min(N, b + BATCH_SIZE) - b)
        epoch_train_loss /= N
        epoch_drift_loss /= N
        epoch_reg_loss /= (N + BATCH_SIZE - 1) // BATCH_SIZE
        epoch_total_loss = (
            args.p_weight * epoch_train_loss
            + args.v_weight * epoch_reg_loss
            + args.a_weight * epoch_drift_loss
        )
        loss_history.append(total_loss.item())
        if total_loss.item() == min(loss_history):
            torch.save(model.state_dict(), args.save_path)
        total_loss.backward()
        optimizer.step()
        pbar.set_postfix(
            {
                "train_loss": f"{epoch_train_loss:.2f}",
                "reg_loss": f"{epoch_reg_loss:.2f}",
                "drift_loss": f"{epoch_drift_loss:.2f}",
                "total_loss": f"{epoch_total_loss:.2f}",
            }
        )
    print(f"Saved model params to {args.save_path}")
    print(f"minimum loss is {min(loss_history)}")


def parse():
    parser = argparse.ArgumentParser(description="Train SimpleRNN on custom dataset")
    parser.add_argument("--epochs", type=int, default=10000)
    parser.add_argument("--hidden_size", type=int, default=20)
    parser.add_argument("--layer", type=int, default=3)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--thresh", type=float, default=5)
    parser.add_argument("--p_weight", type=float, default=1)
    parser.add_argument("--v_weight", type=float, default=0.1)
    parser.add_argument("--a_weight", type=float, default=1)
    parser.add_argument("--use_reflection", action="store_true", default=False)
    parser.add_argument(
        "--save_path",
        type=str,
        default="rnn_params.pth",
        help="Where to save model parameters (state_dict).",
    )
    parser.add_argument(
        "--load_path",
        type=str,
        default="",
    )
    args = parser.parse_args()
    return args


if __name__ == "__main__":
    args = parse()
    main(args)
