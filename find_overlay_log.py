import os

paths = [
    r"C:\Program Files\obs-studio\bin\64bit\logs\overlay.log",
    r"C:\Program Files\obs-studio\bin\64bit\overlay.log",
    os.path.expandvars(r"%APPDATA%\obs-studio\logs\overlay.log"),
    os.path.expandvars(r"%APPDATA%\obs-studio\plugin_config\yt-chat-overlay\overlay.log"),
    r"D:\Download\Proj\logs\overlay.log"
]

for p in paths:
    if os.path.exists(p):
        print(f"FOUND: {p}")
        with open(p, 'r') as f:
            print(f.read())
    else:
        print(f"Not found: {p}")
