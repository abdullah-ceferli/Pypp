import tkinter as tk
from tkinter import filedialog, messagebox
import shutil
import os
import sys

def resource_path(relative_path):
    try:
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)

class PyppInstaller:
    def __init__(self, root):
        self.root = root
        self.root.title("Pypp Installer")
        self.root.geometry("400x200")
        
        self.install_path = tk.StringVar(value=os.path.join(os.path.expanduser("~"), "Pypp"))

        tk.Label(root, text="Select Installation Directory:", font=("Arial", 10)).pack(pady=10)
        frame = tk.Frame(root)
        frame.pack(pady=5)
        tk.Entry(frame, textvariable=self.install_path, width=40).pack(side=tk.LEFT, padx=5)
        tk.Button(frame, text="Browse", command=self.browse_folder).pack(side=tk.LEFT)

        tk.Button(root, text="INSTALL PYPP", 
                  command=self.run_install, 
                  bg="#2ecc71", fg="white", font=("Arial", 10, "bold"), height=2).pack(pady=20)

    def browse_folder(self):
        selected = filedialog.askdirectory()
        if selected:
            self.install_path.set(selected)

    def run_install(self):
        target_dir = self.install_path.get()
        try:
            if not os.path.exists(target_dir):
                os.makedirs(target_dir)

            # Look for the ALREADY COMPILED exe
            source_exe = resource_path("pypp.exe") 
            
            # Copy the binary to the user's path
            shutil.copy(source_exe, os.path.join(target_dir, "pypp.exe"))
            
            messagebox.showinfo("Success", f"Pypp has been installed to:\n{target_dir}")
            self.root.destroy() # Close installer when done

        except Exception as e:
            messagebox.showerror("Error", f"Installation failed: {str(e)}")

if __name__ == "__main__":
    root = tk.Tk()
    app = PyppInstaller(root)
    root.mainloop()