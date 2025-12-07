import torch
import argparse


def parse():
    parser = argparse.ArgumentParser(description="Train SimpleRNN on custom dataset")
    parser.add_argument(
        "--save_path",
        type=str,
        default="rnn_params.pth",
        help="Where to save model parameters (state_dict).",
    )
    parser.add_argument(
        "--dump_path",
        type=str,
        default="state.txt",
        help="Where to save model parameters (state_dict).",
    )
    args = parser.parse_args()
    return args


def dump(args):
    model = torch.load(args.save_path)
    with open(args.dump_path, "w") as f:
        print(model, file=f)


if __name__ == "__main__":
    args = parse()
    dump(args)
