# LALA Live Chat Overlay

Plugin chat YouTube buat OBS. **Nol butuh API key** — plugin baca live chat-nya langsung dari halaman YouTube.

## Cara pasang (gampang)

1. **Tutup OBS dulu.**
2. Klik kanan `LalaLiveChatOverlaySetup.exe` → **Run as Administrator**.
3. Arahin installernya ke folder OBS kamu (yang isinya `obs64.exe`, misal `C:\Program Files\obs-studio`).
4. Buka OBS → menu **Docks** → centang **LALA Live Chat Overlay**.
5. Tempel URL video live YouTube di panelnya. Udah, jalan.

Unduh installer: [Releases](https://github.com/Coding-No/lala-chat-overlay/releases/latest)

## Kalau dock-nya nol muncul

- Pastikan OBS beneran ketutup pas install. Installer matiin OBS sendiri, tapi kadang nyangkut — tutup manual dari Task Manager.
- Jalanin sebagai **Administrator**. Kalau nol, DLL nol bisa kesalin ke `C:\Program Files\obs-studio`.
- Cek ada nol `yt-chat-overlay.dll` di `...\obs-studio\obs-plugins\64bit\`.
- Masih nol muncul → install **Visual C++ Redistributable x64** dari Microsoft, terus buka ulang OBS.
- Pastikan OBS-nya **64-bit** (semua OBS modern udah 64-bit).

## Ada apa aja

- Baca chat YouTube live tanpa API key
- Panel **Dock** di dalam OBS — atur sendiri taruhnya di mana
- Kustom font & warna per bagian (pesan, username, shadow)
- Deteksi window capture biar overlay nol ketangkep di rekaman

## Uninstall

Jalanin `uninstall_plugin.bat` — dia bersihin DLL dari semua jalur pasang.

## Build dari source

Butuh Visual Studio 2022 + CMake. Build **WAJIB Release**:

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Atau tinggal jalanin `rebuild_release.bat`. CI di GitHub juga otomatis build tiap push, plus ada gate yang nolak build kalau hasilnya masih nyantol CRT debug — biar DLL-nya pasti jalan di PC orang yang nol ada Visual Studio.

---
Kalau ada bug, buka **Issues**.
