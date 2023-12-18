// Dear ImGui: standalone example application for SDL2 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context
// creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important to understand: SDL_Renderer is an _optional_ component of SDL2.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#pragma once

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <SDL.h>

#include "imgui.h"
#include "imgui_sdl2/imgui_impl_sdl2.h"
#include "imgui_sdl2/imgui_impl_sdlrenderer2.h"

#include "rule.hpp"
#include "tile.hpp"

// TODO: "This backend"?
#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

// Unfortunately, SDL2-renderer backend doesn't support docking features...
// https://github.com/ocornut/imgui/issues/5835

class app_backend {
    struct [[nodiscard]] scope_guard {
        using F = void (*)();
        F fn;
        scope_guard(F f) : fn(f) {}
        ~scope_guard() { fn(); }
        scope_guard(const scope_guard&) = delete;
        scope_guard& operator=(const scope_guard&) = delete;
    };

    static inline SDL_Window* window = nullptr;
    static inline SDL_Renderer* renderer = nullptr;

public:
    app_backend() = delete;

    // ~ Guaranteed copy-elision is supported since C++17.
    // TODO: describe effects...
    // ... return scope-guard which must not be discarded...
    [[nodiscard]] static scope_guard init() {
        assert(!window && !renderer);

        // Setup SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
            printf("Error: %s", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        // IME: "Input Method Editor"
        // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
        SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

        // Create window with SDL_Renderer graphics context
        const SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
        if (!window) {
            printf("Error: %s", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            printf("Error: %s", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // Setup Platform/Renderer backends
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);

        return scope_guard(+[] {
            ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext();

            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        });
    }

    // TODO: better name...
    // TODO: while(process_events()) looks a bit strange?
    [[nodiscard]] static bool process_events() {
        assert(window && renderer);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                return false;
            }
            // TODO: this appears not needed:
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                return false;
            }
        }
        return true;
    }

    // Begin new frame; return scope-guard which must not be discarded...
    [[nodiscard]] static scope_guard new_frame() {
        assert(window && renderer);

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        return scope_guard(+[] {
            // Rendering
            ImGui::Render();
            // TODO: is this necessary?
            ImGuiIO& io = ImGui::GetIO();
            SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
            // ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
            // SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255),
            //                        (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
            SDL_RenderPresent(renderer);
        });
    }

private:
    // TODO: return unique_ptr instead?
    static SDL_Texture* create_texture(Uint32 format, int access, int w, int h) {
        assert(window && renderer);

        SDL_Texture* texture = SDL_CreateTexture(renderer, format, access, w, h);
        if (!texture) {
            printf("Error: %s", SDL_GetError());
            exit(EXIT_FAILURE);
        }
        return texture;
    }

    friend class tile_image;
    friend class code_image;
    // TODO: there is an extra use of sdl_basepath in main...
};

// TODO: pixel format might be problematic...
// TODO: in namespace or not? better name?
// TODO: able to deal with resized tile...
// TODO: should not update when paused...
class tile_image {
    int m_w, m_h;
    SDL_Texture* m_texture; // owning.

public:
    tile_image(const legacy::tileT& tile) : m_w(tile.width()), m_h(tile.height()) {
        m_texture = app_backend::create_texture(SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, m_w, m_h);
        update(tile);
    }

    ~tile_image() { SDL_DestroyTexture(m_texture); }

    tile_image(const tile_image&) = delete;
    tile_image& operator=(const tile_image&) = delete;

    void update(const legacy::tileT& tile) {
        assert(tile.width() == m_w && tile.height() == m_h);

        void* pixels = nullptr;
        [[maybe_unused]] int pitch = 0;
        [[maybe_unused]] bool succ = SDL_LockTexture(m_texture, nullptr, &pixels, &pitch) == 0;
        assert(succ && pitch == m_w * sizeof(Uint32));

        // Relying on both image and tile data being consecutive.
        const auto data = tile.data();
        for (int i = 0; i < tile.area(); ++i) {
            ((Uint32*)pixels)[i] = data[i] ? -1 /* white */ : 0;
        }
        SDL_UnlockTexture(m_texture);
    }

    int width() const { return m_w; }
    int height() const { return m_h; }

    SDL_Texture* texture() { return m_texture; }
};

// TODO: can be merged into app_backend...
class code_image {
    SDL_Texture* m_texture;

public:
    code_image() {
        const int width = 3, height = 3 * 512;
        m_texture = app_backend::create_texture(SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, height);
        // Uint32 pixels[512][3][3]; // TODO: explain why allocate...
        std::unique_ptr<Uint32[][3][3]> pixels(new Uint32[512][3][3]);
        for_each_code(code) {
            auto [q, w, e, a, s, d, z, x, c] = legacy::decode(code);
            bool fill[3][3] = {{q, w, e}, {a, s, d}, {z, x, c}};
            for (int y = 0; y < 3; ++y) {
                for (int x = 0; x < 3; ++x) {
                    pixels[code][y][x] = fill[y][x] ? -1 : 0;
                }
            }
        }

        SDL_UpdateTexture(m_texture, nullptr, pixels.get(), width * sizeof(Uint32));
    }

    ~code_image() { SDL_DestroyTexture(m_texture); }

    code_image(const code_image&) = delete;
    code_image& operator=(const code_image&) = delete;

    void image(legacy::codeT code, int zoom, const ImVec4& tint_col = ImVec4(1, 1, 1, 1),
               const ImVec4 border_col = ImVec4(0, 0, 0, 0)) const {
        const ImVec2 size(3 * zoom, 3 * zoom);
        const ImVec2 uv0(0, code * (1.0f / 512));
        const ImVec2 uv1(1, (code + 1) * (1.0f / 512));
        ImGui::Image(m_texture, size, uv0, uv1, tint_col, border_col);
    }

    bool button(legacy::codeT code, int zoom, const ImVec4& bg_col = ImVec4(0, 0, 0, 0),
                const ImVec4& tint_col = ImVec4(1, 1, 1, 1)) const {
        const ImVec2 size(3 * zoom, 3 * zoom);
        const ImVec2 uv0(0, code * (1.0f / 512));
        const ImVec2 uv1(1, (code + 1) * (1.0f / 512));
        ImGui::PushID(code);
        bool ret = ImGui::ImageButton("", m_texture, size, uv0, uv1, bg_col, tint_col);
        ImGui::PopID();
        return ret;
    }
};
