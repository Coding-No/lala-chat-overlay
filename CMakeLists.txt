cmake_minimum_required(VERSION 3.20)
project(youtube-chat-overlay VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
)

# Common Core Sources
set(CORE_SOURCES
    overlay/WindowsOverlayWindow.cpp
    overlay/ChatRenderer.cpp
    overlay/OverlayController.cpp
    ui/SettingsDialog.cpp
    ui/DockPanel.cpp
    youtube/HttpClient.cpp
    youtube/ChatDiscovery.cpp
    youtube/ChatParser.cpp
    youtube/MessageParser.cpp
    youtube/UserMetadataParser.cpp
    youtube/YouTubeChatProvider.cpp
)

set(WIN32_LIBS
    user32
    gdi32
    gdiplus
    ole32
    winhttp
    comctl32
    shell32
    comdlg32
    advapi32
)

# 1. OBS Plugin DLL (yt-chat-overlay.dll)
add_library(yt-chat-overlay SHARED
    obs-plugin/plugin-main.cpp
    ${CORE_SOURCES}
    app.rc
)
target_link_libraries(yt-chat-overlay PRIVATE ${WIN32_LIBS})
set_target_properties(yt-chat-overlay PROPERTIES
    OUTPUT_NAME "yt-chat-overlay"
    PREFIX ""
)

# 2. Standalone Application (LalaLiveChatOverlay.exe and YouTubeChatOverlay.exe)
add_executable(LalaLiveChatOverlay WIN32
    main.cpp
    ${CORE_SOURCES}
    app.rc
)
target_link_libraries(LalaLiveChatOverlay PRIVATE ${WIN32_LIBS})

add_executable(YouTubeChatOverlay WIN32
    main.cpp
    ${CORE_SOURCES}
    app.rc
)
target_link_libraries(YouTubeChatOverlay PRIVATE ${WIN32_LIBS})

# 3. One-Click Installer (LalaLiveChatOverlaySetup.exe and YouTubeChatOverlaySetup.exe)
add_executable(LalaLiveChatOverlaySetup WIN32
    installer/installer_main.cpp
    installer/installer.rc
)
target_link_libraries(LalaLiveChatOverlaySetup PRIVATE ${WIN32_LIBS})
add_dependencies(LalaLiveChatOverlaySetup yt-chat-overlay YouTubeChatOverlay LalaLiveChatOverlay)

add_executable(YouTubeChatOverlaySetup WIN32
    installer/installer_main.cpp
    installer/installer.rc
)
target_link_libraries(YouTubeChatOverlaySetup PRIVATE ${WIN32_LIBS})
add_dependencies(YouTubeChatOverlaySetup yt-chat-overlay YouTubeChatOverlay LalaLiveChatOverlay)

# 4. Phase Tests
add_executable(test_overlay_poc tests/test_overlay_poc.cpp overlay/WindowsOverlayWindow.cpp overlay/ChatRenderer.cpp)
target_link_libraries(test_overlay_poc PRIVATE ${WIN32_LIBS})

add_executable(test_youtube_reader tests/test_youtube_reader.cpp 
    youtube/HttpClient.cpp youtube/ChatDiscovery.cpp youtube/ChatParser.cpp 
    youtube/MessageParser.cpp youtube/UserMetadataParser.cpp youtube/YouTubeChatProvider.cpp)
target_link_libraries(test_youtube_reader PRIVATE ${WIN32_LIBS})

add_executable(test_chat_overlay tests/test_chat_overlay.cpp ${CORE_SOURCES})
target_link_libraries(test_chat_overlay PRIVATE ${WIN32_LIBS})

add_executable(test_preview tests/test_preview.cpp ${CORE_SOURCES})
target_link_libraries(test_preview PRIVATE ${WIN32_LIBS})

add_executable(test_exclusion_dpi tests/test_exclusion_dpi.cpp overlay/WindowsOverlayWindow.cpp overlay/ChatRenderer.cpp)
target_link_libraries(test_exclusion_dpi PRIVATE ${WIN32_LIBS})

add_executable(test_stress tests/test_stress.cpp ${CORE_SOURCES})
target_link_libraries(test_stress PRIVATE ${WIN32_LIBS})

add_executable(test_settings_dialog tests/test_settings_dialog.cpp ${CORE_SOURCES})
target_link_libraries(test_settings_dialog PRIVATE ${WIN32_LIBS})

