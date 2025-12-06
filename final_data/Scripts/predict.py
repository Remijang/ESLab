def predict_by_acc(filename):
    result = []
    INTERVAL = 0.01
    count = 0
    with open(filename, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            if line.startswith("data"):
                result.append([])
                if count != 0:
                    print(f"line {count:2}: {x :.2f}, {y :.2f}")
                count += 1
                vx, vy = 0, 0
                x, y = 0, 0
                continue

            nums = list(map(float, line.replace(" ", "").split(",")))
            vx += (nums[0] + 11) * INTERVAL
            vy += (nums[1] - 13) * INTERVAL
            x += vy * INTERVAL
            y -= vx * INTERVAL
    print(f"line {count:2}: {x:3.6f}, {y:.6f}")


if __name__ == "__main__":
    predict_by_acc("DATAS/data.txt")
