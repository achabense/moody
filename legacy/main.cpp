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

#include <SDL.h>
#include <random>
#include <regex>
#include <stdio.h>
#include <vector>

#include "imgui.h"
#include "imgui_sdl2/imgui_impl_sdl2.h"
#include "imgui_sdl2/imgui_impl_sdlrenderer2.h"

#include "image.hpp"
#include "rule.hpp"
#include "serialize.hpp"

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

struct fooo {
    legacy::ruleT m_rule;
    legacy::tileT m_tile;

private:
    legacy::tileT m_side;
    std::mt19937_64 m_rand{0};

public:
    float density = 0.5;

    int m_gen{0}; // TODO: publicly only readable

    void reset_rule(const legacy::ruleT& rule) {
        m_rule = rule;
        reset_tile();
    }

    void reset_tile() {
        m_tile.random_fill(density, std::mt19937_64{m_rand});
        m_gen = 0;
    }

    void change_seed() {
        (void)m_rand(); // enough to make great difference...
        reset_tile();
    }

    void run(int count) {
        for (int i = 0; i < count; ++i) {
            m_tile.gather().apply(m_rule, m_side);
            m_tile.swap(m_side);
        }
        m_gen += count;
    }

    fooo(legacy::shapeT shape) : m_tile(shape), m_side(shape, legacy::tileT::no_clear) {}
};

// TODO: allow resizing the grid.
// TODO: rule editor... (based on mini-window, click the pixel to set...)..
// TODO: where to add gol?
// TODO: add pace.
// TODO: (important) small window...

// TODO: notify on quit...
// TODO!!!!!support more run-mode...
// TODO: restart should...
// TODO: run_extra..
// TODO: some settings should leave states in paused state...
// TODO: rnd-mode for tile...(stable vs arbitary...)
// TODO: right click to enable/disable miniwindow...
// TODO: fine-grained rule edition...
// TODO: file container. easy ways to add fav...

struct barr {
    std::vector<legacy::compress> record; // TODO: compressT? (and compress() function...)
    int cursor{-1};                       // TODO: always<=size()-1.

    // TODO: decide range format
    // [0, max_density]
    static constexpr int max_density = legacy::sym.k;
    int density = max_density * 0.3; // TODO: density is actually for generation, not matching with cursor now...
    std::mt19937_64 m_rand{uint64_t(time(0))};

    static constexpr int as_plain = 0;
    static constexpr int as_flip = 1;
    int interpret_as = as_flip;

    // TODO: the interface relies on too many things...
    // TODO: what if making the same rule?
    legacy::ruleT new_rule() {
        legacy::sruleT srule; // TODO: unnecessary value initialization...
        srule.random_fill(density, m_rand);
        legacy::ruleT rule = interpret_as == as_plain ? srule : srule.as_flip();
        (void)accept_rule(rule); // TODO: see, DEAL WITH SAME RULE... TODO: what if totally the same as run_env's?
        return rule;
    }

    legacy::ruleT prev_rule() {
        assert(cursor != -1);
        if (cursor > 0) {
            --cursor;
        }
        return legacy::ruleT(record[cursor]);
    }

    legacy::ruleT next_rule() {
        assert(cursor + 1 <= record.size());
        if (cursor + 1 != record.size()) {
            ++cursor;
            return legacy::ruleT(record[cursor]);
        } else {
            return new_rule();
        }
    }

    legacy::ruleT accept_rule(const legacy::ruleT& rule) {
        // TODO: refine logic, search around...
        // TODO: let histroy hold one rule to avoid awkwardness...
        legacy::compress cmpr(rule);
        if (!record.empty()) {
            if (record[cursor] == cmpr) {
                return rule;
            }
        }
        cursor = record.size();
        record.push_back(cmpr);
        return rule;
    }
};

// TODO: what should be done when ..?

fooo run_env{{.height = 240, .width = 320}};

constexpr int pergen_min = 1, pergen_max = 10;
int pergen = 1;

barr rul_env;

bool cal_rate = true;

std::string wrap_rule_string(const std::string& str) {
    // TODO: looks *extremely* horrible and inefficient... are there library functions for wrapping?
    return str.substr(0, 32) + "-\n" + str.substr(32, 32) + "-\n" + str.substr(64, 32) + "-\n" + str.substr(96, 32);
}

int main(int, char**) {
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with SDL_Renderer graphics context
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        SDL_Log("Error creating SDL_Renderer!");
        return 0;
    }

    run_env.reset_rule(rul_env.new_rule());
    tile_image img(renderer, run_env.m_tile);
    bool paused = false;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Our state
    int frame = 0;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your
        // inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or
        // clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or
        // clear/overwrite your copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and
        // hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                done = true;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                done = true;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code
        // to learn more about Dear ImGui!).
        ImGui::ShowDemoWindow();

        {
            ImGui::Begin("-v-", nullptr,
                         ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize);

            // clang-format off
            { 
                ImGui::Text("(%.1f FPS) Frame:%d", io.Framerate, frame++);
            }
            // clang-format on
            // TODO wtf, why collapsing into one line?

            {
                ImGui::Text("Width:%d,Height:%d", img.width(), img.height());
                // TODO: clang-format..
            }

            {
                img.update(run_env.m_tile);

                ImVec2 pos = ImGui::GetCursorScreenPos();
                auto img_texture = img.texture();
                ImVec2 img_size = ImVec2(img.width(), img.height());
                ImGui::Image(img_texture, img_size);

                // TODO: should be draggable...
                // mostly copied from imgui_demo.cpp, works but looks terrible...
                auto&& io = ImGui::GetIO();
                ImVec2 uv_min = ImVec2(0.0f, 0.0f); // Top-left
                ImVec2 uv_max = ImVec2(1.0f, 1.0f); // Lower-right
                ImVec4 border_col = ImGui::GetStyleColorVec4(ImGuiCol_Border);
                if (ImGui::BeginItemTooltip()) {
                    const float region_sz = 32.0f;
                    float region_x = io.MousePos.x - pos.x - region_sz * 0.5f;
                    float region_y = io.MousePos.y - pos.y - region_sz * 0.5f;
                    const float zoom = 4.0f; // TODO: constexpr; should be settable between 4 and 8.
                    if (region_x < 0.0f) {
                        region_x = 0.0f;
                    } else if (region_x > img_size.x - region_sz) {
                        region_x = img_size.x - region_sz;
                    }
                    if (region_y < 0.0f) {
                        region_y = 0.0f;
                    } else if (region_y > img_size.y - region_sz) {
                        region_y = img_size.y - region_sz;
                    }
                    ImVec2 uv0 = ImVec2((region_x) / img_size.x, (region_y) / img_size.y);
                    ImVec2 uv1 = ImVec2((region_x + region_sz) / img_size.x, (region_y + region_sz) / img_size.y);
                    ImGui::Image(img_texture, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1);
                    ImGui::EndTooltip();
                }

                if (ImGui::BeginItemTooltip()) {
                    ImGui::Text("Right click to copy to clipboard:"); // TODO: show successful / fail...
                    auto rule_str = to_string(run_env.m_rule);        // how to reuse the resource?
                    ImGui::Text(wrap_rule_string(rule_str).c_str());
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Right)) {
                        ImGui::LogToClipboard();
                        ImGui::LogText(rule_str.c_str());
                        ImGui::LogFinish();
                    }
                    ImGui::EndTooltip();
                }
                // TODO: still in the image...
                // TODO: how to drag to resize?
                if (ImGui::IsItemHovered()) {
                    // TODO: scrolling...
                    // TODO: should be supported in certain areas... add more ways to control!
                    if (io.MouseWheel < 0) {
                        paused = false;
                        run_env.reset_rule(rul_env.next_rule());
                    } else if (io.MouseWheel > 0 && rul_env.cursor != 0) { // TODO: should cursor!=0 be used?
                        paused = false;
                        run_env.reset_rule(rul_env.prev_rule()); // TODO: can "reset_rule" ignore same rule?
                    }
                }

                ImGui::Text("Total:%d At:%d%s", (int)rul_env.record.size(), rul_env.cursor,
                            rul_env.cursor + 1 == rul_env.record.size() ? "(last)" : ""); // TODO: random-access...
            }

            ImGui::SeparatorText("Rule");

            {
                bool changed = false; // TODO problematic... rul_env should be able to feel the changes...
                changed |= ImGui::RadioButton("plain", &rul_env.interpret_as, barr::as_plain);
                ImGui::SameLine();
                changed |= ImGui::RadioButton("flip", &rul_env.interpret_as, barr::as_flip);
                ImGui::SameLine();
                if (changed) {
                    run_env.reset_rule(rul_env.new_rule());
                }
                // TODO: inconsistent?
                ImGui::SameLine();
                if (ImGui::Button("New rule")) {
                    run_env.reset_rule(rul_env.new_rule());
                }

                ImGui::SameLine();
                {
                    bool clicked = ImGui::Button("Paste from clipboard");
                    std::string found_str;
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone)) {
                        const char* text = ImGui::GetClipboardText();
                        // TODO: needed?
                        if (text) {
                            std::string str = text;
                            // TODO: the logic should be in <seralize.h>...
                            std::regex find_rule("[0-9a-z]{128}");
                            std::smatch match_result;
                            if (std::regex_search(str, match_result, find_rule)) {
                                found_str = match_result[0];
                            }
                        }
                        ImGui::SetTooltip(found_str.empty() ? ":|" : wrap_rule_string(found_str).c_str());
                    }
                    if (clicked && !found_str.empty()) {
                        run_env.reset_rule(rul_env.accept_rule(legacy::from_string<legacy::ruleT>(found_str)));
                    }
                }
            }

            // TODO: ¡ý too sensitive...
            // TODO: auto-wrapping... after input from console...
            {
                char str[40];
                snprintf(str, 40, "Rule density [%d-%d]", 0, barr::max_density);
                // TODO: better input
                if (ImGui::SliderInt(str, &rul_env.density, 0, barr::max_density)) {
                    run_env.reset_rule(rul_env.new_rule());
                }
            }

            // TODO: use fixed-point...
            ImGui::SeparatorText("Tile");

            {
                ImGui::Text("Gen:%d", run_env.m_gen);

                // TODO: is "Life" correct?
                ImGui::Checkbox("Calculate life rate", &cal_rate);
                if (cal_rate) {
                    ImGui::SameLine();
                    auto [h, w] = run_env.m_tile.shape();
                    int count = run_env.m_tile.count();
                    ImGui::Text("(%f)", float(count) / (h * w));
                }
            }

            {
                ImGui::Checkbox("Pause", &paused);

                ImGui::SameLine();
                ImGui::PushButtonRepeat(true); // accept consecutive clicks... TODO: too fast...
                if (ImGui::Button(">>1")) {    // todo: reword...
                    run_env.run(1);
                }
                ImGui::PopButtonRepeat();

                ImGui::SameLine();
                if (ImGui::Button("Restart")) {
                    run_env.reset_tile();
                }
                // TODO: should init state be maintained individually?
                ImGui::SameLine();
                if (ImGui::Button("Restart with new seed")) {
                    run_env.change_seed();
                }
            }

            {
                if (ImGui::SliderFloat("Init density [0.0-1.0]", &run_env.density, 0.0f, 1.0f)) {
                    run_env.reset_tile();
                }
            }

            {
                // TODO: shouldnt be here; intergrate to runmode...
                ImGui::SliderInt("Per gen [1-10]", &pergen, pergen_min, pergen_max); // TODO: use sprintf...
            }

            ImGui::End();

            // run tile (TODO: should be here?)
            if (!paused) {
                // TODO: support timing...
                run_env.run(pergen);
            }
        }

        // Rendering
        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255),
                               (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
