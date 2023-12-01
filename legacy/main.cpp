#include "app_sdl.hpp"

#include "app.hpp"
#include "rule_traits.hpp"

namespace legacy {
    // TODO: proper name...
    // TODO: not using mkrule; tends to be very obscure...
    ruleT mirror(const ruleT& rule) {
        ruleT mir{};
        for (codeT code : codeT{}) {
            codeT codex = flip_all(code);
            bool flip = decode_s(codex) != rule(codex);
            if (flip) {
                mir.map[code] = !decode_s(code);
            } else {
                mir.map[code] = decode_s(code);
            }
        }
        return mir;
    }

    // TODO: flip is able to pass through interT (flip on group directly)...
    void flip_inplace(const groupT& group, ruleT_data& rule) {
        for (codeT code : group) {
            rule[code] = !rule[code];
        }
    }

    void set_inplace(const groupT& group, ruleT_data& rule, bool b) {
        for (codeT code : group) {
            rule[code] = b;
        }
    }

} // namespace legacy

// TODO: should be a class... how to decouple? ...
void edit_rule(const char* id_str, bool* p_open, const legacy::ruleT& target, const code_image& icons,
               rule_recorder& recorder) {
    if (auto window = imgui_window(id_str, p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto display_target = [&target, &recorder] {
            std::string rule_str = to_MAP_str(target);

            ImGui::AlignTextToFramePadding();
            imgui_str("[Current rule]");
            ImGui::SameLine();
            if (ImGui::Button("Copy")) {
                ImGui::SetClipboardText(rule_str.c_str());
                logger::log_temp(300ms, "Copied");
            }
            ImGui::SameLine();
            if (ImGui::Button("Paste")) {
                if (const char* text = ImGui::GetClipboardText()) {
                    auto rules = extract_rules(text);
                    if (!rules.empty()) {
                        // TODO: redesign recorder... whether to accept multiple rules?
                        // TODO: target??
                        if (target != rules.front()) {
                            recorder.take(rules.front());
                        } else {
                            logger::log_temp(300ms, "Same rule");
                        }
                    }
                }
            }
            // TODO: re-implement file-saving
            imgui_strwrapped(rule_str);
        };

        // TODO: explain...
        // TODO: for "paired", support 4-step modification (_,S,B,BS)... add new color?
        // TODO: why does clang-format sort using clauses?
        using legacy::interT;
        using legacy::partitionT;
        static partitionT::basespecE base = partitionT::Spatial;
        static partitionT::extrspecE extr = partitionT::None_;
        static interT inter = {};

        const auto set_base_extr = [] {
            int ibase = base;
            int iextr = extr;

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Base");
            ImGui::SameLine();
            ImGui::Combo("##MainMode", &ibase, partitionT::basespecE_names, partitionT::basespecE_size);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Extr");
            ImGui::SameLine();
            ImGui::RadioButton("none", &iextr, partitionT::None_);
            ImGui::SameLine();
            ImGui::RadioButton("paired", &iextr, partitionT::Paired);
            ImGui::SameLine();
            ImGui::RadioButton("state", &iextr, partitionT::State);

            base = partitionT::basespecE{ibase};
            extr = partitionT::extrspecE{iextr};
        };
        const auto set_inter = [&target] {
            int itag = inter.tag;
            const auto tooltip = [](interT::tagE tag, const char* msg = "View from:") {
                if (ImGui::BeginItemTooltip()) {
                    imgui_str(msg);
                    ImGui::PushTextWrapPos(250); // TODO: how to decide wrap pos properly?
                    imgui_str(to_MAP_str(inter.get_viewer(tag)));
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            };

            // TODO: better name (e.g. might be better named "direct")
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("View"); // TODO: rename vars...
            ImGui::SameLine();
            ImGui::RadioButton("Val", &itag, inter.Value);
            tooltip(inter.Value);
            ImGui::SameLine();
            ImGui::RadioButton("Flp", &itag, inter.Flip);
            tooltip(inter.Flip);
            ImGui::SameLine();
            ImGui::RadioButton("Dif", &itag, inter.Diff);
            tooltip(inter.Diff);

            inter.tag = interT::tagE{itag};
            if (inter.tag == inter.Diff) {
                ImGui::SameLine();
                if (ImGui::Button("Take current")) {
                    inter.custom = target;
                }
                tooltip(inter.Diff);
            }
        };

        display_target();
        ImGui::SeparatorText("Settings");
        set_base_extr();
        ImGui::SeparatorText("Rule details");
        set_inter();

        // TODO: rename...
        const auto& part = partitionT::getp(base, extr);
        const int k = part.k();
        const legacy::ruleT_data drule = inter.from_rule(target);
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
                static std::mt19937 rand(time(0));
                // TODO: looks bad.
                legacy::ruleT_data grule{};
                random_fill_count(grule.data(), grule.data() + k, rcount, rand);
                recorder.take(inter.to_rule(part.dispatch_from(grule)));
            }
        }
        // TODO: redesign...
        if (false && ImGui::TreeNode("Misc")) {
            // TODO: should be redesigned...

            // TODO: incorrect place...
            // TODO: how to keep state-symmetry in Diff mode?
            ImGui::AlignTextToFramePadding();
            ImGui::Text("State symmetry: %d", (int)legacy::state_symmetric(target));
            ImGui::SameLine();
            if (ImGui::Button("Details")) {
                extr = partitionT::State;
                inter.tag = inter.Flip;
            }
            ImGui::SameLine();
            if (ImGui::Button("Mir")) {
                recorder.take(legacy::mirror(target));
            }

            // TODO: experimental; not suitable place... should be totally redesigned...
            // Flip each group; the result is actually independent of inter.
            ImGui::SameLine();
            if (ImGui::Button("Flip each group")) {
                std::vector<legacy::compressT> vec;
                // TODO: actually independent of inter.
                vec.emplace_back(inter.to_rule(drule));
                for (const auto& group : part.groups()) {
                    legacy::ruleT_data r = drule;
                    legacy::flip_inplace(group, r);
                    vec.emplace_back(inter.to_rule(r));
                }
                recorder.replace(std::move(vec));
                logger::log_temp(300ms, "...");
                // TODO: the effect is still obscure...
            }
            ImGui::TreePop();
        }
        {
            static const char* const strss[3][3]{{"-0", "-1", "-x"}, //
                                                 {"-.", "-f", "-x"},
                                                 {"-.", "-d", "-x"}};
            const auto strs = strss[inter.tag];

            // TODO: should be foldable; should be able to set max height...
            const legacy::scanlistT scans(part, drule);
            ImGui::Text("Groups:%d [%c:%d] [%c:%d] [%c:%d]", k, strs[0][1], scans.count(scans.A0), strs[1][1],
                        scans.count(scans.A1), strs[2][1], scans.count(scans.Inconsistent));

            const int zoom = 7;
            const int perline = 8;
            const int lines = 7;
            const float child_height = lines * (3 * zoom + 4 /*padding.y*2*/) + (lines - 1) * 4 /*spacing.y*/;
            if (auto child = imgui_childwindow("Details", ImVec2(390, child_height))) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                for (int j = 0; j < k; ++j) {
                    if (j % perline != 0) {
                        ImGui::SameLine();
                    }
                    if (j != 0 && j % (perline * lines) == 0) {
                        ImGui::SeparatorText(""); // TODO: refine...
                    }
                    const bool inconsistent = scans[j] == scans.Inconsistent;
                    const auto& group = part.groups()[j];
                    const legacy::codeT head = group[0];

                    if (inconsistent) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0, 0, 1));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0, 0, 1));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0, 0, 1));
                    }
                    const bool button = icons.button(head, zoom);
                    const bool hover = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);
                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(strs[drule[head]]);
                    if (inconsistent) {
                        ImGui::PopStyleColor(3);
                    }

                    static bool show_group = true;
                    if (hover) {
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                            show_group = !show_group;
                        }
                        if (show_group && ImGui::BeginTooltip()) {
                            for (int x = 0; auto code : group) {
                                if (x++ % perline != 0) { // TODO: sharing the same perline (not necessary)
                                    ImGui::SameLine();
                                }
                                // TODO: change color?
                                icons.image(code, zoom, ImVec4(1, 1, 1, 1), ImVec4(0.5, 0.5, 0.5, 1));
                                ImGui::SameLine();
                                ImGui::AlignTextToFramePadding();
                                ImGui::TextUnformatted(strs[drule[code]]);
                            }
                            ImGui::EndTooltip();
                        }
                    }
                    if (button) {
                        // TODO: document this behavior... (keyctrl->resolve conflicts)
                        legacy::ruleT_data r = drule;
                        if (ImGui::GetIO().KeyCtrl) {
                            const bool b = !drule[group[0]];
                            legacy::set_inplace(group, r, b);
                        } else {
                            // TODO: actually independent of inter.
                            legacy::flip_inplace(group, r);
                        }
                        recorder.take(inter.to_rule(r));
                    }
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
        const std::unique_ptr<char[], decltype(+SDL_free)> p(SDL_GetBasePath(), SDL_free);
        assert(p); // TODO: what if this fails? what if exe path is invalid?
                   // TODO: silence the warning. The deprecation is very stupid.
                   // TODO: start_lifetime (new(...)) as char8_t?
        return std::filesystem::u8path(p.get());
    }

    path pos;
    // TODO: explain why cache `status`.
    // TODO: cache more values?
    struct entryT {
        std::filesystem::directory_entry entry;
        std::filesystem::file_status status;
    };
    std::vector<entryT> list;
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
            // TODO: cooldown for file...
            ImGui::TextUnformatted(cpp17_u8string(pos).c_str());
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

            if (auto child = imgui_childwindow("Child")) {
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
                        // TODO: why does string() return non-utf8?
                        const auto str = cpp17_u8string(entry.path().filename());

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
        if (auto window = imgui_window(id_str, p_open)) {
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

struct runner_ctrl {
    legacy::ruleT rule;

    static constexpr int pergen_min = 1, pergen_max = 20;
    int pergen = 1;
    bool anti_flick = true; // TODO: add explanation
    int actual_pergen() const {
        if (anti_flick && legacy::will_flick(rule) && pergen % 2) {
            return pergen + 1;
        }
        return pergen;
    }

    static constexpr int start_min = 0, start_max = 1000;
    int start_from = 0;

    static constexpr int gap_min = 0, gap_max = 20;
    int gap_frame = 0;

    bool pause = false;

    bool spaused = false;
    // TODO: fragile...
    void push_pause(bool p) {
        spaused = pause;
        pause = p;
    }
    void pop_pause() { pause = spaused; }

    void run(torusT& runner, int extra = 0) const {
        if (runner.gen() < start_from) {
            runner.run(rule, start_from - runner.gen());
        } else {
            if (extra != 0) {
                runner.run(rule, extra);
                extra = 0;
            }
            if (!pause) {
                if (ImGui::GetFrameCount() % (gap_frame + 1) == 0) {
                    runner.run(rule, actual_pergen());
                }
            }
        }
    }
};

int main(int argc, char** argv) {
    // TODO: static object? contextT?
    const auto cleanup = app_backend::init();

    // Program logic:
    // ImGuiIO& io = ImGui::GetIO();
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // TODO: ... works but blurry, and how to apply in project?
    // const char* fnpath = R"(C:\*redacted*\Desktop\Deng.ttf)";
    // io.Fonts->AddFontFromFileTTF(fnpath, 13, nullptr, io.Fonts->GetGlyphRangesChineseFull());

    rule_recorder recorder;
    // TODO: what encoding?
    if (argc == 2) {
        recorder.replace(read_rule_from_file(argv[1]));
    }

    tile_filler filler{.seed = 0, .density = 0.5};
    torusT runner({.width = 320, .height = 240});
    runner.restart(filler);

    runner_ctrl ctrl{
        .rule = recorder.current(), .pergen = 1, .anti_flick = true, .start_from = 0, .gap_frame = 0, .pause = false};

    bool show_rule_editor = true;
    bool show_demo_window = false; // TODO: remove this in the future...
    bool show_log_window = false;  // TODO: less useful than thought...
    bool show_nav_window = true;
    file_navT nav(R"(C:\*redacted*\Desktop\rulelists_new)");

    // Main loop
    tile_image img(runner.tile());
    code_image icons;

    while (app_backend::process_events()) {
        const auto frame_guard = app_backend::new_frame();

        ImGuiIO& io = ImGui::GetIO();

        // TODO: applying following logic; consider refining it.
        // recorder is modified during display, but will synchronize with runner's before next frame.
        assert(ctrl.rule == recorder.current());

        // TODO: remove this when all done...
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
        }
        if (show_log_window) {
            logger::window("Events", &show_log_window);
        }
        logger::tempwindow();

        if (show_rule_editor) {
            edit_rule("Rule editor", &show_rule_editor, ctrl.rule, icons, recorder);
        }
        if (show_nav_window) {
            if (auto sel = nav.window("File Nav", &show_nav_window, ".txt")) {
                logger::log("Tried to open {}", cpp17_u8string(*sel));
                auto result = read_rule_from_file(*sel);
                if (!result.empty()) {
                    // TODO: "append" is a nasty feature...
                    logger::log_temp(500ms, "found {} rules", result.size());
                    recorder.replace(std::move(result));
                } else {
                    logger::log_temp(300ms, "found nothing");
                }
            }
        }

        // TODO: rename...
        if (auto window = imgui_window("##Temp", ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("(%.1f FPS) Frame:%d\n", io.Framerate, ImGui::GetFrameCount());
            ImGui::Separator();

            ImGui::Checkbox("Log window", &show_log_window);
            ImGui::SameLine();
            ImGui::Checkbox("Rule editor", &show_rule_editor);
            ImGui::SameLine();
            ImGui::Checkbox("Nav window", &show_nav_window);

            ImGui::Checkbox("Demo window", &show_demo_window);
        }

        if (auto window = imgui_window("Tile", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
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

                if (ImGui::IsItemActivated()) {
                    ctrl.push_pause(true);
                }
                if (ImGui::IsItemDeactivated()) {
                    ctrl.pop_pause();
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
                    if (show_zoom && ImGui::BeginItemTooltip()) {
                        assert(ImGui::IsMousePosValid());
                        // TODO: better size ctrl
                        static int region_rx = 32, region_ry = 32;
                        region_rx = std::clamp(region_rx, 10, 50);
                        region_ry = std::clamp(region_ry, 10, 50); // TODO: dependent on img_size...
                        ImGui::Text("%d*%d", region_rx, region_ry);

                        float region_x = std::clamp(io.MousePos.x - (pos.x + padding.x) - region_rx * 0.5f, 0.0f,
                                                    img_size.x - region_rx);
                        float region_y = std::clamp(io.MousePos.y - (pos.y + padding.y) - region_ry * 0.5f, 0.0f,
                                                    img_size.y - region_ry);

                        const ImVec2 uv0 = ImVec2((region_x) / img_size.x, (region_y) / img_size.y);
                        const ImVec2 uv1 =
                            ImVec2((region_x + region_rx) / img_size.x, (region_y + region_ry) / img_size.y);
                        const float zoom = 4.0f; // TODO: should be settable? 5.0f?
                        ImGui::Image(img.texture(), ImVec2(region_rx * zoom, region_ry * zoom), uv0, uv1);

                        // TODO: some keyctrls can be supported when dragging...
                        if (imgui_keypressed(ImGuiKey_UpArrow, true)) {
                            --region_ry;
                        }
                        if (imgui_keypressed(ImGuiKey_DownArrow, true)) {
                            ++region_ry;
                        }
                        if (imgui_keypressed(ImGuiKey_LeftArrow, true)) {
                            --region_rx;
                        }
                        if (imgui_keypressed(ImGuiKey_RightArrow, true)) {
                            ++region_rx;
                        }
                        ImGui::EndTooltip();
                    }
                }

                // TODO: reconsider this design...
                // put off to here, for "runner.shift(io.MouseDelta.x, io.MouseDelta.y);".
                // works fine as texture() (pointer) is unchanged by update, and the rendering happens very later...
                img.update(runner.tile());

                // It seems the whole recorder model is problematic...
                ImGui::AlignTextToFramePadding(); // TODO: +1 is clumsy.
                // TODO: -> editor?
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
        }

        int extra = 0;
        if (auto window = imgui_window("Tile##Ctrl", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
            // TODO: uint32_t...
            // TODO: want resetting only when +/-/enter...
            int seed = filler.seed;
            // TODO: same as "rule_editor"'s... but don't want to affect Label...
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(2, 0));
            if (ImGui::InputInt("Seed", &seed)) {
                seed = std::clamp(seed, 0, 9999);
                if (seed >= 0 && seed != filler.seed) {
                    filler.seed = seed;
                    runner.restart(filler);
                }
            }
            ImGui::PopStyleVar();

            // TODO: button <- set to 0.5?
            if (ImGui::SliderFloat("Init density [0-1]", &filler.density, 0.0f, 1.0f, "%.3f",
                                   ImGuiSliderFlags_NoInput)) {
                runner.restart(filler);
            }

            ImGui::SeparatorText("");

            ImGui::Checkbox("Pause", &ctrl.pause);
            ImGui::SameLine();
            ImGui::PushButtonRepeat(true);
            if (ImGui::Button("+1")) {
                extra = 1;
                logger::log_temp(200ms, "+1");
            }
            ImGui::SameLine();
            if (ImGui::Button("+p")) {
                extra = ctrl.actual_pergen();
                logger::log_temp(200ms, "+p({})", ctrl.actual_pergen());
            }
            ImGui::PopButtonRepeat();
            ImGui::SameLine();
            if (ImGui::Button("Restart") || imgui_keypressed(ImGuiKey_R, false)) {
                runner.restart(filler);
            }

            ImGui::AlignTextToFramePadding();
            ImGui::Text("(Actual pergen: %d)", ctrl.actual_pergen());
            ImGui::SameLine();
            ImGui::Checkbox("anti-flick", &ctrl.anti_flick);

            ImGui::SliderInt("Pergen [1-20]", &ctrl.pergen, ctrl.pergen_min, ctrl.pergen_max, "%d",
                             ImGuiSliderFlags_NoInput);
            ImGui::SliderInt("Gap Frame [0-20]", &ctrl.gap_frame, ctrl.gap_min, ctrl.gap_max, "%d",
                             ImGuiSliderFlags_NoInput);
            ImGui::SliderInt("Start gen [0-1000]", &ctrl.start_from, ctrl.start_min, ctrl.start_max, "%d",
                             ImGuiSliderFlags_NoInput);

            // TODO: enable/disable keyboard ctrl (enable by default)
            // TODO: redesign keyboard ctrl...
            if (imgui_keypressed(ImGuiKey_1, true)) {
                ctrl.gap_frame = std::max(ctrl.gap_min, ctrl.gap_frame - 1);
            }
            if (imgui_keypressed(ImGuiKey_2, true)) {
                ctrl.gap_frame = std::min(ctrl.gap_max, ctrl.gap_frame + 1);
            }
            if (imgui_keypressed(ImGuiKey_3, true)) {
                ctrl.pergen = std::max(ctrl.pergen_min, ctrl.pergen - 1);
            }
            if (imgui_keypressed(ImGuiKey_4, true)) {
                ctrl.pergen = std::min(ctrl.pergen_max, ctrl.pergen + 1);
            }
            if (imgui_keypressed(ImGuiKey_Space, true)) {
                if (!ctrl.pause) {
                    ctrl.pause = true;
                } else {
                    // TODO: log too?
                    extra = ctrl.actual_pergen();
                }
            }
            if (imgui_keypressed(ImGuiKey_M, false)) {
                ctrl.pause = !ctrl.pause;
            }
        }

        // TODO: should this be put before begin-frame?
        if (ctrl.rule != recorder.current()) {
            ctrl.rule = recorder.current();
            runner.restart(filler);
        }
        ctrl.run(runner, extra);
    }

    return 0;
}
