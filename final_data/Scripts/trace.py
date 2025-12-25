from tkinter import *


def Mousecoords(event):
    x, y = event.x, event.y
    for i, r in enumerate(radii):
        canvas.coords(glow[i], x - r, y - r, x + r, y + r)


root = Tk()
root.title("Laser Cursor (Glow)")
root.config(cursor="none")

canvas = Canvas(root, width=5000, height=2500, bg="black")
canvas.pack()

radii = [15, 9, 5]
colors = ["#440000", "#aa0000", "#ff0000"]

glow = []
for r, c in zip(radii, colors):
    glow.append(canvas.create_oval(0, 0, 2 * r, 2 * r, fill=c, outline=""))

canvas.bind("<Motion>", Mousecoords)

root.mainloop()
