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
#include "rule_traits.hpp"

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

// Unfortunately, SDL2-renderer backend doesn't support docking features...
// https://github.com/ocornut/imgui/issues/5835

// TODO: apply in several other places...
template <class F>
struct [[nodiscard]] scope_guard {
    F f;
    scope_guard(F&& f) : f(std::move(f)) {}
    ~scope_guard() { f(); }
    scope_guard(const scope_guard&) = delete;
    scope_guard& operator=(const scope_guard&) = delete;
};

// TODO: awful... need to be avoided...
template <class Enum>
auto underlying_address(Enum& e) {
    return reinterpret_cast<std::underlying_type_t<Enum>*>(std::addressof(e));
}

// TODO: move the check elsewhere.
// TODO: "bytes_equal" is not needed elsewhere, remove it.
// C/C++ - command line - /utf-8
// and save file as utf8-encoding...
template <class T, class U>
constexpr bool bytes_equal(const T& t, const U& u) noexcept {
    if constexpr (sizeof(t) != sizeof(u)) {
        return false;
    } else {
        using A = std::array<std::byte, sizeof(t)>;
        return std::bit_cast<A>(t) == std::bit_cast<A>(u);
    }
}

// "aaaaa" and "bbbbb" are workarounds for a compiler bug in clang...
// https://github.com/llvm/llvm-project/issues/63686
constexpr char aaaaa[]{"中文"};
constexpr char8_t bbbbb[]{u8"中文"};
static_assert(bytes_equal(aaaaa, bbbbb));

// TODO: clumsy...
// TODO: should be a class... how to decouple? ...
// TODO: for "paired", support 4-step modification (_,S,B,BS)... add new color?
void edit_rule(const char* id_str, bool* p_open, const legacy::ruleT& to_edit, code_image& icons,
               rule_recorder& recorder) {
    if (imgui_window window(id_str, p_open, ImGuiWindowFlags_AlwaysAutoResize); window) {
        ImGui::TextUnformatted("Current rule:");
        {
            auto rule_str = to_MAP_str(to_edit);
            imgui_strwrapped(rule_str);

            static int cpy = 0, pst = 0; // TODO: whatever the form is, should have visible effect.
            if (ImGui::Button("Copy to clipboard")) {
                ImGui::SetClipboardText(rule_str.c_str());
                // logger::log("Copied");
                cpy = 15;
            }

            ImGui::SameLine();
            if (ImGui::Button("Paste")) {
                // TODO: can text return nullptr?
                if (const char* text = ImGui::GetClipboardText()) {
                    auto rules = extract_rules(text);
                    if (!rules.empty()) {
                        int size = recorder.size();
                        recorder.append(rules); // TODO: requires non-trivial append logic.
                        recorder.set_pos(size);
                        // logger::log("Pasted {}", rules.size());
                        pst = 15;
                    }
                }
            }

            if (cpy > 0) {
                --cpy;
                ImGui::SameLine();
                ImGui::TextUnformatted("Copied");
            }
            if (pst > 0) {
                --pst;
                ImGui::SameLine();
                ImGui::TextUnformatted("Pasted");
            }
            // TODO: re-implement
            // ImGui::SameLine();
            // if (ImGui::Button("Save to file")) {
            //     logger::log("Saved");
            // }
        }

        using legacy::partitionT;

        static partitionT::basespecE base = partitionT::Spatial;
        static partitionT::extrspecE extr = partitionT::None_;
        static legacy::interT inter = {};

        ImGui::SeparatorText("Settings");
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Base");
            ImGui::SameLine();
            ImGui::Combo("##MainMode", underlying_address(base), partitionT::basespecE_names,
                         partitionT::basespecE_size);

            // TODO: use extra_specification_names...
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Extr");
            ImGui::SameLine();
            ImGui::RadioButton("none", underlying_address(extr), partitionT::None_);
            ImGui::SameLine();
            // TODO: support 4-state modification when paired...
            ImGui::RadioButton("paired", underlying_address(extr), partitionT::Paired);
            ImGui::SameLine();
            // TODO: explain...
            ImGui::RadioButton("state", underlying_address(extr), partitionT::State);

            ImGui::Text("Groups: %d", partitionT::getp(base, extr).k()); // TODO: use variable "part"?
            ImGui::Separator();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Inter"); // TODO: suitable name...
            ImGui::SameLine();
            // TODO: might be better named "direct"
            ImGui::RadioButton("Val", underlying_address(inter.tag), inter.Value);
            ImGui::SameLine();
            ImGui::RadioButton("Flp", underlying_address(inter.tag), inter.Flip);
            ImGui::SameLine();
            ImGui::RadioButton("Dif", underlying_address(inter.tag), inter.Diff);
            if (inter.tag == inter.Diff) {
                ImGui::SameLine();
                if (ImGui::Button("Take current")) {
                    inter.custom = to_edit;
                }
                imgui_strwrapped(to_MAP_str(inter.custom));
            }
            // TODO: how to keep state-symmetry in Diff mode?
            ImGui::Text("State symmetry: %d", (int)legacy::state_symmetric(to_edit));
            ImGui::SameLine();
            if (ImGui::SmallButton("details")) {
                extr = partitionT::State;
                inter.tag = inter.Flip;
            }
        }

        const auto& part = partitionT::getp(base, extr);
        const int k = part.k();
        legacy::ruleT_data rule = inter.from_rule(to_edit);

        // TODO: should be foldable; should be able to set max height...
        ImGui::SeparatorText("Rule details");
        {
            {
                // TODO: unstable between base/extr switchs; ratio-based approach is on-trivial though... (double has
                // inaccessible values)
                static int rcount = 0.3 * k;
                rcount = std::clamp(rcount, 0, k);

                ImGui::SliderInt("##Active", &rcount, 0, k, "%d", ImGuiSliderFlags_NoInput);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
                ImGui::PushButtonRepeat(true);
                const float r = ImGui::GetFrameHeight();
                ImGui::SameLine();
                if (ImGui::Button("-", ImVec2(r, r))) {
                    rcount = std::max(0, rcount - 1);
                }
                ImGui::SameLine();
                if (ImGui::Button("+", ImVec2(r, r))) {
                    rcount = std::min(k, rcount + 1);
                }
                ImGui::PopButtonRepeat();
                ImGui::PopStyleVar();

                ImGui::SameLine();
                if (ImGui::Button("Randomize")) {
                    // TODO: use mt19937 instead?
                    static std::mt19937_64 rand(time(0));
                    // TODO: looks bad.
                    legacy::ruleT_data grule{};
                    random_fill(grule.data(), grule.data() + k, rcount, rand);
                    rule = part.dispatch_from(grule);
                    recorder.take(inter.to_rule(rule));
                }

                // TODO: experimental; not suitable place... should be totally redesigned...
                // Flip each group; the result is [actually] independent of inter.
                if (ImGui::Button("Flip each group")) {
                    std::vector<legacy::compressT> vec;
                    vec.emplace_back(inter.to_rule(rule));
                    for (const auto& group : part.groups()) {
                        legacy::ruleT_data r = rule;
                        part.flip(group, r);
                        vec.emplace_back(inter.to_rule(r));
                    }
                    recorder.replace(std::move(vec));
                }
            }

            const auto scans = part.scan(rule);
            ImGui::Text("[1:%d] [0:%d] [x:%d]", scans.count(scans.All_1), scans.count(scans.All_0),
                        scans.count(scans.Inconsistent));

            const ImVec2 icon_size(3 * 7, 3 * 7); // zoom = 7.
            const int perline = 8;
            const int lines = 7;
            const float child_height = lines * (21 /*r*/ + 4 /*padding.y*2*/) + (lines - 1) * 4 /*spacing.y*/;
            // TOOD: this should always return true...
            if (imgui_childwindow child("Details", ImVec2(390, child_height)); child) {
                using legacy::codeT;

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                for (int j = 0; j < k; ++j) {
                    if (j % perline != 0) {
                        ImGui::SameLine();
                    }
                    if (j != 0 && j % (perline * lines) == 0) {
                        ImGui::Separator(); // TODO: refine...
                    }
                    const bool inconsistent = scans[j] == scans.Inconsistent;
                    const auto& group = part.groups()[j];

                    if (inconsistent) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6, 0, 0, 1));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0, 0, 1));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9, 0, 0, 1));
                    }
                    // TODO: "to_string(j).c_str()" looks extremely ugly.
                    if (ImGui::ImageButton(std::to_string(j).c_str(), icons.texture(), icon_size,
                                           ImVec2(0, group[0] * (1.0f / 512)),
                                           ImVec2(1, (group[0] + 1) * (1.0f / 512)))) {
                        // TODO: document this behavior... (keyctrl->resolve conflicts)
                        if (ImGui::GetIO().KeyCtrl) {
                            const bool b = !rule[group[0]];
                            for (codeT code : group) {
                                rule[code] = b;
                            }
                        } else {
                            part.flip(group, rule);
                        }

                        recorder.take(inter.to_rule(rule));

                        // TODO: TextUnformatted(strs[scans[j]]) may yield false result at this frame, but is
                        // negligible...
                    }
                    if (inconsistent) {
                        ImGui::PopStyleColor(3);
                    }

                    if (imgui_itemtooltip tooltip; tooltip) {
                        for (int x = 0; codeT code : group) {
                            if (x++ % perline != 0) { // TODO: sharing the same perline (not necessary)
                                ImGui::SameLine();
                            }

                            // TODO: use the same bordercol as button's?
                            // TODO: icons should provide codeT methods...
                            ImGui::Image(icons.texture(), icon_size, ImVec2(0, code * (1.0f / 512)),
                                         ImVec2(1, (code + 1) * (1.0f / 512)), ImVec4(1, 1, 1, 1),
                                         ImVec4(0.5, 0.5, 0.5, 1));
                            ImGui::SameLine();
                            ImGui::AlignTextToFramePadding();
                            ImGui::TextUnformatted(rule[code] ? ":1" : ":0");
                        }
                    }

                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    static const char* const strs[]{":x", ":0", ":1"};
                    ImGui::TextUnformatted(strs[scans[j]]); // TODO: is ":x" useless?
                }
                ImGui::PopStyleVar(2);
            }
        }
    }
}

// TODO: refine...
// TODO: able to create/append/open file?
struct file_navT {
    using path = std::filesystem::path;

    static path exe_path() {
        // TODO: use scope_guard?
        // TODO: the program should be allowed to create folders in the exe path.(SDL_GetBasePath(), instead of
        // PrefPath)
        // this is a part runtime [dependency]. (btw, where to put imgui config?)
        // The correctness of "pos = base.get()" relies on default coding being utf-8 (asserted; TODO: list all
        // utf8 dependencies)
        const std::unique_ptr<char[], decltype(+SDL_free)> p(SDL_GetBasePath(), SDL_free);
        assert(p); // TODO: what if this fails? what if exe path is invalid?
        return p.get();
    }

    path pos;
    // TODO: rename class name.
    // TODO: explain why cache `status`.
    // TODO: cache more values?
    struct entry {
        std::filesystem::directory_entry entry;
        std::filesystem::file_status status;
    };
    std::vector<entry> list;
    std::optional<std::string> broken = std::nullopt;
    int refresh = 20;

    void refr() noexcept {
        if (!broken) {
            if (--refresh <= 0) {
                set_pos(std::move(pos));
            }
        }
    }

    void set_pos(path&& p) noexcept {
        try {
            if (std::filesystem::exists(p)) {
                pos = std::filesystem::canonical(p);
                // TODO: what if too many entries?
                list.clear();
                for (const auto& entry : std::filesystem::directory_iterator(pos)) {
                    list.emplace_back(entry, entry.status());
                }
            }
        } catch (const std::exception& what) {
            // TODO: what encoding?
            broken = what.what();
            return;
        }
        broken.reset();
        refresh = 20;
    }

    file_navT(path p = exe_path()) { set_pos(std::move(p)); }

    [[nodiscard]] std::optional<path> show(const char* suff = nullptr) {
        using namespace std;
        using namespace std::filesystem;

        optional<path> selfile = nullopt;

        try {
            // TODO: enhance filter...
            // TODO: add text input.
            // TODO: undo operation...
            // TODO: relying on default encoding being utf-8...
            // TODO: how to decide the default encoding of MB str?
            ImGui::TextUnformatted(pos.string().c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("-> Exe path")) {
                set_pos(exe_path());
            }
            if (ImGui::MenuItem("-> Cur path")) {
                set_pos(current_path());
            }
            // TODO: the correctness is doubtful here...
            if (path par = pos.parent_path(); ImGui::MenuItem("-> ..", nullptr, nullptr, par != pos)) {
                set_pos(std::move(par));
            }
            ImGui::Separator();

            if (imgui_childwindow child("child"); child) {
                if (broken) {
                    // TODO: what's the encoding of exception.what()?
                    // TODO: wrapped&&disabled...
                    imgui_strdisabled(broken->c_str());
                } else {
                    refr();

                    int entries = 0;
                    optional<path> selfolder = nullopt;
                    for (const auto& [entry, status] : list) {
                        // TODO: how to compare file extension without creating a real string?
                        const auto str = entry.path().filename().string();

                        if (is_regular_file(status)) {
                            if (!suff || str.ends_with(suff)) {
                                ++entries;
                                // TODO: want double click; how?
                                // TODO: selected...
                                if (ImGui::Selectable(str.c_str())) {
                                    selfile = entry.path();
                                }
                            }
                        } else if (is_directory(status)) {
                            ++entries;
                            if (ImGui::MenuItem(str.c_str(), "dir")) {
                                selfolder = entry.path();
                                // Not breaking eagerly to avoid flicking...
                            }
                        }
                    }
                    if (!entries) {
                        // TODO: better msg...
                        imgui_strdisabled("None");
                    }
                    if (selfolder) {
                        set_pos(std::move(*selfolder));
                    }
                }
            }
        } catch (const exception& what) {
            // TODO: what encoding?
            broken = what.what();
        }

        return selfile;
    }

    [[nodiscard]] std::optional<path> window(const char* id_str, bool* p_open, const char* suff = nullptr) {
        if (imgui_window window(id_str, p_open); window) {
            return show(suff);
        }
        return std::nullopt;
    }
};

// TODO: should enable guide-mode (with a switch...)
// TODO: changes upon the same rule should be grouped together... how? (editor++)...
// TODO: whether to support seed-saving? some patterns are located in certain seed states...
// (maybe a lot less useful than pattern saving)
// TODO: how to capture certain patterns? (editor++)...

int main(int argc, char** argv) {
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // IME: "Input Method Editor"
    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with SDL_Renderer graphics context
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    // TODO: should be able to control framerate (should not rely on VSYNC rate)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        SDL_Log("Error creating SDL_Renderer!");
        return 0;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // TODO: regulate lambda capturing...
    scope_guard cleanup([=] {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    });

    // Program logic:
    ImGuiIO& io = ImGui::GetIO();
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // TODO: ... works but blurry, and how to apply in project?
    // const char* fnpath = R"(C:\*redacted*\Desktop\Deng.ttf)";
    // io.Fonts->AddFontFromFileTTF(fnpath, 13, nullptr, io.Fonts->GetGlyphRangesChineseFull());

    logger::log("Entered main");

    rule_recorder recorder;
    // TODO: what encoding?
    if (argc == 2) {
        recorder.replace(read_rule_from_file(argv[1]));
    }

    tile_filler filler{.seed = 0, .density = 0.5};
    rule_runner runner({.width = 320, .height = 240});
    runner.restart(filler);

    legacy::ruleT rule = recorder.current();

    // TODO: combine to tile_runner...
    constexpr int pergen_min = 1, pergen_max = 20;
    int pergen = 1;
    bool anti_flick = true; // TODO: add explanation
    auto actual_pergen = [&]() {
        if (anti_flick && legacy::will_flick(rule) && pergen % 2) {
            return pergen + 1;
        }
        return pergen;
    };

    constexpr int start_min = 0, start_max = 1000;
    int start_from = 0;

    constexpr int gap_min = 0, gap_max = 20;
    int gap_frame = 0;

    bool paused = false;

    auto run = [&](bool restart) {
        if (rule != recorder.current()) {
            rule = recorder.current();
            restart = true;
        }
        if (restart) {
            runner.restart(filler);
        }
        if (runner.gen() < start_from) {
            runner.run(rule, start_from - runner.gen());
        } else if (!paused) {
            if (ImGui::GetFrameCount() % (gap_frame + 1) == 0) {
                runner.run(rule, actual_pergen());
            }
        }
    };

    bool show_rule_editor = true;
    bool show_demo_window = false; // TODO: remove this in the future...
    bool show_log_window = true;
    bool show_nav_window = true;
    file_navT nav(R"(C:\*redacted*\Desktop\rulelists_new)");

    // Main loop
    tile_image img(renderer, runner.tile());
    code_image icons(renderer);
    // TODO: cannot break eagerly?
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
            // TODO: this appears not needed:
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                done = true;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        scope_guard endframe([renderer, &io]() {
            // Rendering
            ImGui::Render();
            SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
            // ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
            // SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255),
            //                        (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
            SDL_RenderPresent(renderer);
        });

        // Frame state:
        // TODO: applying following logic; consider refining it.
        // recorder is modified during display, but will synchronize with runner's before next frame.
        assert(rule == recorder.current());
        bool should_restart = false;

        // TODO: remove this when all done...
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
        }
        if (show_log_window) {
            logger::window("Events", &show_log_window);
        }
        if (show_rule_editor) {
            edit_rule("Rule editor", &show_rule_editor, rule, icons, recorder);
        }
        if (show_nav_window) {
            if (auto sel = nav.window("File Nav", &show_nav_window, ".txt")) {
                logger::log("Tried to open {}", sel->filename().string());
                // TODO: is string() encoding-safe for fopen()?
                // TODO: use ifstream(path()) instead?
                auto result = read_rule_from_file(sel->string().c_str());
                if (!result.empty()) {
                    // TODO: "append" is a nasty feature...
                    logger::append(" ~ found {} rules", result.size());
                    recorder.replace(std::move(result));
                } else {
                    logger::append(" ~ found nothing");
                }
            }
        }

        // TODO: rename...
        if (imgui_window window("##Temp", ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize); window) {
            ImGui::Text("(%.1f FPS) Frame:%d\n", io.Framerate, ImGui::GetFrameCount());
            ImGui::Separator();

            ImGui::Checkbox("Log window", &show_log_window);
            ImGui::SameLine();
            ImGui::Checkbox("Rule editor", &show_rule_editor);
            ImGui::SameLine();
            ImGui::Checkbox("Nav window", &show_nav_window);

            ImGui::Checkbox("Demo window", &show_demo_window);
        }

        if (imgui_window window("Tile", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize); window) {
            ImGui::Text("Gen:%d", runner.gen());
            ImGui::Text("Width:%d,Height:%d,Density:%f", runner.tile().width(), runner.tile().height(),
                        float(runner.tile().count()) / runner.tile().area());
            {
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImVec2 img_size = ImVec2(img.width(), img.height()); // TODO: support zooming?

                const ImVec2 padding(2, 2);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, padding);
                // img.update(runner.tile());
                ImGui::ImageButton("##Tile", img.texture(), img_size);
                ImGui::PopStyleVar();

                static bool spaused = false;
                if (ImGui::IsItemActivated()) {
                    spaused = paused;
                    paused = true;
                }
                if (ImGui::IsItemDeactivated()) {
                    paused = spaused;
                }
                if (ImGui::IsItemHovered()) {
                    if (ImGui::IsItemActive()) {
                        if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) {
                            runner.shift(io.MouseDelta.x, io.MouseDelta.y);
                        }
                    } else {
                        if (io.MouseWheel < 0) { // scroll down
                            recorder.next();
                        } else if (io.MouseWheel > 0) { // scroll up
                            recorder.prev();
                        }
                        // TODO: reconsider whether/where to support next/prev actions
                    }
                    static bool show_zoom = true; // TODO: should be here?
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        // TODO: how to support other operations?
                        show_zoom = !show_zoom;
                    }
                    if (imgui_itemtooltip tooltip(show_zoom); tooltip) {
                        assert(ImGui::IsMousePosValid());
                        const float region_sz = 32.0f;
                        float region_x = std::clamp(io.MousePos.x - (pos.x + padding.x) - region_sz * 0.5f, 0.0f,
                                                    img_size.x - region_sz);
                        float region_y = std::clamp(io.MousePos.y - (pos.y + padding.y) - region_sz * 0.5f, 0.0f,
                                                    img_size.y - region_sz);

                        const ImVec2 uv0 = ImVec2((region_x) / img_size.x, (region_y) / img_size.y);
                        const ImVec2 uv1 =
                            ImVec2((region_x + region_sz) / img_size.x, (region_y + region_sz) / img_size.y);
                        const float zoom = 4.0f; // TODO: should be settable? 5.0f?
                        ImGui::Image(img.texture(), ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1);
                    }
                }

                // TODO: reconsider this design...
                // put off to here, for "runner.shift(io.MouseDelta.x, io.MouseDelta.y);".
                // works fine as texture() (pointer) is unchanged by update, and the rendering happens very later...
                img.update(runner.tile());

                // It seems the whole recorder model is problematic...
                ImGui::AlignTextToFramePadding(); // TODO: +1 is clumsy.
                ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1);
                // TODO: pos may not reflect runner's real pos, as recorder can be modified on the way... may not
                // matters

                static char go_to[20]{};
                const auto filter = [](ImGuiInputTextCallbackData* data) {
                    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
                };
                ImGui::SameLine();
                if (ImGui::InputTextWithHint(
                        "##Goto", "GOTO e.g. 2->enter", go_to, 20,
                        ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue, filter)) {
                    int val{};
                    if (std::from_chars(go_to, go_to + strlen(go_to), val).ec == std::errc{}) {
                        recorder.set_pos(val - 1); // TODO: -1 is clumsy.
                    }
                    go_to[0] = '\0';

                    // Regain focus:
                    ImGui::SetKeyboardFocusHere(-1);
                }
            }

            // TODO: better layout
            if (ImGui::Button("Ctrl")) {
                ImGui::OpenPopup("Ctrl##0");
            }
            // TODO: class?
            if (ImGui::BeginPopup("Ctrl##0")) {
                // TODO: button <- set to 0.5?
                if (ImGui::SliderFloat("Init density [0-1]", &filler.density, 0.0f, 1.0f, "%.3f",
                                       ImGuiSliderFlags_NoInput)) {
                    should_restart = true;
                }

                ImGui::Checkbox("Pause", &paused);

                ImGui::SameLine();
                ImGui::PushButtonRepeat(true);
                if (ImGui::Button("+1")) {
                    runner.run(rule, 1); // TODO: should run be called here?
                    // TODO: should have visible (text) effect even when not paused...
                }
                ImGui::SameLine();
                if (ImGui::Button("+p")) {
                    runner.run(rule, actual_pergen());
                }
                ImGui::PopButtonRepeat();

                ImGui::SameLine();
                if (ImGui::Button("Restart") || imgui_keypressed(ImGuiKey_R, false)) {
                    should_restart = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Reseed") || imgui_keypressed(ImGuiKey_S, false)) {
                    ++filler.seed;
                    should_restart = true;
                }

                // TODO: toooo ugly...
                ImGui::SliderInt("Pergen [1-20]", &pergen, pergen_min, pergen_max, "%d", ImGuiSliderFlags_NoInput);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("(Actual pergen: %d)", actual_pergen());
                ImGui::SameLine();
                ImGui::Checkbox("anti-flick", &anti_flick);

                ImGui::SliderInt("Gap Frame [0-20]", &gap_frame, gap_min, gap_max, "%d", ImGuiSliderFlags_NoInput);
                ImGui::SliderInt("Start gen [0-1000]", &start_from, start_min, start_max, "%d",
                                 ImGuiSliderFlags_NoInput);

                // TODO: enable/disable keyboard ctrl (enable by default)
                ImGui::EndPopup();
            }

            // TODO: redesign keyboard ctrl...
            if (imgui_keypressed(ImGuiKey_1, true)) {
                gap_frame = std::max(gap_min, gap_frame - 1);
            }
            if (imgui_keypressed(ImGuiKey_2, true)) {
                gap_frame = std::min(gap_max, gap_frame + 1);
            }
            if (imgui_keypressed(ImGuiKey_3, true)) {
                pergen = std::max(pergen_min, pergen - 1);
            }
            if (imgui_keypressed(ImGuiKey_4, true)) {
                pergen = std::min(pergen_max, pergen + 1);
            }
            if (imgui_keypressed(ImGuiKey_Space, true)) {
                if (!paused) {
                    paused = true;
                } else {
                    runner.run(rule, actual_pergen()); // ?run(1)
                    // TODO: whether to take place after set_rule?
                    // TODO: whether to update image this frame?
                }
            }
            if (imgui_keypressed(ImGuiKey_M, false)) {
                paused = !paused;
            }
        }

        // TODO: should this be put before begin-frame?
        run(should_restart);
    }

    return 0;
}
