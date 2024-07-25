import serial
import serial.tools.list_ports
import time
import cv2
import tkinter as tk
from PIL import Image, ImageTk
from tkinter import ttk
import threading
from lib.Spresense import Spresense

class SpresenseCameraChecker:
    def __init__(self, window, window_title, port_num=0):
        self.window = window
        self.window.title(window_title)

        self.port_num = port_num
        self.com_ports = list_com_port()
        self.com_port = self.com_ports[self.port_num]
        self.ser = serial.Serial(self.com_port.device, Spresense.BAUD_RATE, timeout = 3)
        time.sleep(3)
        self.spresense = Spresense(self.ser)

        self.canvas = tk.Canvas(window, width=640, height=480)# type: ignore
        self.canvas.pack()

        self.btn_snapshot = tk.Button(window, text="Capture", width=10, command=self.update)
        self.btn_snapshot.pack(padx=10, pady=10)

        self.progress = ttk.Progressbar(window, orient='horizontal', length=100, mode='indeterminate')
        self.progress_explain = tk.Label(window, text="Capturing...", font=("Helvetica", 16))



        self.window.mainloop()

    def update_image(self):
        self.progress.place(x=320, y=240, anchor=tk.CENTER)
        self.progress_explain.place(x=320, y=200, anchor=tk.CENTER)
        self.progress.start(10)

        ret, frame = self.spresense.read()
        frame = cv2.resize(frame, (640, 480))
        if ret:
            self.photo = ImageTk.PhotoImage(image=Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))) # type: ignore
            self.canvas.create_image(0, 0, image=self.photo, anchor=tk.NW)

        self.progress.stop()
        self.progress_explain.place_forget()
        self.progress.place_forget()

    def update(self):
        t = threading.Thread(target=self.update_image)
        t.start()

    def __del__(self):
        self.ser.close()

def list_com_port():
    com_ports = list(serial.tools.list_ports.comports())
    return com_ports