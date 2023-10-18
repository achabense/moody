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

#include "image.hpp"
#include "rule.hpp"
#include "serialize.hpp"

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

// TODO: add ctor...
class tile_filler {
    std::mt19937_64 m_rand{0};

public:
    float density = 0.5;

    void disturb() {
        (void)m_rand();
    }
    void random_fill(legacy::tileT& tile) const {
        tile.random_fill(density, std::mt19937_64{m_rand});
    }
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

////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: editor, very hard...
struct rule_maker2 {
    void random_fill(bool* begin, int size, int density) {
        assert(size > 0);
        density = std::clamp(density, 0, size);
        std::fill_n(begin, size, false);
        std::fill_n(begin, density, true);
        std::shuffle(begin, begin + size, m_rand);
    }

    bool want_spatial_symmetry = true;
    bool want_state_symmetry = false;

    std::mt19937_64 m_rand;

    static constexpr int as_abs = 0;
    static constexpr int as_flip = 1;
    int interpret_as = as_abs;

    // a series of filters...
    // TODO: construct...
    // TODO: user-defined?
    enum sym_mode : int {
        none, // arbitary
        spatial_symmetric,
        spatial_state_symmetric,

        gol
    } m_sym{};

    rule_maker2(uint64_t seed = time(0)) : m_rand{seed} {}
    void disturb() {
        // TODO: explain...
        (void)m_rand();
    }

    int density{};
    int max_density() const {
        switch (m_sym) {
        case none:
            return 512;
        }
    }

    legacy::ruleT make() {
        legacy::ruleT::array_base rule; // todo: zeroed-out...
        switch (m_sym) {
        case none:
            random_fill(rule.data(), 512, density);
        case spatial_symmetric:;
        }
        return rule;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: density is actually for generation, not matching with cursor now...
class rule_maker {
    std::mt19937_64 m_rand;

public:
    rule_maker(uint64_t seed) : m_rand{seed} {}

    // density ¡Ê [0, max_density]
    static constexpr int max_density = legacy::sym.k;
    int density = max_density * 0.3;

    static constexpr int as_abs = 0;
    static constexpr int as_flip = 1; // as_xor?
    int interpret_as = as_flip;

    // TODO: refine making logic...
    // TODO: as this is already decoupled, support different modes.
    legacy::ruleT make() {
        legacy::sruleT srule; // TODO: unnecessary value initialization...
        srule.random_fill(density, m_rand);
        return interpret_as == as_abs ? srule : srule.as_flip();
    }
};

// TODO: support shifting...
// TODO: !!!! recheck when to "restart"..
class rule_runner {
    legacy::ruleT m_rule;
    legacy::tileT m_tile;
    legacy::tileT m_side;
    int m_gen = 0;

    // TODO: reconsider this design...
    std::vector<legacy::compress> m_record;
    int m_pos = -1; // always<=size()-1.// TODO: how to make m_pos always valid?

public:
    // partly detachable?
    tile_filler* m_filler = nullptr;

    const legacy::ruleT& rule() const {
        return m_rule;
    }

    const legacy::tileT& tile() const {
        return m_tile;
    }

    int gen() const {
        return m_gen;
    }

    int record_size() const {
        return m_record.size();
    }

    int record_pos() const {
        return m_pos;
    }

    void reset_rule(const legacy::ruleT& rule) {
        // TODO: refine logic, search around...
        // TODO: let histroy hold one rule to avoid awkwardness...
        if (m_record.empty() || m_rule != rule) {
            m_record.emplace_back(rule);
            m_pos = m_record.size() - 1;

            m_rule = rule;
            restart();
        }
    }

    void reset_rule(rule_maker& maker) {
        reset_rule(maker.make());
    }

    void next(rule_maker& maker) {
        assert(m_pos + 1 <= m_record.size());
        if (m_pos + 1 != m_record.size()) {
            m_rule = legacy::ruleT(m_record[++m_pos]);
            restart();
        } else {
            reset_rule(maker);
        }
    }

    void prev() {
        assert(m_pos != -1); // TODO: needed? enough?
        if (m_pos > 0) {
            m_rule = legacy::ruleT(m_record[--m_pos]);
            restart(); // TODO: conditional or not?
        }
    }

    // TODO: --> reset_tile?
    void restart() {
        m_filler->random_fill(m_tile);
        m_gen = 0;
    }

    void reseed() {
        m_filler->disturb();
        restart();
    }

    void run(int count) {
        for (int i = 0; i < count; ++i) {
            m_tile.gather().apply(m_rule, m_side);
            m_tile.swap(m_side);
        }
        m_gen += count;
    }

    void shift(int dy, int dx) {
        m_tile.shift(dy, dx, m_side);
        m_tile.swap(m_side);
    }

    rule_runner(legacy::shapeT shape) : m_tile(shape), m_side(shape, legacy::tileT::no_clear) {}
    // TODO: incomplete: m_filler!=nullptr, and m_record.!empty...
};

rule_maker maker(time(0));
tile_filler filler;
rule_runner runner({.height = 240, .width = 320});

constexpr int pergen_min = 1, pergen_max = 10;
int pergen = 1;

bool cal_rate = true;

// looks *extremely* horrible and inefficient
// TODO: use imgui line wrapping....
std::string wrap_rule_string(const std::string& str) {
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

    runner.m_filler = &filler;
    runner.reset_rule(maker);

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

        // TODO: remove this when suitable...
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code
        // to learn more about Dear ImGui!).
        ImGui::ShowDemoWindow();

        {
            ImGui::Begin("-v-", nullptr,
                         ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize);

            {
                ImGui::Text("(%.1f FPS) Frame:%d\n"
                            "Width:%d,Height:%d",
                            io.Framerate, frame++, img.width(), img.height());
            }

            {
                img.update(runner.tile());

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
                    ImGui::TextUnformatted("Right click to copy to clipboard:"); // TODO: show successful / fail...
                    auto rule_str = to_string(runner.rule());                    // how to reuse the resource?
                    ImGui::TextUnformatted(wrap_rule_string(rule_str).c_str());
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Right)) {
                        ImGui::SetClipboardText(rule_str.c_str()); // TODO: notify...
                    }
                    ImGui::EndTooltip();
                }
                // TODO: how to drag to resize?
                if (ImGui::IsItemHovered()) {
                    // TODO: should be supported in certain areas... add more ways to control!
                    if (io.MouseWheel < 0) {
                        paused = false;
                        runner.next(maker);
                    } else if (io.MouseWheel > 0 /* && recorder.pos() != 0*/) { // TODO: should cursor!=0 be used?
                        paused = false; // TODO: ¡ü has behavioral change here..
                        runner.prev();
                    }
                }

                ImGui::Text("Total:%d At:%d%s", runner.record_size(), runner.record_pos(),
                            runner.record_pos() + 1 == runner.record_size() ? "(last)" : ""); // TODO: random-access...
            }

            ImGui::SeparatorText("Rule");

            {
                bool changed = false; // TODO problematic... rul_env should be able to feel the changes...
                changed |= ImGui::RadioButton("plain", &maker.interpret_as, rule_maker::as_abs);
                ImGui::SameLine();
                changed |= ImGui::RadioButton("flip", &maker.interpret_as, rule_maker::as_flip);
                ImGui::SameLine();
                if (changed) {
                    runner.reset_rule(maker);
                }
                // TODO: inconsistent?
                ImGui::SameLine();
                if (ImGui::Button("New rule")) {
                    runner.reset_rule(maker);
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
                        runner.reset_rule(legacy::from_string<legacy::ruleT>(found_str));
                    }
                }
            }

            // TODO: ¡ý too sensitive...
            // TODO: auto-wrapping... after input from console...
            {
                char str[40];
                snprintf(str, 40, "Rule density [%d-%d]", 0, rule_maker::max_density);
                // TODO: better input
                if (ImGui::SliderInt(str, &maker.density, 0, rule_maker::max_density, "%d", ImGuiSliderFlags_NoInput)) {
                    runner.reset_rule(maker);
                }
            }

            // TODO: use fixed-point...
            ImGui::SeparatorText("Tile");

            {
                ImGui::Text("Gen:%d", runner.gen());

                // TODO: is "Life" correct?
                ImGui::Checkbox("Calculate life rate", &cal_rate);
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
                if (ImGui::Button(">>1")) {    // todo: reword...
                    runner.run(1);
                }
                ImGui::PopButtonRepeat();

                ImGui::SameLine();
                if (ImGui::Button("Restart")) {
                    runner.restart();
                    // runner.reset_tile();
                }
                ImGui::SameLine();
                if (ImGui::Button("Restart with new seed")) {
                    runner.reseed();
                }
            }

            {
                if (ImGui::SliderFloat("Init density [0.0-1.0]", &runner.m_filler->density, 0.0f, 1.0f, "%.3f",
                                       ImGuiSliderFlags_NoInput)) {
                    runner.restart();
                }
            }

            {
                ImGui::SliderInt("Per gen [1-10]", &pergen, pergen_min, pergen_max, "%d",
                                 ImGuiSliderFlags_NoInput); // TODO: use sprintf for pergen_min and pergen_max.
            }

            ImGui::End();

            // run tile (TODO: should be here?)
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
