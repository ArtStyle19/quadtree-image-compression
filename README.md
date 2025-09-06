# 🌀 Quadtree Image Compression

---

## Developed by

## Jorge Guillermo Olarte Quispe

---

## Universidad Nacional del Altiplano – Ingeniería de Sistemas

---

A fast and interactive C++ / OpenGL tool to explore image compression with quadtrees.
The algorithm adaptively divides the image: big squares in uniform areas, tiny squares where details matter.
You can adjust parameters live, zoom and pan the canvas, customize colors, and export your compressed image as a PNG.

## Screenshots

| Quadtree Grid                       | Grid (Tinted)                             | Compressed Images                        |
| ----------------------------------- | ----------------------------------------- | ---------------------------------------- |
| ![](readme-images/parrot_lines.png) | ![](readme-images/parrot_lines_color.png) | ![](readme-images/parrot_compressed.png) |
| ![](readme-images/ltp_lines.png)    | ![](readme-images/ltp_lines_color.png)    | ![](readme-images/ltp_compressed.png)    |
| ![](readme-images/cube_lines.png)   | ![](readme-images/cube_lines_color.png)   | ![](readme-images/cube_compressed.png)   |
| ![](readme-images/unap_lines.png)   | ![](readme-images/unap_lines_color.png)   | ![](readme-images/unap_compressed.png)   |

> Screenshot UI:  
> ![](readme-images/screenshot_doge.png)

## Features

- Quadtree segmentation of images
- Adjustable leaf size and standard deviation threshold
- Zoom & pan the canvas
- Customizable grid color, line width, and background color
- Load images via drag-and-drop or file browser
- Save compressed images to PNG

## Build & Run

### Requirements

- C++17 compiler
- CMake ≥ 3.15
- OpenGL (fixed-function is fine)
- GLFW3
- (Linux) X11 + pthread + dl
- Vendored: Dear ImGui, stb, ImGuiFileDialog (already in `include/`)

### Linux / macOS

## Using cmake

```bash
git clone https://github.com/ArtStyle19/quadtree-image-compression.git
cd quadtree-image-compression

rm -rf build
cmake -S . -B build
cmake --build build -j

# run
./build/bin/quadtree_viewer images/image.png
```

## Using g++

```bash
g++ -std=c++17 src/main3.cpp \
  include/imgui/imgui.cpp include/imgui/imgui_draw.cpp include/imgui/imgui_widgets.cpp include/imgui/imgui_tables.cpp \
  include/imgui/backends/imgui_impl_glfw.cpp include/imgui/backends/imgui_impl_opengl2.cpp \
  -Iinclude -Iinclude/imgui -Iinclude/imgui/backends \
  -lglfw -lGL -ldl -lpthread -lX11 -lXrandr -lXi -lXxf86vm -lXcursor \
  -o quadtree_viewer


./quadtree_viewer
```

## Controls

- **Mouse wheel**: zoom in/out (anchored at cursor)
- **Drag (LMB/RMB)**: pan
- **Double-click (LMB)**: reset view
- **GUI (left panel)**:
  - _Leaf power_ (`2^k`) and _StdDev power_ (`2^k`)
  - Toggle **Fill** / **Grid**
  - **Grid color** picker + **line width**
  - **Canvas background** color picker
  - **Browse…** to load image; **Save PNG** to export
