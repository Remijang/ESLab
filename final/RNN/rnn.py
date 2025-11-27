import argparse
import json
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
import os


class SequenceDataset(Dataset):
    def __init__(
        self,
        path,
        input_dim=6,
    ):
        self.input_dim = input_dim

        # handle load dataset logic from the path

    def __len__(self):
        return self.size

    def __getitem__(self, idx):
        return self.data[idx]


class RNNModel(nn.Module):
    def __init__(
        self,
        vocab_size,
        embed_dim,
        hidden_dim,
        num_layers,
        dropout=0.2,
    ):
        super().__init__()

        self.embedding = nn.Embedding(vocab_size, embed_dim)

        rnn = nn.RNN

        self.rnn = rnn(
            embed_dim,
            hidden_dim,
            num_layers=num_layers,
            batch_first=True,
            dropout=dropout if num_layers > 1 else 0.0,
        )

        self.fc = nn.Linear(hidden_dim, vocab_size)

    def forward(self, x):
        emb = self.embedding(x)  # [B, T, D]
        out, _ = self.rnn(emb)  # [B, T, H]
        logits = self.fc(out)  # [B, T, V]
        return logits


def load_config(path):
    with open(path) as f:
        return json.load(f)


def train(cfg):

    # Dataset and DataLoader
    dataset = SequenceDataset(
        vocab_size=cfg["model"]["vocab_size"],
        seq_len=cfg["data"]["seq_len"],
        size=cfg["data"]["train_size"],
    )
    loader = DataLoader(dataset, batch_size=cfg["training"]["batch_size"], shuffle=True)

    # Model
    model = RNNModel(
        vocab_size=cfg["model"]["vocab_size"],
        embed_dim=cfg["model"]["embed_dim"],
        hidden_dim=cfg["model"]["hidden_dim"],
        num_layers=cfg["model"]["num_layers"],
        rnn_type=cfg["model"]["type"],
        dropout=cfg["model"]["dropout"],
    )
    model = model.to(cfg["training"]["device"])

    # Optimizer + Loss
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=cfg["training"]["lr"])

    # Output directory
    os.makedirs(cfg["training"]["save_dir"], exist_ok=True)

    # Training
    for epoch in range(cfg["training"]["epochs"]):
        total_loss = 0
        for x, y in loader:
            x = x.to(cfg["training"]["device"])
            y = y.to(cfg["training"]["device"])

            logits = model(x)  # [B, T, V]
            loss = criterion(logits.view(-1, cfg["model"]["vocab_size"]), y.view(-1))

            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

            total_loss += loss.item()

        avgloss = total_loss / len(loader)
        print(f"Epoch {epoch+1}/{cfg['training']['epochs']} - loss={avgloss:.4f}")

        # Save checkpoint
        ckpt_path = os.path.join(cfg["training"]["save_dir"], f"epoch{epoch+1}.pt")
        torch.save(model.state_dict(), ckpt_path)
        print(f"Saved checkpoint → {ckpt_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=str, required=True)
    args = parser.parse_args()

    cfg = load_config(args.config)
    train(cfg)
