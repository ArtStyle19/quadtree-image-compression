// main.cpp
// Quadtree + OpenGL + Dear ImGui (GLFW + OpenGL2 backend)
// Build with CMakeLists.txt below. Drag & drop an image file into the window to load it.

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <filesystem>   // C++17
#include <cmath>

// ---------------- ImGui ----------------
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl2.h"


// ---------------- Image buffer ----------------
struct Color { uint8_t r,g,b; };
static int IMG_W = 0, IMG_H = 0;
static std::vector<std::vector<Color>> image;

// NDC helpers (render image in [-1,1]x[-1,1] or fit-to-window)
static inline float ndcX(float x, float canvasW) { return (x / canvasW) * 2.0f - 1.0f; }
static inline float ndcY(float y, float canvasH) { return 1.0f - (y / canvasH) * 2.0f; } // flip Y

// ---------------- Quadtree ----------------
struct Node {
    int x, y, w, h;
    bool leaf = false;
    Color avg{};
    Node* ch[4]{nullptr,nullptr,nullptr,nullptr};
};

static inline double clamp0(double v){ return v < 0 ? 0 : v; }

static double calcStdDevRGB(const std::vector<std::vector<Color>>& px, int x, int y, int w, int h) {
    double sumR=0,sumG=0,sumB=0, sqR=0,sqG=0,sqB=0;
    const int cnt = w*h;
    for (int j=y; j<y+h; ++j) {
        const auto& row = px[j];
        for (int i=x; i<x+w; ++i) {
            const double R=row[i].r, G=row[i].g, B=row[i].b;
            sumR += R; sumG += G; sumB += B;
            sqR  += R*R; sqG += G*G; sqB += B*B;
        }
    }
    const double mR=sumR/cnt, mG=sumG/cnt, mB=sumB/cnt;
    const double sdR = std::sqrt(clamp0(sqR/cnt - mR*mR));
    const double sdG = std::sqrt(clamp0(sqG/cnt - mG*mG));
    const double sdB = std::sqrt(clamp0(sqB/cnt - mB*mB));
    return (sdR + sdG + sdB) / 3.0;
}

static Color averageRGB(const std::vector<std::vector<Color>>& px, int x, int y, int w, int h) {
    uint64_t sumR=0, sumG=0, sumB=0;
    const int cnt = w*h;
    for (int j=y; j<y+h; ++j) {
        const auto& row = px[j];
        for (int i=x; i<x+w; ++i) {
            sumR += row[i].r;
            sumG += row[i].g;
            sumB += row[i].b;
        }
    }
    Color c;
    c.r = (uint8_t)(sumR / cnt);
    c.g = (uint8_t)(sumG / cnt);
    c.b = (uint8_t)(sumB / cnt);
    return c;
}

struct BuildStats { size_t nodes=0, leaves=0; double ms=0; };

static Node* buildQT(const std::vector<std::vector<Color>>& px,
                     int x, int y, int w, int h,
                     int minLeaf, double sdThresh,
                     BuildStats& stats)
{
    Node* n = new Node();
    n->x=x; n->y=y; n->w=w; n->h=h;
    stats.nodes++;

    if (w <= minLeaf || h <= minLeaf || calcStdDevRGB(px,x,y,w,h) <= sdThresh) {
        n->leaf = true;
        n->avg = averageRGB(px,x,y,w,h);
        stats.leaves++;
        return n;
    }

    const int w2 = w/2, h2 = h/2;
    if (w2==0 || h2==0) {
        n->leaf = true;
        n->avg = averageRGB(px,x,y,w,h);
        stats.leaves++;
        return n;
    }

    n->ch[0] = buildQT(px, x,        y,        w2,       h2,       minLeaf, sdThresh, stats); // NW
    n->ch[1] = buildQT(px, x + w2,   y,        w - w2,   h2,       minLeaf, sdThresh, stats); // NE
    n->ch[2] = buildQT(px, x,        y + h2,   w2,       h - h2,   minLeaf, sdThresh, stats); // SW
    n->ch[3] = buildQT(px, x + w2,   y + h2,   w - w2,   h - h2,   minLeaf, sdThresh, stats); // SE
    return n;
}

static void destroy(Node* n) {
    if (!n) return;
    if (!n->leaf) for (int i=0;i<4;++i) destroy(n->ch[i]);
    delete n;
}

// ---------------- Rendering ----------------
static bool gDrawFill  = true;
static bool gDrawLines = true;
static bool gFitToWindow = false;

static void drawLeafRect(const Node* n, float canvasW, float canvasH) {
    const float x0 = ndcX((float)n->x,            canvasW);
    const float y0 = ndcY((float)n->y,            canvasH);
    const float x1 = ndcX((float)(n->x + n->w),   canvasW);
    const float y1 = ndcY((float)(n->y + n->h),   canvasH);
    const float r = n->avg.r/255.f, g = n->avg.g/255.f, b = n->avg.b/255.f;

    if (gDrawFill) {
        glColor3f(r,g,b);
        glBegin(GL_TRIANGLES);
        glVertex2f(x0,y0); glVertex2f(x1,y0); glVertex2f(x1,y1);
        glVertex2f(x0,y0); glVertex2f(x1,y1); glVertex2f(x0,y1);
        glEnd();
    }

    if (gDrawLines) {
// background: var(--violet900, rgba(65, 70, 126, 1));
// 
        // glColor3f(52/255.f, 55/255.f, 99/255.f);
        // glColor3f(72/255.f, 75/255.f, 119/255.f);
        glColor3f(102/255.f, 105/255.f, 149/255.f);





        glBegin(GL_LINE_LOOP);
        glVertex2f(x0,y0); glVertex2f(x1,y0); glVertex2f(x1,y1); glVertex2f(x0,y1);
        glEnd();
    }
}

static void renderQT(const Node* n, float canvasW, float canvasH) {
    if (!n) return;
    if (n->leaf) { drawLeafRect(n, canvasW, canvasH); return; }
    for (int i=0;i<4;++i) renderQT(n->ch[i], canvasW, canvasH);
}

// ---------------- Image IO ----------------
static bool loadImage(const std::string& path) {
    int w,h,ch;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &ch, 3); // force RGB
    if (!data) {
        std::cerr << "Failed to load image: " << path << "\n";
        return false;
    }
    IMG_W=w; IMG_H=h;
    image.assign(IMG_H, std::vector<Color>(IMG_W));
    for (int j=0; j<IMG_H; ++j) {
        for (int i=0; i<IMG_W; ++i) {
            const int idx = (j*IMG_W + i)*3;
            image[j][i] = { data[idx+0], data[idx+1], data[idx+2] };
        }
    }
    stbi_image_free(data);
    std::cout << "Loaded: " << path << " ("<<IMG_W<<"x"<<IMG_H<<")\n";
    return true;
}

// Global current image path (for drag & drop)
static std::string gCurrentImagePath = "";
static std::string gPendingImagePath = "";   

static void dropCallback(GLFWwindow*, int count, const char** paths) {
    if (count > 0 && paths && paths[0]) {
        gPendingImagePath = paths[0];
    }
}


// -------- File size helper --------
static uintmax_t getFileSize(const std::string& path) {
    try {
        if (!path.empty() && std::filesystem::exists(path)) {
            return std::filesystem::file_size(path);
        }
    } catch (...) {}
    return 0;
}

// -------- Estimate quadtree data size (uncompressed) --------
// Very rough: one leaf = avg RGB (3 bytes) + small header (e.g., 8 bytes for x,y,w,h if you stored it).
// Tweak overhead if you want a different estimate.
static size_t estimateQuadtreeBytes(size_t leaves, bool includeRects = true) {
    const size_t rgbBytes = 3;
    const size_t rectBytes = includeRects ? 4 * sizeof(int) : 0; // x,y,w,h
    return leaves * (rgbBytes + rectBytes);
}


// -------- Rasterize quadtree to buffer --------
static void blitRect(std::vector<Color>& buf, int W, int H, int x, int y, int w, int h, Color c) {
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(W, x + w), y1 = std::min(H, y + h);
    for (int j = y0; j < y1; ++j) {
        Color* row = &buf[j * W];
        for (int i = x0; i < x1; ++i) row[i] = c;
    }
}

static void rasterizeQT(const Node* n, int W, int H, std::vector<Color>& out) {
    if (!n) return;
    if (n->leaf) { blitRect(out, W, H, n->x, n->y, n->w, n->h, n->avg); return; }
    for (int i = 0; i < 4; ++i) rasterizeQT(n->ch[i], W, H, out);
}

// -------- Save current quadtree view as PNG --------
static bool saveQuadtreePNG(const std::string& path, const Node* root, int W, int H) {
    if (!root || W <= 0 || H <= 0) return false;

    std::vector<Color> buf(W * H);
    rasterizeQT(root, W, H, buf);

    // stbi_write_png expects rows as contiguous bytes
    const int stride = W * 3;
    // Convert to raw bytes view without copying:
    unsigned char* data = reinterpret_cast<unsigned char*>(buf.data());
    int ok = stbi_write_png(path.c_str(), W, H, 3, data, stride);
    return ok != 0;
}

// ---------------- Helpers (GUI bindings) ----------------
static int gPowIdx = 0;         // 0..8 => 1..256
static int gSdIdx  = 3;         // 0..6 => 1..64
static inline int   leafFromIdx(int idx){ idx = std::clamp(idx,0,8); return 1<<idx; }
static inline double sdFromIdx(int idx){ idx = std::clamp(idx,0,6); return double(1<<idx); }


// Track sizes we want to show in UI
static uintmax_t gOriginalFileBytes = 0; // size on disk of the source image
static size_t    gLastPngBytes      = 0; // size of current quadtree-render as PNG
static size_t    gLeafDataBytes     = 0; // raw leaf data size (uncompressed)

// Encode current quadtree image to PNG in memory and return byte size
static size_t pngSizeOfCurrent(const Node* root, int W, int H) {
    if (!root || W <= 0 || H <= 0) return 0;
    std::vector<Color> buf(W * H);
    rasterizeQT(root, W, H, buf); // already provided in your code

    int out_len = 0;
    unsigned char* mem = stbi_write_png_to_mem(
        reinterpret_cast<unsigned char*>(buf.data()),
        W * 3,   // stride in bytes
        W, H, 3, // w, h, channels
        &out_len
    );
    if (mem) { STBIW_FREE(mem); }
    return (size_t)out_len;
}


// ---------------- Main ----------------
int main(int argc, char** argv) {
    // Persistent current path shown in UI
    gCurrentImagePath  = (argc >= 2) ? argv[1] : "./images/image.png"; // change default if you like
    // One-shot pending path that triggers a single load/rebuild
    gPendingImagePath  = "";  // <- make sure this is declared as a global

    if (!glfwInit()) { std::fprintf(stderr, "GLFW init failed\n"); return 1; }
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* win = glfwCreateWindow(std::max(IMG_W, 1024), std::max(IMG_H, 640),
                                       "Quadtree OpenGL + ImGui", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetDropCallback(win, dropCallback); // dropCallback should set gPendingImagePath

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL2_Init();

    // Load initial image (if exists)
    if (!gCurrentImagePath.empty() && loadImage(gCurrentImagePath)) {
        gOriginalFileBytes = getFileSize(gCurrentImagePath);
    }

    if (IMG_W == 0 || IMG_H == 0) {
        // Fallback to tiny checker
        IMG_W = IMG_H = 64;
        image.assign(IMG_H, std::vector<Color>(IMG_W));
        for (int y=0;y<IMG_H;++y) for (int x=0;x<IMG_W;++x) {
            bool b = ((x/8 + y/8) & 1)==0;
            image[y][x] = b ? Color{220,220,220} : Color{40,40,40};
        }
    }

    // Build first quadtree
    Node* root = nullptr;
    BuildStats stats{};
    auto rebuild = [&](){
        destroy(root);
        stats = {};
        auto t0 = std::chrono::high_resolution_clock::now();
        root = buildQT(image, 0,0, IMG_W, IMG_H, leafFromIdx(gPowIdx), sdFromIdx(gSdIdx), stats);
        auto t1 = std::chrono::high_resolution_clock::now();
        stats.ms = std::chrono::duration<double, std::milli>(t1-t0).count();

        // Update size readouts whenever we rebuild
        gLeafDataBytes = estimateQuadtreeBytes(stats.leaves, true);
        gLastPngBytes  = pngSizeOfCurrent(root, IMG_W, IMG_H);
    };
    rebuild();

    // Main loop
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        // Handle drag & drop / manual load exactly once per path
        if (!gPendingImagePath.empty()) {
            gCurrentImagePath = gPendingImagePath; // persist for UI
            if (loadImage(gCurrentImagePath)) {
                gOriginalFileBytes = getFileSize(gCurrentImagePath);
                rebuild();
            }
            gPendingImagePath.clear(); // consume the pending request
        }

        // Start ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ---- GUI panel ----
        ImGui::Begin("Quadtree Controls");

        if (ImGui::CollapsingHeader("Image", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Size: %d x %d", IMG_W, IMG_H);

            if (!gCurrentImagePath.empty()) {
                ImGui::Text("Path: %s", gCurrentImagePath.c_str());
                if (gOriginalFileBytes > 0) {
                    ImGui::Text("Original file size: %.2f KB (%llu bytes)",
                                gOriginalFileBytes / 1024.0,
                                (unsigned long long)gOriginalFileBytes);
                } else {
                    ImGui::Text("Original file size: (unknown)");
                }
            } else {
                ImGui::Text("No image loaded from disk.");
            }

            ImGui::Text("Drag & drop an image file into the window.");
            static char buf[512] = {0};
            if (ImGui::InputTextWithHint("##path","path/to/image", buf, sizeof(buf),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string path(buf);
                gPendingImagePath = path; // one-shot load next frame
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                std::string path(buf);
                gPendingImagePath = path; // one-shot load next frame
            }

            ImGui::Checkbox("Fit to window", &gFitToWindow);

            // --- Save button ---
            static char outPath[512] = "/output/output.png";
            ImGui::InputTextWithHint("##out","output filename", outPath, sizeof(outPath));
            if (ImGui::Button("Save quadtree PNG")) {
                bool ok = saveQuadtreePNG(outPath, root, IMG_W, IMG_H);
                if (ok) {
                    std::cout << "Saved: " << outPath << "\n";
                    // Optional: also report actual file size on disk if you saved to disk
                    try {
                        gLastPngBytes = (size_t)std::filesystem::file_size(outPath);
                    } catch (...) {
                        // fallback: keep in-memory size
                        gLastPngBytes = pngSizeOfCurrent(root, IMG_W, IMG_H);
                    }
                } else {
                    std::cerr << "Failed to save: " << outPath << "\n";
                }
            }
        }

        if (ImGui::CollapsingHeader("Segmentation", ImGuiTreeNodeFlags_DefaultOpen)) {
            int powIdxTmp = gPowIdx;
            int sdIdxTmp  = gSdIdx;

            ImGui::SliderInt("Leaf power", &powIdxTmp, 0, 8, "2^%d");
            ImGui::Text("Leaf size: %d px", leafFromIdx(powIdxTmp));
            ImGui::SliderInt("StdDev power", &sdIdxTmp, 0, 6, "2^%d");
            ImGui::Text("StdDev threshold: %.0f", sdFromIdx(sdIdxTmp));

            bool changed = (powIdxTmp != gPowIdx) || (sdIdxTmp != gSdIdx);
            gPowIdx = powIdxTmp; gSdIdx = sdIdxTmp;

            ImGui::Separator();
            ImGui::Checkbox("Fill", &gDrawFill); ImGui::SameLine();
            ImGui::Checkbox("Grid", &gDrawLines);
            if (ImGui::Button("Rebuild") || changed) rebuild();
        }

        if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Nodes:  %zu", stats.nodes);
            ImGui::Text("Leaves: %zu", stats.leaves);
            ImGui::Text("Build:  %.3f ms", stats.ms);



            // Raw (uncompressed) leaf data size (x,y,w,h + RGB per leaf)
            ImGui::Text("Raw leaf data: %.2f KB (%zu bytes)",
                        gLeafDataBytes / 1024.0, gLeafDataBytes);

            // Accurate (compressed) PNG size of current quadtree render
            ImGui::Text("Quadtree PNG size: %.2f KB (%zu bytes)",
                        gLastPngBytes / 1024.0, gLastPngBytes);



            float leavesPct = stats.nodes ? (100.0f * (float)stats.leaves / (float)stats.nodes) : 0.f;
            ImGui::ProgressBar(leavesPct/100.f, ImVec2(-FLT_MIN, 0),
                               (std::to_string((int)leavesPct)+"% leaves").c_str());
        }

        ImGui::End();

        // ---- Render scene ----
        int fbW, fbH; glfwGetFramebufferSize(win, &fbW, &fbH);
        glViewport(0,0,fbW,fbH);
        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Compute canvas dimensions (fit or pixel-true)
        float canvasW = (float)IMG_W, canvasH = (float)IMG_H;
        if (gFitToWindow) {
            // render in NDC; map to a virtual canvas maintaining aspect ratio
            float imgAspect = canvasW / canvasH;
            float fbAspect  = (float)fbW / (float)fbH;
            if (fbAspect > imgAspect) {
                // wider window: expand width
                canvasW = imgAspect * (float)fbH;
                canvasH = (float)fbH;
            } else {
                // taller window: expand height
                canvasW = (float)fbW;
                canvasH = (float)fbW / imgAspect;
            }
        }

        renderQT(root, canvasW, canvasH);

        // ImGui draw
        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    destroy(root);
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}

