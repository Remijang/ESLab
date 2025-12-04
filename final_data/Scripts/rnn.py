import argparse

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

DATA_FILE = "DATAS/data.txt"
LABEL_FILE = "DATAS/label.txt"


class RNNDataset:
    def __init__(self):
        self.data = self._load_file(DATA_FILE)
        self.label = self._load_label(LABEL_FILE)

    def _load_file(self, filename):
        result = []
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
                x = -(nums[1] - E) / B
                y = (nums[0] - D) / A
                nums[0] = x
                nums[1] = y
                nums = torch.tensor(nums).unsqueeze(dim=0)
                result[count].append(nums)
        for i in range(count + 1):
            result[i] = torch.cat(result[i])

        return result

    def _load_label(self, filename):
        result = []

        with open(filename, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                nums = torch.tensor(list(map(float, line.replace(" ", "").split(","))))
                result.append(nums)

        return result

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
        self.rnn = nn.RNN(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
        )

        self.output = nn.Linear(hidden_size, output_size)
        self.alpha = nn.Parameter(torch.tensor(1.0))

    def forward(self, x):
        # x shape: (batch, seq_len, input_size)
        h0 = torch.zeros(1, self.hidden_size)
        out, hn = self.rnn(x, h0)
        hidden = out
        out = self.output(out)  # (batch, output_size)

        return out, hidden


INTERVAL = 0.01


def main(args):
    dataset = RNNDataset()
    model = SimpleRNN(
        input_size=2, hidden_size=args.hidden_size, output_size=2, num_layers=1
    )

    criterion = nn.MSELoss()
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr)
    N = len(dataset)
    pbar = tqdm(range(args.epochs))
    loss_history = []
    for epoch in pbar:
        model.train()
        total_loss = 0.0
        for n in range(N):
            x, y = dataset[n]
            optimizer.zero_grad()
            v = torch.tensor([0.0, 0.0])
            p = torch.tensor([0.0, 0.0])
            L = len(x)
            for i in range(L):
                v += x[i] * INTERVAL
                p += v * INTERVAL

            y_pred, _ = model(x)
            v = torch.tensor([0.0, 0.0])
            for i in range(L):
                v += y_pred[i]
                p += v * INTERVAL
            loss = criterion(p * model.alpha, y)
            loss.backward()
            optimizer.step()

            total_loss += loss.item()

        avg_loss = total_loss / len(dataset)
        pbar.set_postfix({"loss": f"{avg_loss:.3f}"})
        loss_history.append(avg_loss)
        if avg_loss == min(loss_history):
            torch.save(model.state_dict(), args.save_path)
    print(f"Saved model params to {args.save_path}")


def parse():
    parser = argparse.ArgumentParser(description="Train SimpleRNN on custom dataset")
    parser.add_argument("--epochs", type=int, default=10000)
    parser.add_argument("--hidden_size", type=int, default=15)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument(
        "--save_path",
        type=str,
        default="rnn_params.pth",
        help="Where to save model parameters (state_dict).",
    )
    args = parser.parse_args()
    return args


if __name__ == "__main__":
    args = parse()
    main(args)
