import matplotlib.pyplot as plt


def main():
    x = []
    y1 = []
    y2 = []

    # Create a figure
    fig, ax = plt.subplots()

    ax.plot(x, y1, label="Line 1", linewidth=2)
    ax.plot(x, y2, label="Line 2", linewidth=2)

    ax.set_title("Two Lines Example")
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.legend()

    # Show window on desktop
    plt.show()


if __name__ == "__main__":
    main()
