import ctypes
from ctypes import wintypes

user32 = ctypes.windll.user32

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

def enum_proc(hwnd, lparam):
    if user32.IsWindowVisible(hwnd):
        length = user32.GetWindowTextLengthW(hwnd)
        buff = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buff, length + 1)
        
        class_buff = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, class_buff, 256)
        
        title = buff.value
        cls = class_buff.value
        if 'OBS' in title or 'obs' in title.lower():
            print(f"HWND: {hwnd:#x}, Class: {cls}, Title: {title}")
    return True

user32.EnumWindows(WNDENUMPROC(enum_proc), 0)
