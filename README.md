# Lala Live Chat Overlay for OBS

> **Production-Ready OBS Studio Plugin & Standalone Windows Overlay for Single-Monitor Live Streamers**  
> *Crafted with ❤️ by Coding-No*  
> Read public YouTube live chat transparently over games without API keys, third-party relays, or screen capture leakage.

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(64--bit)-blue.svg)](#system-requirements)
[![OBS Studio](https://img.shields.io/badge/OBS%20Studio-v29%20%7C%20v30%20%7C%20v31%20%7C%20v32+-purple.svg)](#obs-integration)
[![Donation](https://img.shields.io/badge/Donasi-Trakteer-red.svg)](https://trakteer.id/nopauwxp/gift)
[![GitHub](https://img.shields.io/badge/GitHub-Coding--No-black.svg)](https://github.com/Coding-No)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## Overview

Playing PC games on a **single monitor** while live streaming presents a major dilemma: you need to see your audience's live chat, but alt-tabbing breaks your gameplay flow, and traditional chat overlays either steal your keyboard focus, capture clicks, or leak chat into your stream recording.

**Lala Live Chat Overlay for OBS** solves this completely:
1. **Direct Connection:** You simply paste your YouTube livestream URL. The plugin connects directly to YouTube's live chat stream without needing Google API keys, OAuth logins, or third-party relay bots (Streamlabs, StreamElements, Restream).
2. **True Transparent Overlay:** Renders floating, anti-aliased chat messages directly over your borderless fullscreen game. Support for customizable transparent background cards and crisp bold typography.
3. **Zero Gameplay Interference:** The overlay window uses Windows `WS_EX_TRANSPARENT` and `WS_EX_NOACTIVATE`. Every mouse click, scroll, and keystroke passes directly through to your game. The overlay never steals focus.
4. **Stream Capture Exclusion:** Built-in Windows `WDA_EXCLUDEFROMCAPTURE` technology ensures **you see the chat on your screen, but your stream viewers see only the game**.

---

## Architecture

The project is built with clean separation of concerns and high modularity:

```
[YouTube Stream URL]
        │
        ▼
[YouTubeChatProvider] ──── (Backoff Reconnect: 2s, 5s, 10s, 20s, 30s)
        │
        ├─► [ChatDiscovery]        (Extracts Video ID, API Key, InnerTube Token)
        ├─► [ChatParser]           (Fetches live batches, actions, & continuations)
        ├─► [MessageParser]        (Sanitizes input, formats runs & Super Chats)
        └─► [UserMetadataParser]   (Resolves avatars & roles: Owner, Mod, Member, Verified)
        │
        ▼
[ChatMessage Queue] ──────── (Deduplication by messageId & Spam Guard)
        │
        ▼
[OverlayController] ──────── (DPI awareness, 60 FPS ticker, Auto-hide, Layout)
        │
   ┌────┴────────────────────────┐
   ▼                             ▼
[WindowsOverlayWindow]      [ChatRenderer]
 - WS_EX_TOPMOST             - GDI+ Hardware Antialiased
 - WS_EX_TRANSPARENT         - 32-bit ARGB DIB Section
 - WS_EX_NOACTIVATE          - Circular Avatar Compositing
 - WDA_EXCLUDEFROMCAPTURE    - Role Badges & Text Shadows
```

### Abstraction Layer
The YouTube pipeline is isolated into dedicated abstraction classes:
- **`YouTubeChatProvider`**: Manages thread lifecycle, state machine (`Connecting`, `Connected`, `Reconnecting`, `Error`), and asynchronous avatar caching.
- **`ChatDiscovery`**: Resolves any YouTube URL format (`watch?v=`, `youtu.be/`, `/live/`, `/shorts/`), extracts `INNERTUBE_API_KEY` and client version, and queries the InnerTube `next` endpoint for the active `liveChatRenderer`.
- **`ChatParser`**: Communicates with the InnerTube `get_live_chat` endpoint, managing continuation cursors and polling timeouts.
- **`MessageParser`**: Enforces strict untrusted input sanitization (removes control codes, encodes raw HTML, truncates abuse payloads, preserves unicode/emojis).
- **`UserMetadataParser`**: Categorizes viewer roles (`Owner`, `Moderator`, `Member`, `Verified`, `Regular`) and resolves highest-resolution avatars.

---

## OBS Capture Method Matrix

| Capture Method in OBS | Underlying Technology | Streamer Sees Chat? | Stream Sees Chat? | Status & Technical Explanation |
| :--- | :--- | :---: | :---: | :--- |
| **OBS Game Capture** | In-process graphics hook (`graphics-hook.dll`) hooking DirectX/Vulkan `Present` | **YES** | **NO (Excluded)** | **Supported.** The hook captures frames inside the game process before DWM composites desktop windows. |
| **OBS Window Capture** | Win32 PrintWindow / WGC targeted at game window | **YES** | **NO (Excluded)** | **Supported.** Captures only the game's specific HWND; the overlay HWND is omitted. |
| **OBS Display Capture (WGC)** | Windows Graphics Capture (`Windows.Graphics.Capture`) | **YES** | **NO (Excluded)** | **Supported.** Desktop Window Manager (DWM) omits windows with `WDA_EXCLUDEFROMCAPTURE`. |
| **OBS Display Capture (DXGI)** | DXGI Desktop Duplication API | **YES** | **NO (Excluded)** | **Supported.** Hardware compositor excludes the window surface from the capture buffer. |
| **Legacy GDI BitBlt** | Screen DC memory copy | **YES** | Depends (Black Box) | Deprecated. On older Windows builds, legacy BitBlt may draw a black box where the excluded window is. OBS 30+ uses WGC/DXGI by default. |

### Borderless Fullscreen vs. Exclusive Fullscreen
- **Borderless Fullscreen (Recommended):** The game runs as a maximized borderless window. Windows Desktop Window Manager (DWM) handles composition, allowing our topmost layered overlay to render seamlessly above the game with hardware acceleration, zero click interference, and capture exclusion.
- **Exclusive Fullscreen (Known OS Limitation):** In true Exclusive Fullscreen, the game takes direct exclusive control of the GPU display adapter, bypassing DWM composition. No external Windows desktop application can draw on top of Exclusive Fullscreen without injecting code into the game process (which triggers anti-cheat bans like Easy Anti-Cheat, BattlEye, Vanguard). The plugin gracefully detects fullscreen changes, will **never crash**, and advises the user to select **Borderless Fullscreen** in game settings.

---

## Features

- **No Third-Party Relay Services:** Zero dependency on Streamlabs, StreamElements, Restream, or third-party servers.
- **No Google API Key / Quota:** Connects directly via public streaming protocols without paying for or configuring Google Cloud console projects.
- **True Transparency:** No background boxes or dimming overlays. Pure text floating over your game.
- **High-Contrast Text Shadows:** Built-in drop shadows ensure readability whether you are playing in a dark dungeon or bright sunny field.
- **Circular Avatars:** Real YouTube profile pictures with automatic fallback to role-colored initials.
- **Role Badges:** Compact visual badges for **Owner**, **Moderator**, **Member**, and **Verified**.
- **Chat Duration & Smooth Fade:** Configurable expiration (3s, 5s, 10s, 15s, 30s, 60s, Never) with smooth alpha fade-out.
- **Maximum Chat Limiter:** Caps simultaneous messages (3, 5, 8, 10, 15, 20) to prevent screen clutter.
- **Live Preview System:** Hit **PREVIEW** in the settings dialog to view dummy messages and tweak position, size, opacity, colors, and margins in real time.
- **Auto-Hide:** Automatically fades overlay out if no chat is sent for X seconds, reappearing instantly upon new chat.
- **Spam Guard & Rate Limiting:** Deduplicates messages by ID and processes rapid chat floods smoothly without stuttering or memory leaks.
- **Auto-Reconnect:** Exponential backoff reconnects automatically (2s, 5s, 10s, 20s, 30s) on network drops.
- **Role Filtering:** Choose to display chat from Everyone, Moderators Only, Members Only, or Verified Viewers.

---

## Project Structure

```
youtube-chat-overlay/
├── CMakeLists.txt              # Unified CMake build script (Ninja / MSVC)
├── README.md                   # Complete documentation & user guide
├── LICENSE                     # MIT Open Source License
├── main.cpp                    # Standalone application entrypoint (YouTubeChatOverlay.exe)
├── models/
│   ├── ChatMessage.hpp         # ChatMessage, BadgeInfo, and rendering state
│   ├── ChatConfig.hpp          # Settings, layout presets, and JSON persistence
│   └── UserRole.hpp            # UserRole enum & helpers
├── youtube/
│   ├── HttpClient.hpp/.cpp     # Native Windows WinHttp client (TLS 1.3, zero 3rd party libs)
│   ├── ChatDiscovery.hpp/.cpp  # URL validation & InnerTube continuation discovery
│   ├── ChatParser.hpp/.cpp     # Chat batch fetcher & pagination parser
│   ├── MessageParser.hpp/.cpp  # Text run extraction & untrusted input sanitization
│   ├── UserMetadataParser.hpp/.cpp # Avatar URL selection & role badge parser
│   └── YouTubeChatProvider.hpp/.cpp # Asynchronous polling loop & reconnect engine
├── overlay/
│   ├── WindowsOverlayWindow.hpp/.cpp # Win32 layered, click-through, topmost window
│   ├── ChatRenderer.hpp/.cpp   # GDI+ antialiased compositor with text shadows
│   └── OverlayController.hpp/.cpp # Coordinator, message queue, & 60 FPS animation ticker
├── ui/
│   └── SettingsDialog.hpp/.cpp # Modern dark-themed control dialog with live preview
├── obs-plugin/
│   ├── obs-module-compat.h     # OBS C ABI definitions & semantic versioning
│   └── plugin-main.cpp         # OBS module entrypoints & Tools menu integration
├── installer/
│   ├── installer_main.cpp      # Standalone one-click installer (YouTubeChatOverlaySetup.exe)
│   └── setup.iss               # Inno Setup 6 compilation script
├── tests/
│   ├── test_overlay_poc.cpp    # Phase 2: Layered click-through PoC test
│   ├── test_youtube_reader.cpp # Phase 3: InnerTube chat streaming reader test
│   ├── test_chat_overlay.cpp   # Phase 4: Live chat to overlay integration test
│   ├── test_preview.cpp        # Phase 7: Live preview & dynamic parameter test
│   ├── test_exclusion_dpi.cpp  # Phase 8: Capture exclusion & multi-DPI/res test
│   └── test_stress.cpp         # Phase 10: Spam flood, security, & stress test
└── docs/
    └── RESEARCH_AND_ARCHITECTURE.md # Detailed technical research report
```

---

## Installation
 
### Method 1: All-in-One Standalone Installer (Recommended for Streamers)
1. Download or share **`LalaLiveChatOverlaySetup.exe`** (Self-contained, ~4.9 MB single executable).
2. Double-click the installer. It displays a sleek modern Dark UI with official branding.
3. Features:
   - **Auto-Detects OBS Studio:** Finds standard and custom OBS installations automatically, with a folder browse dialog to select any custom/portable OBS path.
   - **Checkbox "Pasang Plugin OBS Studio":** Automatically deploys the 64-bit plugin into OBS.
   - **Checkbox "Software Saja Tanpa Plugin OBS":** Installs the standalone app (`LalaLiveChatOverlay.exe`) with optional Desktop & Start Menu shortcuts.
4. Open OBS Studio. Navigate to **Docks -> Lala Live Chat Overlay** or **Tools -> Lala Live Chat Overlay**.
 
### Method 2: Standalone Application
If you prefer not to use OBS plugins, you can directly run **`LalaLiveChatOverlay.exe`**. It runs independently alongside any game and any streaming software.
 
### Method 3: Manual Plugin Installation
Copy `yt-chat-overlay.dll` to your OBS plugins directory:
- **Global:** `C:\Program Files\obs-studio\obs-plugins\64bit\yt-chat-overlay.dll`
- **ProgramData:** `C:\ProgramData\obs-studio\plugins\yt-chat-overlay\bin\64bit\yt-chat-overlay.dll`
- **User:** `%APPDATA%\obs-studio\plugins\yt-chat-overlay\bin\64bit\yt-chat-overlay.dll`
 
---
 
## How to Build from Source
 
### Prerequisites
- Windows 10/11 (64-bit)
- Visual Studio 2022 (MSVC v143 or Build Tools)
- Windows 10/11 SDK (10.0.19041+)
- CMake 3.20+ and Ninja
 
### Build Commands
```cmd
:: 1. Open Visual Studio Developer Command Prompt (x64)
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
 
:: 2. Configure and build with CMake & Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
 
The build produces:
- `build/LalaLiveChatOverlaySetup.exe` (All-in-One Standalone Installer Wizard)
- `build/LalaLiveChatOverlay.exe` (Standalone Desktop Application)
- `build/yt-chat-overlay.dll` (OBS Studio 64-bit Native Plugin)
- `build/test_*.exe` (Independent test executables)

---

## Verification Test Suite

Every phase of development includes an independent verification executable:

| Test Binary | Target Verification | Command |
| :--- | :--- | :--- |
| `test_overlay_poc.exe` | Verifies `WS_EX_TRANSPARENT`, `WS_EX_TOPMOST`, `WS_EX_NOACTIVATE`, click-through, and zero focus theft | `build\test_overlay_poc.exe` |
| `test_youtube_reader.exe` | Verifies InnerTube API discovery, live chat polling, error handling, and avatar parsing | `build\test_youtube_reader.exe [URL]` |
| `test_chat_overlay.exe` | Verifies real-time chat streaming directly onto the desktop transparent overlay | `build\test_chat_overlay.exe [URL]` |
| `test_preview.exe` | Verifies dummy chat injection and real-time live parameter adjustments | `build\test_preview.exe` |
| `test_exclusion_dpi.exe` | Verifies `WDA_EXCLUDEFROMCAPTURE`, 100%/150%/200% DPI, and 1080p/1440p/4K bounds | `build\test_exclusion_dpi.exe` |
| `test_stress.exe` | Verifies input sanitization (XSS/control codes) and 1,000-message concurrent spam flood | `build\test_stress.exe` |

---

## License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.
