cmake_minimum_required(VERSION 3.15)
project(quadtree_viewer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Options ----
option(IMGUI_WITH_DEMO "Build with Dear ImGui demo window" OFF)

# ---- Paths ----
set(SRC_DIR         ${CMAKE_SOURCE_DIR}/src)
set(IMGUI_DIR       ${CMAKE_SOURCE_DIR}/include/imgui)
set(IMGUI_BACKENDS  ${IMGUI_DIR}/backends)
# You are including stb as: #include "stb/stb_image.h"
# so we add the parent folder "include" to includes:
set(STB_ROOT        ${CMAKE_SOURCE_DIR}/include)

# ---- Sources ----
set(IMGUI_SOURCES
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_BACKENDS}/imgui_impl_glfw.cpp
  ${IMGUI_BACKENDS}/imgui_impl_opengl2.cpp
)
if(IMGUI_WITH_DEMO)
  list(APPEND IMGUI_SOURCES ${IMGUI_DIR}/imgui_demo.cpp)
endif()

add_executable(quadtree_viewer
  ${SRC_DIR}/main.cpp
  ${IMGUI_SOURCES}
)

target_sources(quadtree_viewer PRIVATE include/ImGuiFileDialog/ImGuiFileDialog.cpp)

target_include_directories(quadtree_viewer PRIVATE
  ${IMGUI_DIR}
  ${IMGUI_BACKENDS}
  ${STB_ROOT}         # gives access to "stb/stb_image.h"
  include/ImGuiFileDialog
)

# Put binaries in build/bin/
set_target_properties(quadtree_viewer PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ---- OpenGL ----
find_package(OpenGL REQUIRED)
# On Apple, OpenGL::GL maps to the framework automatically
target_link_libraries(quadtree_viewer PRIVATE OpenGL::GL)

# ---- GLFW (per-OS) ----
if(APPLE)
  # brew install glfw
  find_package(glfw3 REQUIRED)
  target_link_libraries(quadtree_viewer PRIVATE glfw)
  # ImGui GLFW backend on macOS needs these frameworks:
  target_link_libraries(quadtree_viewer PRIVATE
    "-framework Cocoa" "-framework IOKit" "-framework CoreVideo"
  )

elseif(WIN32)
  # Recommended: vcpkg install glfw3
  #   cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
  find_package(glfw3 CONFIG REQUIRED) # falls back to non-CONFIG if provided by user
  # If CONFIG isn’t found, try the non-Config package
  if(NOT TARGET glfw)
    find_package(glfw3 REQUIRED)
    target_link_libraries(quadtree_viewer PRIVATE glfw)
  else()
    target_link_libraries(quadtree_viewer PRIVATE glfw)
  endif()
  target_link_libraries(quadtree_viewer PRIVATE opengl32 gdi32 dwmapi user32 shell32 ole32 uuid)

else() # Linux / BSD
  # Prefer pkg-config to get correct transitive deps (X11/Wayland)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(GLFW3 REQUIRED glfw3)
    target_include_directories(quadtree_viewer PRIVATE ${GLFW3_INCLUDE_DIRS})
    target_link_libraries(quadtree_viewer PRIVATE ${GLFW3_LINK_LIBRARIES})
  else()
    # Fallback to CMake package
    find_package(glfw3 REQUIRED)
    target_link_libraries(quadtree_viewer PRIVATE glfw)
  endif()
  # Do NOT hardcode X11 libs; pkg-config/glfw package will supply what’s needed.
endif()
