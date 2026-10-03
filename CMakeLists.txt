cmake_minimum_required(VERSION 3.20)
project(youtube-chat-overlay VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static link CRT so users don't need MSVC Redistributable (vcruntime140.dll etc.)
if(MSVC)
    add_compile_options(/utf-8)

    # Modern CMake >= 3.15: use CMAKE_MSVC_RUNTIME_LIBRARY
    if(POLICY CMP0091)
        cmake_policy(SET CMP0091 NEW)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        # Fallback: replace /MD with /MT in compiler flags
        foreach(flag_var
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
            if(${flag_var} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif()
        endforeach()
    endif()
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
    urlmon
    wininet
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

# 2. Standalone Application (LalaLiveChatOverlay.exe)
add_executable(LalaLiveChatOverlay WIN32
    main.cpp
    ${CORE_SOURCES}
    app.rc
)
target_link_libraries(LalaLiveChatOverlay PRIVATE ${WIN32_LIBS})

# 3. One-Click Installer (LalaLiveChatOverlaySetup.exe)
add_executable(LalaLiveChatOverlaySetup WIN32
    installer/installer_main.cpp
    installer/installer.rc
)
target_link_libraries(LalaLiveChatOverlaySetup PRIVATE ${WIN32_LIBS})
add_dependencies(LalaLiveChatOverlaySetup yt-chat-overlay LalaLiveChatOverlay)

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

