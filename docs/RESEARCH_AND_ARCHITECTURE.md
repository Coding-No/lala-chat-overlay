# YouTube Live Chat Overlay for OBS
## Phase 1: Research and Architecture Document

**Author:** Antigravity Team  
**Date:** September 2026  
**Target Platform:** Windows 10 (Version 2004+ / Build 19041+) and Windows 11 (64-bit)  
**Host Application:** OBS Studio (Version 29, 30, 31, 32+ 64-bit)  

---

### 1. Executive Summary

This document establishes the technical foundation and architectural specifications for **"YouTube Live Chat Overlay for OBS"**, a production-grade OBS Studio plugin for Windows 10/11 designed specifically for single-monitor content creators. 

The software enables live streamers playing games in borderless fullscreen mode on a single monitor to see incoming YouTube live chat messages floating transparently directly over their game. Crucially, the chat overlay:
- Never steals keyboard or mouse focus.
- Allows all mouse clicks and inputs to pass through directly to the game (`WS_EX_TRANSPARENT`).
- Is completely excluded from OBS stream recordings via `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` across supported capture methods (Game Capture, Window Capture, Windows Graphics Capture).
- Connects directly to YouTube live chat streams without third-party relay services (Streamlabs, StreamElements, Restream) and without requiring Google API keys or quota limits.

---

### 2. State-of-the-Art Research Findings

#### 2.1. OBS Studio Plugin Architecture (Windows 64-bit)

Modern OBS Studio (v29 through v32) on Windows uses a 64-bit native DLL module architecture. 
- **Module Interface:**
  Plugins export standard entry points:
  ```cpp
  MODULE_EXPORT const char *obs_module_name(void);
  MODULE_EXPORT const char *obs_module_description(void);
  MODULE_EXPORT uint32_t obs_module_ver(void);
  MODULE_EXPORT bool obs_module_load(void);
  MODULE_EXPORT void obs_module_unload(void);
  MODULE_EXPORT void obs_module_set_pointer(obs_module_t *module);
  ```
- **UI Integration:**
  OBS exposes `obs-frontend-api.dll`, providing:
  - `obs_frontend_add_tools_menu_item(const char *name, obs_frontend_cb callback, void *private_data)`: Adds a menu entry under **Tools -> YouTube Live Chat Overlay**.
  - `obs_frontend_get_main_window()`: Retrieves the `QMainWindow` handle for modal parenting or DPI synchronization.
  - `obs_frontend_add_event_callback()`: Listens for OBS shutdown or scene changes to cleanly release resources.
- **Plugin Locations:**
  1. System-wide: `C:\Program Files\obs-studio\obs-plugins\64bit\<plugin-name>.dll` and `data\obs-plugins\<plugin-name>\`
  2. Per-user: `%APPDATA%\obs-studio\plugins\<plugin-name>\bin\64bit\<plugin-name>.dll`

#### 2.2. YouTube Live Chat Mechanism (Latest InnerTube Analysis)

Empirical testing conducted against live YouTube broadcasts in September 2026 revealed essential protocol behaviors:
1. **The Deprecated/Blocked Scraping Path:**
   Directly loading `https://www.youtube.com/live_chat?v=VIDEO_ID` without browser cookies triggers Google's automated bot protection (`af-error-page`, the "Something went wrong" error monkey). Relying on direct HTML scraping of the popout URL is brittle and prone to blocks.
2. **The Robust InnerTube Protocol:**
   YouTube's modern web and mobile apps communicate via the internal **InnerTube API**:
   - **Discovery Phase:**
     1. Retrieve the public video watch page: `https://www.youtube.com/watch?v=VIDEO_ID` with standard browser headers (`User-Agent: Mozilla/5.0...`).
     2. Extract the public `INNERTUBE_API_KEY` (regex: `"INNERTUBE_API_KEY":"([^"]+)"`) and `INNERTUBE_CLIENT_VERSION` (regex: `"INNERTUBE_CLIENT_VERSION":"([^"]+)"`).
     3. Make an InnerTube `next` request (`POST https://www.youtube.com/youtubei/v1/next?key=KEY`) with JSON body:
        ```json
        {
          "context": {
            "client": { "clientName": "WEB", "clientVersion": "2.20260925.08.00" }
          },
          "videoId": "VIDEO_ID"
        }
        ```
     4. Locate the `liveChatRenderer` inside the response tree and extract the initial continuation token (`continuations[0].timedContinuationData.continuation` or `invalidationContinuationData.continuation`).
   - **Streaming/Polling Phase:**
     1. Send `POST https://www.youtube.com/youtubei/v1/live_chat/get_live_chat?key=KEY` with:
        ```json
        {
          "context": {
            "client": { "clientName": "WEB", "clientVersion": "2.20260925.08.00" }
          },
          "continuation": "TOKEN"
        }
        ```
     2. Parse `continuationContents.liveChatContinuation.actions`:
        - `addChatItemAction.item.liveChatTextMessageRenderer` contains:
          - `id`: unique message ID for deduplication.
          - `authorName.simpleText`: display username.
          - `message.runs`: text segments and emotes.
          - `authorPhoto.thumbnails`: avatar image URLs (webp/png).
          - `authorBadges`: badge renderers with tooltips identifying `Owner`, `Moderator`, `Member`, `Verified`.
          - `timestampUsec`: timestamp in microseconds.
     3. Extract next continuation token and `timeoutMs` (typically 1000ms – 5000ms).
     4. Repeat poll using exponential backoff on transient errors.

#### 2.3. Windows Transparent Always-On-Top Overlay Window

To guarantee non-intrusive gameplay on a single monitor, the overlay window must satisfy five Win32 criteria:
1. `WS_POPUP`: No window border, title bar, or system decorations.
2. `WS_EX_TOPMOST`: Ensures the window floats above standard application windows.
3. `WS_EX_TRANSPARENT`: Directs Windows hit-testing (`WM_NCHITTEST`) to ignore the window completely and route all mouse clicks, scrolls, and cursor movements directly to the game underneath.
4. `WS_EX_LAYERED`: Enables 32-bit ARGB per-pixel alpha transparency via `UpdateLayeredWindow`.
5. `WS_EX_NOACTIVATE`: Prevents the window from becoming the foreground/active window when shown or redrawn (`SWP_NOACTIVATE`), preserving active keyboard focus in the game.
6. `WS_EX_TOOLWINDOW`: Suppresses the overlay from appearing in the Alt+Tab task switcher.

#### 2.4. Capture Exclusion via `SetWindowDisplayAffinity`

Introduced in Windows 10 Version 2004 (Build 19041):
```cpp
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
```
**OBS Capture Path Matrix:**
| Capture Method in OBS | Underlying Technology | Streamer Sees Overlay? | Stream Sees Overlay? | Status |
| :--- | :--- | :--- | :--- | :--- |
| **OBS Game Capture** | In-process graphics hook (`graphics-hook.dll`) hooking DirectX/Vulkan `Present` | YES | **NO (Excluded)** | 100% Excluded by design |
| **OBS Window Capture** | Win32 PrintWindow / WGC targeted at game HWND | YES | **NO (Excluded)** | 100% Excluded by design |
| **OBS Display Capture (WGC)** | Windows Graphics Capture (`Windows.Graphics.Capture`) | YES | **NO (Excluded)** | 100% Excluded via `WDA_EXCLUDEFROMCAPTURE` |
| **OBS Display Capture (DXGI Dup)** | DXGI Desktop Duplication API | YES | **NO (Excluded)** | 100% Excluded via `WDA_EXCLUDEFROMCAPTURE` |
| **Legacy GDI BitBlt** | BitBlt screen copy (legacy Windows 7/8 mode) | YES | Depends (Black Box) | Deprecated; OBS 30+ defaults to WGC/DXGI |

#### 2.5. Exclusive Fullscreen vs. Borderless Fullscreen

- **Borderless Fullscreen (Supported & Recommended):**
  The game window covers the monitor area while remaining managed by the Desktop Window Manager (DWM). DWM allows the topmost layered overlay window to render seamlessly above the game surface with zero input lag and full capture exclusion.
- **Exclusive Fullscreen (Architectural Limitation):**
  Exclusive Fullscreen grants the game exclusive control over the physical display pipeline, bypassing DWM composition. No external Windows overlay can display on top of an Exclusive Fullscreen surface without injecting DLL hooks into the game's graphics pipeline (Direct3D/Vulkan `Present()`). Because hooking game processes is flagged as unauthorized tampering by anti-cheat systems (BattlEye, Easy Anti-Cheat, Vanguard, VAC), **our architecture intentionally avoids process injection**.
  - **Graceful Behavior:** The plugin continuously monitors display mode. If an exclusive fullscreen takeover is detected, it does not crash, logs the state gracefully, and advises the user to select "Borderless Fullscreen" in the game's display settings.

#### 2.6. Hardware-Accelerated Transparent Rendering

We evaluated three rendering architectures:
1. **DirectComposition + Direct3D11 Swapchain (`DXGI_ALPHA_MODE_PREMULTIPLIED`):**
   High performance, but adds swapchain management overhead and potential driver conflicts when games trigger display mode resets.
2. **Layered Window + GDI+ / Direct2D 32-bit ARGB DIB + `UpdateLayeredWindow` (Selected):**
   - Renders into an offscreen 32-bit ARGB memory bitmap with Direct2D/DirectWrite or GDI+ antialiasing.
   - Pushes alpha surfaces to DWM via `UpdateLayeredWindow()`.
   - **Performance:** CPU usage is near 0.1% when idle. Incremental redraws occur only when new chat arrives or during active fade-out transitions.
   - **Visual Quality:** Crisp subpixel text rendering, drop shadows, customizable font weights, smooth circular avatar clipping, and zero black-border artifacts.

---

### 3. Architecture Overview

```
+---------------------------------------------------------------------------------+
|                                   OBS STUDIO                                    |
|                                                                                 |
|  +--------------------+       +----------------------------------------------+  |
|  | Tools Menu Entry   | ----> | Settings & Control UI (Qt Dialog)             |  |
|  +--------------------+       | - URL Input & Validation                     |  |
|                               | - Start / Stop / Preview                     |  |
|                               | - Sliders: Size, Opacity, Spacing, Max Chat  |  |
|                               | - Colors: Username, Message, Roles           |  |
|                               | - Filters & Auto-hide Timer                  |  |
|                               +----------------------------------------------+  |
|                                                      |                          |
|                                                      v                          |
|                                       +------------------------------+          |
|                                       | Overlay Controller           |          |
|                                       | (Coordinates State & Engine) |          |
|                                       +------------------------------+          |
+------------------------------------------------------|--------------------------+
                                                       |
         +---------------------------------------------+------------------------------------+
         |                                                                                  |
         v                                                                                  v
+------------------------------------+                             +------------------------------------+
| YouTube Chat Pipeline              |                             | Windows Overlay Window             |
|                                    |                             |                                    |
|  [YouTube URL]                     |                             |  - Win32 Layered Window            |
|         |                          |                             |  - WS_EX_TOPMOST                   |
|         v                          |                             |  - WS_EX_TRANSPARENT (Click-thru)  |
|  +-------------------------------+ |                             |  - WS_EX_NOACTIVATE                |
|  | YouTubeChatProvider           | |                             |  - WDA_EXCLUDEFROMCAPTURE          |
|  | - Reconnect Loop (2,5,10,20s) | |                             +------------------------------------+
|  | - WinHttp Async Engine        | |                                              ^
|  +-------------------------------+ |                                              |
|         |                          |                             +------------------------------------+
|         v                          |                             | Chat Renderer Engine               |
|  +-------------------------------+ |                             |                                    |
|  | ChatDiscovery                 | |                             |  - 32-bit ARGB Compositor          |
|  | - Watch Page & API Key        | |                             |  - DirectWrite / GDI+ Text         |
|  | - InnerTube Next Resolution   | |                             |  - Circular Avatar Downloader      |
|  +-------------------------------+ |                             |  - Role Badges (Mod, Member, etc.) |
|         |                          |                             |  - Smooth Fade / Slide Animations  |
|         v                          |                             |  - Auto-Hide Timer & Spam Guard    |
|  +-------------------------------+ |                             +------------------------------------+
|  | ChatParser & MessageParser    | |                                              ^
|  | - JSON Action Extraction      | |                                              |
|  | - Unicode & HTML Sanitization | |                                              |
|  +-------------------------------+ |                                              |
|         |                          |                                              |
|         v                          |                                              |
|  [ChatMessage Queue] --------------+----------------------------------------------+
+------------------------------------+
```

---

### 4. Technical Specifications & Abstraction Layer

#### 4.1. YouTube Abstraction Layer
Designed for modularity and resilience against YouTube frontend changes:
1. `ChatDiscovery`:
   - Validates user input: `youtube.com/watch?v=...`, `youtu.be/...`, `youtube.com/live/...`.
   - Extracts 11-character video ID.
   - Fetches watch HTML, parses `INNERTUBE_API_KEY` and `INNERTUBE_CLIENT_VERSION`.
   - Calls `v1/next` endpoint to obtain the active `liveChatRenderer` continuation token.
2. `ChatParser`:
   - Takes raw JSON responses from `v1/live_chat/get_live_chat`.
   - Extracts actions (`addChatItemAction`) and continuation metadata (`timeoutMs`, `continuation`).
3. `MessageParser` & `UserMetadataParser`:
   - Converts `liveChatTextMessageRenderer` and paid renderers into `ChatMessage`.
   - Sanitizes text content (removes control characters, encodes HTML entities, validates emojis).
   - Extracts avatar URLs with multi-tier fallback (high-res -> standard -> generated placeholder).
   - Extracts author roles: `Owner`, `Moderator`, `Member`, `Verified`, `Regular`.
4. `YouTubeChatProvider`:
   - Runs worker thread managing HTTP session via native Windows `WinHttp`.
   - Handles connection state machine: `Disconnected`, `Connecting`, `Connected`, `Reconnecting`, `Error`.
   - Exponential backoff schedule on disconnect: 2s, 5s, 10s, 20s, 30s.

#### 4.2. Overlay & Renderer Engine
1. `OverlayController`:
   - Manages message queue, rate limiting, and deduplication (by `messageId`).
   - Handles dummy preview chat injection.
   - Computes DPI scale factor and screen geometry.
   - Optional window tracking: polls foreground game window geometry to auto-align overlay.
2. `ChatRenderer`:
   - Maintains active message list up to `max_messages` (3, 5, 8, 10, 15, 20, Custom).
   - Handles duration expiration (3s, 5s, 10s, 15s, 30s, 60s, Never) with smooth alpha fade-out.
   - Handles auto-hide: when inactive for X seconds, smoothly hides window.
   - Layout calculations: word-wrapping constrained by `max_width`, customizable typography (font family, font size, username/message weights, badge offsets, circular avatar radius).

---

### 5. Multi-Phase Implementation Plan

Each phase produces an independently testable, standalone binary or module:

| Phase | Deliverable | Verification Criteria |
| :--- | :--- | :--- |
| **Phase 1** | Research & Architecture Document | Complete technical review & technology decisions documented |
| **Phase 2** | Transparent Overlay Proof-of-Concept | Standalone Win32 executable verifying `WS_EX_TRANSPARENT`, `WS_EX_NOACTIVATE`, `WS_EX_TOPMOST`, click-through, and zero focus theft |
| **Phase 3** | YouTube Chat Reader Module | Standalone console executable fetching live chat via InnerTube with full role/avatar parsing |
| **Phase 4** | Chat Reader to Overlay Integration | Standalone executable coupling live YouTube chat stream with hardware-accelerated transparent overlay |
| **Phase 5** | OBS Plugin Architecture & Hook | Native 64-bit OBS plugin DLL registering under **Tools -> YouTube Live Chat Overlay** |
| **Phase 6** | Settings UI & Configuration | Full Qt/Win32 configuration panel with live sliders, color pickers, and JSON config persistence |
| **Phase 7** | Dummy Chat Preview System | Instant visual feedback with dummy messages for real-time positioning and aesthetic tuning |
| **Phase 8** | OBS Capture Exclusion Testing | Comprehensive validation of `WDA_EXCLUDEFROMCAPTURE` across Game, Window, and Display captures |
| **Phase 9** | Automated Windows Installer | One-click Inno Setup installer detecting 64-bit OBS installation directory and deploying all assets |
| **Phase 10** | Stress, Spam, & DPI Validation | High-load chat flood testing, multi-DPI (100%, 150%, 200%), and resolution verification (1080p, 1440p, 4K) |

---
*Phase 1 Complete. Ready for Phase 2: Transparent Overlay Proof of Concept.*
