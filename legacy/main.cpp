// TODO: not used in this source file...
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

// Unfortunately, SDL2-renderer backend doesn't support docking features...
// https://github.com/ocornut/imgui/issues/5835

#include <SDL.h>

#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "app.hpp"

[[noreturn]] static void resource_failure() {
    printf("Error: %s", SDL_GetError());
    exit(EXIT_FAILURE);
}

static void init();
static void clear();
static bool begin_frame();
static void end_frame();

// TODO: Right-click must either to open a submenu, or to toggle on/off the tooltip.

// Never empty.
class recorderT {
    std::vector<legacy::compressT> m_record;
    int m_pos;

public:
    recorderT() {
        m_record.push_back(legacy::compress({legacy::game_of_life()}));
        m_pos = 0;
    }

    int size() const { return m_record.size(); }

    // [0, size() - 1]
    int pos() const { return m_pos; }

    void update(const legacy::moldT& mold) {
        const legacy::compressT cmpr = legacy::compress(mold);
        if (cmpr != m_record[m_pos]) {
            m_record.push_back(cmpr);
            set_last();
        }
    }

    legacy::moldT current() const {
        assert(m_pos >= 0 && m_pos < size());
        return legacy::decompress(m_record[m_pos]);
    }

    void set_next() { set_pos(m_pos + 1); }
    void set_prev() { set_pos(m_pos - 1); }
    void set_first() { set_pos(0); }
    void set_last() { set_pos(size() - 1); }

private:
    void set_pos(int pos) { //
        m_pos = std::clamp(pos, 0, size() - 1);
    }
};

// The encoding of `argv` cannot be relied upon, see:
// https://stackoverflow.com/questions/5408730/what-is-the-encoding-of-argv
int main(int, char**) {
    init();
    {
        char* base_path = SDL_GetBasePath();
        if (!base_path) {
            resource_failure();
        }

        file_nav_add_special_path(base_path, "Exe path");
        // TODO: remove when finished...
        file_nav_add_special_path(R"(C:\*redacted*\Desktop\rulelists_new)", "Temp");

        const std::string path = base_path;
        SDL_free(base_path);

        const auto strdup = [](const std::string& str) {
            char* buf = new char[str.size() + 1];
            std::copy_n(str.c_str(), str.size() + 1, buf);
            return buf;
        };

        // Freeze the absolute path of "imgui.ini" and "imgui_log.txt".
        // (wontfix) These memory leaks are negligible.
        assert(path.ends_with('\\') || path.ends_with('/'));
        ImGui::GetIO().IniFilename = strdup(path + "imgui.ini");
        ImGui::GetIO().LogFilename = strdup(path + "imgui_log.txt");
    }

    // TODO: Currently the controls of the program are poorly designed, and are especially not taking
    // navigation mode into consideration...
    assert(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard));
    assert(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad));

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // TODO: works but blurry, and how to apply in project?
    // ImGui::GetIO().Fonts->AddFontFromFileTTF(R"(C:\*redacted*\Desktop\Deng.ttf)", 13, nullptr,
    //                                          ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());

    recorderT recorder;

    // TODO: dtors(SDL_DestroyTexture) are being called after clear(SDL_Quit)
    code_image icons;
    tile_image img;

    while (begin_frame()) {
        ImGui::ShowDemoWindow(); // TODO: remove (or comment-out) this when all done...

        legacy::moldT current = recorder.current();
        bool update = false;

        static bool load = true;
        if (load) {
            // TODO: this should be controlled by load_rule ...
            ImGui::SetNextWindowSize({600, 400}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200), ImVec2(FLT_MAX, FLT_MAX));
            if (auto window = imgui_window("Load rule", &load, ImGuiWindowFlags_NoCollapse)) {
                if (auto out = load_rule(current)) {
                    current = *out;
                    update = true;
                }
            }
        }

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        if (auto window = imgui_window("Main", flags)) {
            ImGui::Text("(%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());

            // TODO: as `current` may have been changed by static_constraints, `current` may have been out-of-sync with
            // recorder at this frame... Does this matter?
            {
                const std::string rule_str = legacy::to_MAP_str(current.rule);

                // TODO: temp...
                if (ImGui::Button("Copy&lock")) {
                    ImGui::SetClipboardText(legacy::to_MAP_str(current).c_str());
                }
                ImGui::SameLine();
                // TODO: better gui logic for copy... find ways to show feedback
                if (ImGui::Button("Copy")) {
                    ImGui::SetClipboardText(rule_str.c_str());
                }
                ImGui::SameLine();
                // TODO: (temp) added back; remove when pasting is supported by load_rule...
                if (ImGui::Button("Paste")) {
                    if (const char* str = ImGui::GetClipboardText()) {
                        if (auto out = legacy::extract_MAP_str(std::string_view(str)).mold) {
                            current = *out;
                            update = true;
                        }
                    }
                }

                if (!load) {
                    ImGui::SameLine();
                    if (ImGui::Button("Load")) {
                        load = true;
                    }
                }

                imgui_str(rule_str);

                // TODO: +1 is clumsy
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1);
                ImGui::SameLine();
                iter_pair(
                    "<|", "prev", "next", "|>", //
                    [&] { recorder.set_first(); }, [&] { recorder.set_prev(); }, [&] { recorder.set_next(); },
                    [&] { recorder.set_last(); });
            }

            ImGui::Separator();

            if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (auto child = imgui_childwindow("Rul")) {
                    if (auto out = edit_rule(current, icons)) {
                        current = *out;
                        update = true;
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
                    if (auto out = edit_tile(current.rule, img)) {
                        current.lock = *out;
                        update = true;
                    }
                }
                ImGui::EndTable();
            }
        }

        if (update) {
            recorder.update(current);
        }

        logger::display();
        end_frame();

        {
            // Added as an extra assurance for the framerate.
            // (Normally `SDL_RENDERER_PRESENTVSYNC` is enough to guarantee a moderate framerate.)
            static Uint64 next = 0;
            const Uint64 now = SDL_GetTicks64();
            if (now < next) {
                SDL_Delay(next - now);
            }
            next = SDL_GetTicks64() + 10;
        }
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
        resource_failure();
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
        resource_failure();
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        resource_failure();
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
        resource_failure();
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
        resource_failure();
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
        const legacy::envT env = legacy::decode(code);
        const bool fill[3][3] = {{env.q, env.w, env.e}, {env.a, env.s, env.d}, {env.z, env.x, env.c}};
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                pixels[code][y][x] = fill[y][x] ? IM_COL32_WHITE : IM_COL32_BLACK;
            }
        }
    }

    SDL_UpdateTexture(cast(m_texture), nullptr, pixels.get(), width * sizeof(Uint32));
}

code_image::~code_image() { SDL_DestroyTexture(cast(m_texture)); }
