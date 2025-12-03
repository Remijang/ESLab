import argparse
import math

import numpy as np

DATA_FILE = "calibration.txt"


def linear_regression(X, y):
    """
    Fit linear regression using the Normal Equation:
        w = (X^T X)^(-1) X^T y
    """
    w = np.linalg.inv(X.T @ X) @ X.T @ y
    return w


def predict(X, w):
    return X @ w


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Program with verbose mode")
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable verbose output"
    )
    args = parser.parse_args()

    result = {}
    with open(DATA_FILE, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            if line.startswith("data"):
                current_key = line
                result[current_key] = []
                continue

            if current_key is not None:
                nums = list(map(int, line.replace(" ", "").split(",")))
                result[current_key].append(nums)

    datas = []
    for key, val in result.items():
        length = len(val)
        if length == 0:
            continue
        datas.append(val[length // 2])
    X = []
    y = []
    for data in datas:
        X.append(
            [data[0] ** 2, data[1] ** 2, 2 * data[0], 2 * data[1], 2 * data[2], -1]
        )
        y.append(-(data[2] ** 2))

    X = np.array(X)
    y = np.array(y)
    w = linear_regression(X, y)

    if args.verbose:
        print("Weights (bias, slope):", w)

    a = math.sqrt(1 / w[0])
    b = math.sqrt(1 / w[1])
    c = 1
    d = w[2] * (-a)
    e = w[3] * (-b)
    f = -w[4]
    G = math.sqrt(w[5] - (d / a) ** 2 - (e / b) ** 2 - (f / c) ** 2)
    print(f"a = {a}, b = {b}, c = {c}")
    print(f"d = {d}, e = {e}, f = {f}")
    print(f"G = {G}")

    err = 0
    if args.verbose:
        for data in datas:
            print("original data:")
            print(f"    {data[0]}, {data[1]}, {data[2]}")
            mag = math.sqrt((data[0] ** 2) + (data[1] ** 2) + (data[2] ** 2))
            print(f"Magnitude: ")
            print(f"    {mag}")
            print("calibrated data:")
            x, y, z = (data[0] - d) / a, (data[1] - e) / b, (data[2] - f) / c
            print(f"    {x}, {y}, {z}")
            mag = math.sqrt((x**2) + (y**2) + (z**2))
            print(f"Magnitude: ")
            print(f"    {mag}")
            print("")
