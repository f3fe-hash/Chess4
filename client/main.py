import tkinter as tk

from config import SERVER_HOST, SERVER_PORT
from gui import ChessGUI


def main():
    root = tk.Tk()

    ChessGUI(
        root,
        SERVER_HOST,
        SERVER_PORT
    )

    root.mainloop()


if __name__ == "__main__":
    main()