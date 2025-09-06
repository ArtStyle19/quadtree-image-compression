// main.cpp
// Quadtree + OpenGL + Dear ImGui (GLFW + OpenGL2 backend)
// Build with CMakeLists.txt below. Drag & drop an image file into the window to load it.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <algorithm>

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
static bool gFitToWindow = true;

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
        // glColor3f(0.47f, 0.0f, 0.47f);
        glColor3f(1.0f, 0.0f, 0.0f);
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

// Drag & drop handler
static std::string gCurrentImagePath = "";
static void dropCallback(GLFWwindow*, int count, const char** paths) {
    if (count > 0 && paths && paths[0]) {
        gCurrentImagePath = paths[0];
    }
}

// ---------------- Helpers (GUI bindings) ----------------
static int gPowIdx = 8;         // 0..8 => 1..256
static int gSdIdx  = 4;         // 0..6 => 1..64
static inline int   leafFromIdx(int idx){ idx = std::clamp(idx,0,8); return 1<<idx; }
static inline double sdFromIdx(int idx){ idx = std::clamp(idx,0,6); return double(1<<idx); }

// ---------------- Main ----------------
int main(int argc, char** argv) {
    gCurrentImagePath = (argc >= 2) ? argv[1] : "logo.png"; // you can change default here
    if (!glfwInit()) { std::fprintf(stderr, "GLFW init failed\n"); return 1; }
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* win = glfwCreateWindow( std::max(IMG_W, 1024), std::max(IMG_H, 640), "Quadtree OpenGL + ImGui", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetDropCallback(win, dropCallback);

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL2_Init();

    // Load initial image (if exists)
    if (!gCurrentImagePath.empty()) loadImage(gCurrentImagePath);
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
    };
    rebuild();

    // Main loop
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        // Handle drag & drop load
        if (!gCurrentImagePath.empty()) {
            if (loadImage(gCurrentImagePath)) {
                rebuild();
            }
            gCurrentImagePath.clear();
        }

        // Start ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ---- GUI panel ----
        ImGui::Begin("Quadtree Controls");
        if (ImGui::CollapsingHeader("Image", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Size: %d x %d", IMG_W, IMG_H);
            ImGui::Text("Drag & drop an image file into the window.");
            static char buf[512] = {0};
            if (ImGui::InputTextWithHint("##path","path/to/image", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string path(buf);
                if (loadImage(path)) { rebuild(); }
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                std::string path(buf);
                if (loadImage(path)) { rebuild(); }
            }
            ImGui::Checkbox("Fit to window", &gFitToWindow);
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
            float leavesPct = stats.nodes ? (100.0f * (float)stats.leaves / (float)stats.nodes) : 0.f;
            ImGui::ProgressBar(leavesPct/100.f, ImVec2(-FLT_MIN, 0), (std::to_string((int)leavesPct)+"% leaves").c_str());
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
            // We still render in NDC; just map source coordinates to a virtual canvas that matches framebuffer aspect
            // so the image fills the smaller dimension and keeps aspect ratio.
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
