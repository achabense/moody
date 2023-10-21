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
#include <vector>

#include "imgui.h"
#include "imgui_sdl2/imgui_impl_sdl2.h"
#include "imgui_sdl2/imgui_impl_sdlrenderer2.h"

#include "app.hpp"
#include "image.hpp"

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

rule_maker maker(/* seed= */ time(nullptr));
tile_filler filler(/* seed= */ 0);
rule_runner runner({.height = 240, .width = 320});
rule_recorder recorder;

// TODO: how to generalize this?
constexpr int pergen_min = 1, pergen_max = 10;
int pergen = 1;

// TODO: when is this needed?
constexpr int start_min = 0, start_max = 20;
int start_from = 0;

bool cal_rate = true;

// looks *extremely* horrible and inefficient
// TODO: use imgui line wrapping....
std::string wrap_rule_string(const std::string& str) {
    return str.substr(0, 32) + "\n" + str.substr(32, 16) + "...";
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

    // init:
    runner.m_filler = &filler;
    recorder.m_runner = &runner;
    // TODO: is this correct design?
    recorder.attach_maker(&maker);
    // recorder.m_maker = &maker;
    // recorder.new_rule(); // first rule...

    tile_image img(renderer, runner.tile()); // TODO: can be
    bool paused = false;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Our state
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

        // TODO: remove this when suitable...
        ImGui::ShowDemoWindow();

        {
            ImGui::Begin("-v-", nullptr,
                         ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize);

            {
                static int frame = 0;
                ImGui::Text("(%.1f FPS) Frame:%d\n"
                            "Width:%d,Height:%d",
                            io.Framerate, frame++, img.width(), img.height()); // TODO: why img?
            }

            {
                ImVec2 pos = ImGui::GetCursorScreenPos();

                img.update(runner.tile());
                auto img_texture = img.texture();
                ImVec2 img_size = ImVec2(img.width(), img.height());
                ImGui::Image(img_texture, img_size);

                // TODO: should be draggable...
                if (ImGui::BeginItemTooltip()) {
                    auto&& io = ImGui::GetIO();
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

                    ImGui::TextUnformatted("Right click to copy to clipboard:"); // TODO: show successful / fail...
                    auto rule_str = to_string(runner.rule());                    // how to reuse the resource?
                    ImGui::TextUnformatted(wrap_rule_string(rule_str).c_str());
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Right)) {
                        ImGui::SetClipboardText(rule_str.c_str()); // TODO: notify...
                    }
                    ImGui::TextUnformatted("Left click to copy from clipboard:");
                    std::string found_str;
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
                    ImGui::TextUnformatted(found_str.empty() ? "(none)" : wrap_rule_string(found_str).c_str());
                    // TODO: not suitable to use left click...
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Left) && !found_str.empty()) {
                        recorder.take(legacy::from_string<legacy::ruleT>(found_str));
                    }
                    ImGui::EndTooltip();
                }

                // TODO: how to drag to resize?
                if (ImGui::IsItemHovered()) {
                    // TODO: should be supported in certain areas... add more ways to control!
                    if (io.MouseWheel < 0) { // scroll down
                        paused = false;
                        recorder.next();
                    } else if (io.MouseWheel > 0) { // scroll up
                        paused = false;
                        recorder.prev();
                    }
                }

                ImGui::Text("Total:%d At:%d%s", recorder.size(), recorder.pos(),
                            recorder.pos() + 1 == recorder.size() ? "(last)" : ""); // TODO: random-access...

                // how to activate only on Enter?
                // static char _goto[20]{};
                // if (ImGui::InputText("Goto", _goto,std::size(_goto))) {
                //     putchar('\a');
                // }
            }

            ImGui::SeparatorText("Rule generator");

            {
                bool new_rule = false;

                // TODO: horribly hard to maintain...
                // is reinter ok?
                if (ImGui::Combo("Mode", reinterpret_cast<int*>(&maker.g_mode),
                                 "none(deprecated)\0sub_spatial\0sub_spatial2\0spatial\0spatial_paired\0spatial_"
                                 "state\0permutation\0\0")) {
                    maker.density = maker.max_density() * 0.3;
                    new_rule = true;
                }

                // TODO: too sensitive...
                // TODO: is it suitable to restart immediately?
                char str[40];
                snprintf(str, 40, "Rule density [%d-%d]", 0, maker.max_density());
                if (ImGui::SliderInt(str, &maker.density, 0, maker.max_density(), "%d", ImGuiSliderFlags_NoInput)) {
                    new_rule = true;
                }

                auto prev = maker.interpret_as;
                ImGui::RadioButton("plain", &maker.interpret_as, legacy::ABS);
                ImGui::SameLine();
                ImGui::RadioButton("flip", &maker.interpret_as, legacy::XOR);
                if (maker.interpret_as != prev) {
                    new_rule = true;
                }

                ImGui::SameLine();
                if (ImGui::Button("New rule")) {
                    new_rule = true;
                }

                if (new_rule) {
                    recorder.new_rule();
                }
            }

            // TODO: use fixed-point...
            ImGui::SeparatorText("Tile");

            {
                ImGui::Text("Gen:%d", runner.gen());

                ImGui::Checkbox("Calculate density", &cal_rate);
                if (cal_rate) {
                    ImGui::SameLine();
                    auto [h, w] = runner.tile().shape();
                    int count = runner.tile().count();
                    ImGui::Text("(%f)", float(count) / (h * w));
                }
            }

            {
                ImGui::Checkbox("Pause", &paused);

                ImGui::SameLine();
                ImGui::PushButtonRepeat(true); // accept consecutive clicks... TODO: too fast...
                if (ImGui::Button("+1")) {
                    runner.run(1);
                }
                ImGui::PopButtonRepeat();

                ImGui::SameLine();
                if (ImGui::Button("Restart")) {
                    runner.restart();
                    // runner.reset_tile();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reseed")) {
                    runner.m_filler->disturb();
                    runner.restart();
                }
            }

            {
                // TODO: (?) currently suitable to restart immediately...
                if (ImGui::SliderFloat("Init density [0.0-1.0]", &runner.m_filler->density, 0.0f, 1.0f, "%.3f",
                                       ImGuiSliderFlags_NoInput)) {
                    runner.restart();
                }
                ImGui::SliderInt("Per gen [1-10]", &pergen, pergen_min, pergen_max, "%d",
                                 ImGuiSliderFlags_NoInput); // TODO: use sprintf for pergen_min and pergen_max,
                                                            // start_min, start_max...
                ImGui::SliderInt("Start gen [0-20]", &start_from, start_min, start_max, "%d", ImGuiSliderFlags_NoInput);
            }

            ImGui::End();

            // run tile (TODO: should be here?)
            if (runner.gen() == 0) {
                runner.run(start_from);
            }
            if (!paused) {
                // TODO: support timing...
                runner.run(pergen);
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
