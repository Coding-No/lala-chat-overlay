import os
import shutil
import glob

appdata = os.environ.get('APPDATA', '')
logs_dir = os.path.join(appdata, 'obs-studio', 'logs')
files = sorted(glob.glob(os.path.join(logs_dir, '*.txt')), key=os.path.getmtime, reverse=True)
if files:
    latest = files[0]
    print(f"Latest log: {latest}")
    shutil.copyfile(latest, 'obs_latest.log')
    print("Copied to obs_latest.log")
else:
    print("No logs found in", logs_dir)
