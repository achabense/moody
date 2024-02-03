#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

// Unfortunately, SDL2-renderer backend doesn't support docking features...
// https://github.com/ocornut/imgui/issues/5835

#include <SDL.h>

#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "app.hpp"

// TODO (temp) the logics are extracted and modified from imgui's "example_sdl2_sdlrenderer2"...
static void init();
static void clear();
static bool begin_frame();
static void end_frame();

// TODO: Right-click must either to open a submenu, or to toggle on/off the tooltip.
// TODO: Generalize typical behavior patterns to find new rules.

void show_target_rule(const legacy::ruleT& target, rule_recorder& recorder) {
    const std::string rule_str = to_MAP_str(target);

    ImGui::AlignTextToFramePadding();
    imgui_str("[Current rule]");
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        ImGui::SetClipboardText(rule_str.c_str());
        logger::log_temp(300ms, "Copied");
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste")) {
        // TODO: redesign... (especially, should not replace directly?)
        if (const char* text = ImGui::GetClipboardText()) {
            auto result = legacy::extract_rules(text);
            // TODO: copied from the main function...
            if (!result.empty()) {
                logger::log_temp(500ms, "found {} rules", result.size());
                recorder.replace(std::move(result));
            } else {
                logger::log_temp(300ms, "found nothing");
            }
        }
        // else...
    }

    // TODO: re-implement file-saving?
    imgui_str(rule_str);

    // TODO: better layout...
    ImGui::Separator();
    // TODO: +1 is clumsy
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1);
    ImGui::SameLine();
    iter_pair(
        "<|", "prev", "next", "|>", //
        [&] { recorder.set_first(); }, [&] { recorder.set_prev(); }, [&] { recorder.set_next(); },
        [&] { recorder.set_last(); });
}

// TODO: refine...
struct fileT {
    struct lineT {
        bool has_rule;
        int id; // valid if has_rule.
        std::string text;
    };

    std::filesystem::path m_path;
    std::vector<lineT> m_lines;
    std::vector<legacy::compressT> m_rules;
    int pointing_at = 0; // valid if !m_rules.empty().

    // TODO: whether to throw if not found?
    fileT(std::filesystem::path path) : m_path(std::move(path)) {
        std::ifstream ifs(m_path);
        std::string line;

        int id = 0;
        // TODO: MSVC shows a warning here... can getline safely deal with moved-from string?
        while (std::getline(ifs, line)) {
            auto extr = legacy::extract_rules(line);
            bool has_rule = !extr.empty();
            m_lines.emplace_back(has_rule, has_rule ? id++ : 0, std::move(line));
            if (has_rule) {
                m_rules.push_back(extr[0]);
                // TODO: (how&) whether to support more fine-grained (in-line) rule-location?
                // (only the first rule in each line is being recognized... (so `extract_rules` is wasteful)
            }
        }
    }

    // TODO: how to combine with file-nav (into a single window)?
    std::optional<legacy::ruleT> display(const legacy::ruleT& test_sync) {
        static const bool wrap = true; // TODO: (temporarily const)

        std::optional<legacy::ruleT> out;
        bool hit = false; // TODO: also sync on window appearing?
        const int total = m_rules.size();

        if (total != 0) {
            const bool in_sync = test_sync == m_rules[pointing_at];

            ImGui::BeginGroup();
            iter_pair(
                "<|", "prev", "next", "|>",                                              //
                [&] { hit = true, pointing_at = 0; },                                    //
                [&] { hit = true, pointing_at = std::max(0, pointing_at - 1); },         //
                [&] { hit = true, pointing_at = std::min(total - 1, pointing_at + 1); }, //
                [&] { hit = true, pointing_at = total - 1; }, false, false);
            ImGui::SameLine();
            ImGui::Text("Total:%d At:%d", total, pointing_at + 1);
            if (!in_sync) {
                ImGui::SameLine();
                imgui_str(" (click to sync)");
            }
            ImGui::EndGroup();
            if (!in_sync && ImGui::IsItemHovered()) {
                const ImVec2 pos_min = ImGui::GetItemRectMin();
                const ImVec2 pos_max = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 30));
            }

            if (ImGui::IsItemClicked()) {
                hit = true;
            }

            // TODO: (temp) without ImGuiFocusedFlags_ChildWindows, clicking the child window will invalidate this.
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                if (imgui_keypressed(ImGuiKey_UpArrow, true)) {
                    hit = true, pointing_at = std::max(0, pointing_at - 1);
                }
                if (imgui_keypressed(ImGuiKey_DownArrow, true)) {
                    hit = true, pointing_at = std::min(total - 1, pointing_at + 1);
                }
            }
        } else {
            ImGui::Text("Not found");
        }

        const bool focus = hit;

        ImGui::Separator();

        if (auto child = imgui_childwindow("Child", {}, 0,
                                           wrap ? ImGuiWindowFlags_None : ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            // TODO: refine line-no logic (line-no or id-no)?
            for (int l = 1; const auto& [has_rule, id, text] : m_lines) {
                ImGui::TextDisabled("%3d ", l++);
                ImGui::SameLine();
                if (wrap) {
                    imgui_strwrapped(text);
                } else {
                    imgui_str(text);
                }
                if (has_rule) {
                    if (id == pointing_at) {
                        const ImVec2 pos_min = ImGui::GetItemRectMin();
                        const ImVec2 pos_max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 60));
                        if (!ImGui::IsItemVisible() && focus) {
                            ImGui::SetScrollHereY();
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        const ImVec2 pos_min = ImGui::GetItemRectMin();
                        const ImVec2 pos_max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 30));
                        if (ImGui::IsItemClicked()) {
                            pointing_at = id;
                            hit = true;
                        }
                    }
                }
            }
            ImGui::PopStyleVar();
        }

        if (hit) {
            assert(pointing_at >= 0 && pointing_at < total);
            return m_rules[pointing_at];
        } else {
            return std::nullopt;
        }
    }
};

// TODO: "paste" should have a similar widget...
class file_nav_v2 {
    file_nav m_nav;
    std::optional<fileT> m_file;
    bool m_open = false;

public:
    std::optional<legacy::ruleT> display(const legacy::ruleT& test_sync) {
        std::optional<legacy::ruleT> out;

        if (m_file) {
            assert(m_open);
            ImGui::SetNextWindowSize({720, 400}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200), ImVec2(FLT_MAX, FLT_MAX));
            if (auto window = imgui_window((cpp17_u8string(m_file->m_path) + "###File_").c_str(), &m_open,
                                           ImGuiWindowFlags_NoSavedSettings)) {
                out = m_file->display(test_sync);
            }
        }

        // TODO: whether to support opening multiple files?
        if (!m_open) {
            m_file.reset();
        }

        // TODO: better layout...
        if (auto sel = m_nav.display()) {
            m_file.emplace(*sel);
            m_open = true;
        }

        return out;
    }
};

// TODO: are there portable ways to convert argv to a valid filesystem::path (without messing up
// encodings)?
int main(int argc, char** argv) {
    init();
    {
        char* base_path = SDL_GetBasePath();
        if (!base_path) {
            printf("Error: %s", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        const std::string path = base_path;
        assert(path.ends_with('\\') || path.ends_with('/'));
        SDL_free(base_path);

        const auto strdup = [](const std::string& str) {
            char* buf = new char[str.size() + 1];
            strcpy(buf, str.c_str());
            return buf;
        };

        file_nav::add_special_path(std::filesystem::u8path(path), "Exe path");

        // Avoid "imgui.ini" (and maybe also "imgui_log.txt") sprinkling everywhere.
        // TODO: IniFilename and LogFilename should be unconditionally fixed (even if not using
        // base-path) (wontfix) These memory leaks are negligible.
        ImGui::GetIO().IniFilename = strdup(path + "imgui.ini");
        ImGui::GetIO().LogFilename = strdup(path + "imgui_log.txt");

        // TODO: remove when finished...
        file_nav::add_special_path(R"(C:\*redacted*\Desktop\rulelists_new)", "Temp");
    }

    // TODO: rephrase...
    // Currently the program doesn't attempt to deal with navigation mode.
    // The controls and program-defined widgets are not taking nav-mode compatiblity into consideration.
    // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // X
    // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // X
    assert(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard));
    assert(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad));

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // TODO: works but blurry, and how to apply in project?
    // ImGui::GetIO().Fonts->AddFontFromFileTTF(R"(C:\*redacted*\Desktop\Deng.ttf)", 13, nullptr,
    //                                          ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());

    // TODO: (temp) sync point is being {recorder.current(), locked}...
    rule_recorder recorder; // rule ~ current...
    legacy::lockT locked{};

    file_nav_v2 nav;

    // TODO: dtors(SDL_DestroyTexture) are being called after clear(SDL_Quit)
    code_image icons;
    tile_image img;

    while (begin_frame()) {
        ImGui::ShowDemoWindow(); // TODO: remove (or comment-out) this when all done...

        // TODO: refine sync logic...
        const legacy::ruleT rule = recorder.current();

        if (auto window = imgui_window("File nav")) {
            if (auto out = nav.display(rule)) {
                recorder.take(*out);
                // TODO: `locked` can be broken by irrelevant rules...
            }
        }

        if (auto window = imgui_window("Constraints", ImGuiWindowFlags_AlwaysAutoResize)) {
            if (auto out = static_constraints()) {
                auto& [rule, lock] = *out;
                recorder.take(rule); // TODO: (temp) will be used at next frame...
                locked = lock;
            }
        }

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        if (auto window = imgui_window("Main", flags)) {
            // TODO: change color when is too fps is too low...
            ImGui::Text("(%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());

            show_target_rule(rule, recorder);
            ImGui::Separator();

            if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (auto child = imgui_childwindow("Rul")) {
                    // TODO: let lock be dealt with like rule... (modifications are returned by value)
                    if (auto out = edit_rule(rule, locked, icons)) {
                        recorder.take(*out); // TODO: `edit_tile` uses the old rule at this frame...
                                             // does this matter?
                    }

                    // TODO: This is used to pair with enter key and is somewhat broken...
                    // TODO: should enter set_next first?
                    if (imgui_keypressed(ImGuiKey_Apostrophe, false)) {
                        recorder.set_prev();
                    }
                }
                ImGui::TableNextColumn();
                // TODO: it seems this childwindow is not necessary?
                if (auto child = imgui_childwindow("Til")) {
                    edit_tile(rule, locked, img);
                }
                ImGui::EndTable();
            }
        }

        logger::tempwindow();
        end_frame();
    }

    clear();
    return 0;
}

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;

static void init() {
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
    window = SDL_CreateWindow("Rule editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
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
}

static void clear() {
    assert(window && renderer);

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    window = nullptr;
    renderer = nullptr;
}

static bool begin_frame() {
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

    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    return true;
}

static void end_frame() {
    assert(window && renderer);

    ImGui::Render();
    // TODO: is this necessary?
    ImGuiIO& io = ImGui::GetIO();
    SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    // SDL_RenderClear(renderer); // TODO: really ignorable? (the app uses a full-screen window)
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(renderer);
}

static SDL_Texture* create_texture(SDL_PixelFormatEnum format, SDL_TextureAccess access, int w, int h) {
    assert(window && renderer);

    SDL_Texture* texture = SDL_CreateTexture(renderer, format, access, w, h);
    if (!texture) {
        printf("Error: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    return texture;
}

// TODO: temp...
static SDL_Texture* cast(ImTextureID texture) { //
    return static_cast<SDL_Texture*>(texture);
}

ImTextureID tile_image::update(const legacy::tileT& tile) {
    if (!m_texture || m_w != tile.width() || m_h != tile.height()) {
        if (m_texture) {
            SDL_DestroyTexture(cast(m_texture));
        }

        m_w = tile.width();
        m_h = tile.height();
        // TODO: pixel format might be problematic...
        // ~ To make IM_COL32 work, what format should be specified?
        m_texture = create_texture(SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, m_w, m_h);
    }

    void* pixels = nullptr;
    int pitch = 0;
    // TODO: when will pitch != m_w * sizeof(Uint32)?
    const bool succ = SDL_LockTexture(cast(m_texture), nullptr, &pixels, &pitch) == 0;
    if (!succ) {
        printf("Error: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    tile.for_each_line(tile.range(), [&](int y, std::span<const bool> line) {
        Uint32* p = (Uint32*)((char*)pixels + pitch * y);
        for (bool v : line) {
            *p++ = v ? IM_COL32_WHITE : IM_COL32_BLACK;
        }
    });

    SDL_UnlockTexture(cast(m_texture));

    return m_texture;
}

tile_image::~tile_image() {
    if (m_texture) {
        SDL_DestroyTexture(cast(m_texture));
    }
}

code_image::code_image() {
    const int width = 3, height = 3 * 512;
    m_texture = create_texture(SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, height);
    // Uint32 pixels[512][3][3]; // "Function uses XXX bytes of stack"
    std::unique_ptr<Uint32[][3][3]> pixels(new Uint32[512][3][3]);
    for_each_code(code) {
        const auto [q, w, e, a, s, d, z, x, c] = legacy::decode(code);
        const bool fill[3][3] = {{q, w, e}, {a, s, d}, {z, x, c}};
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                pixels[code][y][x] = fill[y][x] ? IM_COL32_WHITE : IM_COL32_BLACK;
            }
        }
    }

    SDL_UpdateTexture(cast(m_texture), nullptr, pixels.get(), width * sizeof(Uint32));
}

code_image::~code_image() { SDL_DestroyTexture(cast(m_texture)); }
