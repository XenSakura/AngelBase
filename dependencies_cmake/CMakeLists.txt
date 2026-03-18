find_package(Vulkan REQUIRED)

# The package grabber
include(FetchContent)

# Get glm
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0af55ccecd98d4e5a8d1fad7de25ba429d60e863 #refs/tags/1.0.1
)
FetchContent_MakeAvailable(glm)
target_link_libraries(AngelBase PRIVATE glm::glm)

# Get glfw
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579
)
FetchContent_MakeAvailable(glfw)
target_link_libraries(AngelBase PRIVATE glfw)

# Get VulkanMemoryAllocator
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(vma)
target_link_libraries(AngelBase PRIVATE GPUOpen::VulkanMemoryAllocator)

# Get imgui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.5
)
FetchContent_MakeAvailable(imgui)
# ImGui doesn't provide CMake targets, so we create our own
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)
target_link_libraries(AngelBase PRIVATE imgui)

# Get assimp
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)
target_link_libraries(AngelBase PRIVATE assimp::assimp)

# Get simdjson
FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.11.4
)
FetchContent_MakeAvailable(simdjson)
target_link_libraries(AngelBase PUBLIC simdjson)

if(WIN32)
    set(SLANG_PLATFORM "windows-x86_64")
    set(SLANG_EXECUTABLE "slangc.exe")
    set(SLANG_LIB "slang.dll")
elseif(APPLE)
    set(SLANG_PLATFORM "macosx-86_64")
    set(SLANG_EXECUTABLE "slangc")
    set(SLANG_LIB "libslang.dylib")
else()
    set(SLANG_PLATFORM "linux-x86_64")
    set(SLANG_EXECUTABLE "slangc")
    set(SLANG_LIB "libslang.so")
endif()

set(SLANG_VERSION "2025.24.3")

# Fetch precompiled Slang release
FetchContent_Declare(
    slang
    URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_PLATFORM}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(slang)

add_library(slang-lib SHARED IMPORTED GLOBAL)
set_target_properties(slang-lib PROPERTIES
    IMPORTED_LOCATION "${slang_SOURCE_DIR}/bin/slang.dll"
    IMPORTED_IMPLIB "${slang_SOURCE_DIR}/lib/slang.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${slang_SOURCE_DIR}/include"
)

add_library(slang-compiler SHARED IMPORTED GLOBAL)
set_target_properties(slang-compiler PROPERTIES
    IMPORTED_LOCATION "${slang_SOURCE_DIR}/bin/slang-compiler.dll"
    IMPORTED_IMPLIB "${slang_SOURCE_DIR}/lib/slang-compiler.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${slang_SOURCE_DIR}/include"
)

set(SLANGC_EXECUTABLE "${slang_SOURCE_DIR}/bin/${SLANG_EXECUTABLE}" CACHE FILEPATH "Path to slangc")

# Link both libraries
target_link_libraries(AngelBase PRIVATE slang-lib slang-compiler)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(stb)

target_include_directories(AngelBase PRIVATE ${stb_SOURCE_DIR})
