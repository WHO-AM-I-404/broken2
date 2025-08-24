cmake_minimum_required(VERSION 3.15)
project(GlitchSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files
set(SOURCES
    src/main.cpp
    src/VisualEffects.cpp
    src/DestructiveActions.cpp
    src/Utilities.cpp
    src/AdvancedEffects.cpp
)

# Include directories
include_directories(src)

# Create executable
add_executable(GlitchSimulator ${SOURCES})

# Link libraries
target_link_libraries(GlitchSimulator
    WindowsApp
    crypt32
    virtdisk
    setupapi
    winmm
    user32
    dwmapi
    gdi32
    ntdll
    shell32
    advapi32
    iphlpapi
    ws2_32
    winhttp
    wininet
    cryptui
    gdiplus
    urlmon
    shlwapi
)

# Windows specific settings
if(WIN32)
    set_target_properties(GlitchSimulator PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()cmake_minimum_required(VERSION 3.15)
project(GlitchSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files
set(SOURCES
    src/main.cpp
    src/VisualEffects.cpp
    src/DestructiveActions.cpp
    src/Utilities.cpp
    src/AdvancedEffects.cpp
)

# Include directories
include_directories(src)

# Create executable
add_executable(GlitchSimulator ${SOURCES})

# Link libraries
target_link_libraries(GlitchSimulator
    WindowsApp
    crypt32
    virtdisk
    setupapi
    winmm
    user32
    dwmapi
    gdi32
    ntdll
    shell32
    advapi32
    iphlpapi
    ws2_32
    winhttp
    wininet
    cryptui
    gdiplus
    urlmon
    shlwapi
)

# Windows specific settings
if(WIN32)
    set_target_properties(GlitchSimulator PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()
