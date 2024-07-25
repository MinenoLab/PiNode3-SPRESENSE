import tkinter as tk
from lib.SpresenseCameraChecker import SpresenseCameraChecker

if __name__ == "__main__":
    # ウィンドウを作成してプログラムを実行
    root = tk.Tk()
    app = SpresenseCameraChecker(window=root, window_title="SPRESENSE Camera Checker", port_num=0)