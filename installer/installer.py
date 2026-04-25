import tkinter as tk
from tkinter import filedialog, messagebox
import shutil
import os

def select_directory():
    path = filedialog.askdirectory()
    if path:
        entry_var.set(path)

def start_install():
    dest = entry_var.get()
    if not dest:
        messagebox.showerror("Error", "Please select a directory first!")
        return
    
    try:
        # Assuming your 'payload.exe' is in the same folder as this script
        shutil.copy("payload.exe", dest)
        messagebox.showinfo("Success", f"Installed to {dest}")
    except Exception as e:
        messagebox.showerror("Error", str(e))

root = tk.Tk()
root.title("My Custom Installer")

entry_var = tk.StringVar()

# UI Layout
tk.Label(root, text="Installation Path:").pack(pady=5)
tk.Entry(root, textvariable=entry_var, width=50).pack(pady=5)
tk.Button(root, text="Change Directory", command=select_directory).pack(pady=5)
tk.Button(root, text="Start Installation", command=start_install, bg="green", fg="white").pack(pady=20)

root.mainloop()