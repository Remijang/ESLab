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

MEAN = torch.tensor([0.0, 0.0])
STD = torch.tensor([2.5, 2.5])
AUGMENTED_RATIO = 1
DEVICE = "cuda"


class RNNDataset:
    def __init__(self):
        self.data = self._load_file(DATA_FILE)
        self.label = self._load_label(LABEL_FILE)

    def _load_file(self, filename):
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
                x = (nums[1] - E) / B
                y = -(nums[0] - D) / A
                nums[0] = x
                nums[1] = y
                nums = torch.tensor(nums).unsqueeze(dim=0)
                result[count].append(nums)
        for i in range(count + 1):
            result[i] = torch.cat(result[i])
            for _ in range(AUGMENTED_RATIO - 1):
                tmp = result[i].clone()
                for k in range(len(result[i])):
                    if random.random() < 0.1:
                        tmp[k] += torch.normal(MEAN, STD)
                augmented_result.append(tmp.to(DEVICE))
            augmented_result.append(result[i].to(DEVICE))
        return augmented_result

    def _load_label(self, filename):
        result = []
        augmented_result = []
        with open(filename, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                nums = torch.tensor(list(map(float, line.replace(" ", "").split(","))))
                result.append(nums)
        for i in range(len(result)):
            for _ in range(AUGMENTED_RATIO):
                augmented_result.append(result[i].to(DEVICE))

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
        self.rnn = nn.RNN(
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


def main(args):
    dataset = RNNDataset()
    model = SimpleRNN(
        input_size=2, hidden_size=args.hidden_size, output_size=2, num_layers=args.layer
    ).to(DEVICE)
    if args.load_path != "":
        model.load_state_dict(torch.load(args.load_path))
    criterion = nn.MSELoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=0.01)
    N = len(dataset)
    pbar = tqdm(range(args.epochs))
    loss_history = []
    for epoch in pbar:
        model.train()
        total_loss = 0.0
        optimizer.zero_grad()
        batch_loss = torch.zeros((1,), device=DEVICE)
        for n in range(N):
            x, y = dataset[n]
            p = torch.tensor([0.0, 0.0], device=DEVICE)
            L = len(x)
            y_pred, _ = model(x)
            for i in range(L):
                p += y_pred[i] * INTERVAL
            batch_loss += criterion(p * model.alpha, y)
        batch_loss /= N
        loss_history.append(batch_loss)
        if batch_loss == min(loss_history):
            torch.save(model.state_dict(), args.save_path)
        batch_loss.backward()
        optimizer.step()
        pbar.set_postfix({"train_loss": f"{batch_loss.item():.3f}"})
    print(f"Saved model params to {args.save_path}")
    print(f"minimum loss is {min(loss_history)}")


def parse():
    parser = argparse.ArgumentParser(description="Train SimpleRNN on custom dataset")
    parser.add_argument("--epochs", type=int, default=10000)
    parser.add_argument("--hidden_size", type=int, default=20)
    parser.add_argument("--layer", type=int, default=2)
    parser.add_argument("--lr", type=float, default=1e-3)
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
