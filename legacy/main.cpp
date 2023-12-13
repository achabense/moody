#define IMGUI_DEFINE_MATH_OPERATORS

#include "app2.hpp"
#include "app_sdl.hpp"

#include "rule_pp.hpp"

void debug_putavail() {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, IM_COL32(255, 0, 255, 255));
}

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
} // namespace legacy

namespace legacy {
    struct termT {
        const char* msg;
        mapperT_pair eq;

        bool selected = false;
        bool covered = false;
    };

    class partition_collection {
        std::vector<termT> terms; // TODO: group by concepts...
        std::optional<partitionT> par;

        void reset_par() {
            equivT q{};
            for (auto& t : terms) {
                if (t.selected) {
                    q.add_eq(t.eq);
                }
            }
            for (auto& t : terms) {
                t.covered = q.has_eq(t.eq);
            }
            par.emplace(q);
        }

    public:
        partition_collection() {
            auto mk = [](mapperT m) { return mapperT_pair{mp_identity, m}; };

            terms.emplace_back("|", mk(mp_wsx_refl));
            terms.emplace_back("-", mk(mp_asd_refl));
            terms.emplace_back("\\", mk(mp_qsc_refl));
            terms.emplace_back("/", mk(mp_esz_refl));
            terms.emplace_back("R180", mk(mp_ro_180));
            terms.emplace_back("R90", mk(mp_ro_90));

            // TODO: misc?
            terms.emplace_back("R45*", mk(mp_ro_45));

            terms.emplace_back("Dual", mk(mp_dual));

            terms.emplace_back("q", mk(mp_ignore_q));
            terms.emplace_back("w", mk(mp_ignore_w));
            terms.emplace_back("e", mk(mp_ignore_e));
            terms.emplace_back("a", mk(mp_ignore_a));
            terms.emplace_back("s", mk(mp_ignore_s));
            terms.emplace_back("d", mk(mp_ignore_d));
            terms.emplace_back("z", mk(mp_ignore_z));
            terms.emplace_back("x", mk(mp_ignore_x));
            terms.emplace_back("c", mk(mp_ignore_c));

            reset_par();
        }

        const partitionT& get_par() {
            bool sel = false;
            if (ImGui::BeginTable(".....", terms.size(), ImGuiTableFlags_BordersInner)) {
                ImGui::TableNextRow();
                for (auto& t : terms) {
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(t.msg, &t.selected)) {
                        sel = true;
                    }
                }
                ImGui::TableNextRow();

                for (auto& t : terms) {
                    ImGui::TableNextColumn();
                    ImGui::Text(t.covered ? "y" : "-");
                }
                ImGui::EndTable();
            }

            if (sel) {
                reset_par();
            }

            return *par;
        }
    };
} // namespace legacy

// TODO: should be a class... how to decouple? ...
void edit_rule(const legacy::ruleT& target, const code_image& icons, rule_recorder& recorder) {
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
        // TODO: add border...
        imgui_strwrapped(rule_str);
    };

    // TODO: explain...
    // TODO: for "paired", support 4-step modification (_,S,B,BS)... add new color?
    // TODO: why does clang-format sort using clauses?
    using legacy::interT;
    using legacy::partitionT;

    static interT inter = {};
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

    if (auto child =
            imgui_childwindow("ForBorder", {}, true | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize)) {
        display_target();
    }
    ImGui::SeparatorText("Settings");
    set_inter();

    // TODO: rename...
    static legacy::partition_collection parcol;
    const auto& part = parcol.get_par();
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
        if (ImGui::Button("Randomize") || imgui_keypressed(ImGuiKey_Enter, false)) {
            recorder.take(random_flip(inter.get_viewer(), part, rcount, rcount)); // TODO: range...
        }
    }
    // TODO: redesign...
    if (true && ImGui::TreeNode("Misc")) {
        // TODO: should be redesigned...

        // TODO: incorrect place...
        // TODO: how to keep state-symmetry in Diff mode?
        ImGui::AlignTextToFramePadding();
        // ImGui::Text("State symmetry: %d", (int)legacy::state_symmetric(target));
        // ImGui::SameLine();
        // if (ImGui::Button("Details")) {
        //     extr = partitionT::State;
        //     inter.tag = inter.Flip;
        // }
        // ImGui::SameLine();
        if (ImGui::Button("Mir")) {
            recorder.take(legacy::mirror(target));
        }

        // TODO: experimental; not suitable place... should be totally redesigned...
        // Flip each group; the result is actually independent of inter.
        ImGui::SameLine();
        if (ImGui::Button("Flip each group")) {
            std::vector<legacy::compressT> vec;
            vec.emplace_back(target);
            for (int j = 0; j < part.k(); ++j) {
                const auto& group = part.jth_group(j);
                legacy::ruleT r = target;
                legacy::flip(group, r);
                vec.emplace_back(r);
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
        if (auto child = imgui_childwindow("Details")) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            for (int j = 0; j < k; ++j) {
                if (j % 8 != 0) {
                    ImGui::SameLine();
                }
                if (j != 0 && j % 64 == 0) {
                    ImGui::Separator(); // TODO: refine...
                }
                const bool inconsistent = scans[j] == scans.Inconsistent;
                const auto& group = part.jth_group(j);
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
                            if (x++ % 8 != 0) {
                                ImGui::SameLine();
                            }
                            // TODO: change color?
                            // TODO: what is tint_col?
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
                    legacy::ruleT r = target;
                    if (ImGui::GetIO().KeyCtrl) {
                        legacy::copy(group, inter.get_viewer(), r);
                    } else {
                        legacy::flip(group, r);
                    }
                    recorder.take(r);
                }
            }
            ImGui::PopStyleVar(2);
        }
    }
}

// TODO: refine...
// TODO: able to create/append/open file?
class file_navT {
    using path = std::filesystem::path;
    using clock = std::chrono::steady_clock;

    static path _u8path(const char* str) {
        assert(str);
        // TODO: silence the warning. The deprecation is very stupid.
        // TODO: start_lifetime (new(...)) as char8_t?
        return std::filesystem::u8path(str);
    }

    static path exe_path() {
        const std::unique_ptr<char[], decltype(+SDL_free)> p(SDL_GetBasePath(), SDL_free);
        assert(p); // TODO: what if this fails? what if exe path is invalid?
        return _u8path(p.get());
    }

    char buf_path[200]{};
    char buf_filter[20]{".txt"};

    std::vector<std::filesystem::directory_entry> dirs;
    std::vector<std::filesystem::directory_entry> files;

    clock::time_point expired = {};

    void set_current(const path& p) {
        // (Catching eagerly to avoid some flickering...)
        try {
            std::filesystem::current_path(p);
            expired = {};
        } catch (const std::exception& what) {
            // TODO: what encoding?
            logger::log_temp(1000ms, "Exception:\n{}", what.what());
        }
    }
    void refresh() {
        // Setting outside of try-block to avoid too frequent failures...
        // TODO: log_temp is global; can be problematic...
        expired = std::chrono::steady_clock::now() + 3000ms;
        try {
            dirs.clear();
            files.clear();
            for (const auto& entry : std::filesystem::directory_iterator(
                     std::filesystem::current_path(), std::filesystem::directory_options::skip_permission_denied)) {
                const auto status = entry.status();
                if (is_regular_file(status)) {
                    files.emplace_back(entry);
                }
                if (is_directory(status)) {
                    dirs.emplace_back(entry);
                }
            }
        } catch (const std::exception& what) {
            // TODO: what encoding?
            logger::log_temp(1000ms, "Exception:\n{}", what.what());
        }
    }

public:
    [[nodiscard]] std::optional<path> window(const char* id_str, bool* p_open) {
        auto window = imgui_window(id_str, p_open);
        if (!window) {
            return std::nullopt;
        }

        using namespace std;
        using namespace std::filesystem;
        optional<path> target = nullopt;

        imgui_strwrapped(cpp17_u8string(current_path()));
        ImGui::Separator();

        if (ImGui::BeginTable("##Table", 2, ImGuiTableFlags_Resizable)) {
            if (clock::now() > expired) {
                refresh();
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                if (ImGui::InputText("Path", buf_path, std::size(buf_path), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    set_current(_u8path(buf_path));
                    buf_path[0] = '\0';
                }
                if (ImGui::MenuItem("-> Exe path")) {
                    set_current(exe_path());
                }
                if (ImGui::MenuItem("-> ..")) {
                    set_current("..");
                }
                ImGui::Separator();
                if (auto child = imgui_childwindow("Folders")) {
                    if (dirs.empty()) {
                        imgui_strdisabled("None");
                    }
                    for (const auto& entry : dirs) {
                        // TODO: cache str?
                        const auto str = cpp17_u8string(entry.path().filename());
                        if (ImGui::Selectable(str.c_str())) {
                            // Won't affect the loop at this frame.
                            set_current(entry.path());
                        }
                    }
                }
            }
            ImGui::TableNextColumn();
            {
                ImGui::InputText("Filter", buf_filter, std::size(buf_filter));
                ImGui::Separator();
                if (auto child = imgui_childwindow("Files")) {
                    if (ImGui::GetIO().KeyCtrl) {
                        debug_putavail();
                    }
                    bool has = false;
                    for (const auto& entry : files) {
                        const auto str = cpp17_u8string(entry.path().filename());
                        if (str.find(buf_filter) != str.npos) {
                            has = true;
                            if (ImGui::Selectable(str.c_str())) {
                                target = entry.path();
                            }
                        }
                    }
                    if (!has) {
                        imgui_strdisabled("None");
                    }
                }
            }
            ImGui::EndTable();
        }

        return target;
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
    bool pause2 = false; // TODO: explain...

    void run(torusT& runner, int extra = 0) const {
        if (runner.gen() < start_from) {
            runner.run(rule, start_from - runner.gen());
        } else {
            if (extra != 0) {
                runner.run(rule, extra);
                extra = 0;
            }
            if (!pause && !pause2) {
                if (ImGui::GetFrameCount() % (gap_frame + 1) == 0) {
                    runner.run(rule, actual_pergen());
                }
            }
        }
    }
};

int main(int argc, char** argv) {
    // As found in debug mode, these variables shall outlive the scope_guard to avoid UB... (c_str got invalidated...)
    // (otherwise the program made mojibake-named ini file at rulelists_new on exit...)
    // Avoid "imgui.ini" (and maybe also "imgui_log.txt") sprinking everywhere.
    // TODO: maybe setting to SDL_GetBasePath / ...
    const auto cur = std::filesystem::current_path();
    const auto ini = cpp17_u8string(cur / "imgui.ini");
    const auto log = cpp17_u8string(cur / "imgui_log.txt");

    // TODO: static object? contextT?
    const auto cleanup = app_backend::init();
    ImGui::GetIO().IniFilename = ini.c_str();
    ImGui::GetIO().LogFilename = log.c_str();
    std::filesystem::current_path(R"(C:\*redacted*\Desktop\rulelists_new)");

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
        // TODO: what if the path is invalid?
    }

    tile_filler filler{.seed = 0, .density = 0.5};
    // TODO: the canvas shall not be too small...
    // TODO: should support in-screen zooming... (instead of relying on a window...)
    // TODO: should support basic-level pattern copy/pasting...
    torusT runner({.width = 480, .height = 360});
    runner.restart(filler);

    runner_ctrl ctrl{
        .rule = recorder.current(), .pergen = 1, .anti_flick = true, .start_from = 0, .gap_frame = 0, .pause = false};

    bool show_demo_window = false; // TODO: remove this in the future...
    bool show_log_window = false;  // TODO: less useful than thought...
    bool show_nav_window = true;
    file_navT nav;

    // Main loop
    tile_image img(runner.tile());
    code_image icons;

    while (app_backend::process_events()) {
        const auto frame_guard = app_backend::new_frame();

        ImGuiIO& io = ImGui::GetIO();

        // TODO: applying following logic; consider refining it.
        // recorder is modified during display, but will synchronize with runner's before next frame.
        assert(ctrl.rule == recorder.current());

        if (show_nav_window) {
            if (auto sel = nav.window("File Nav", &show_nav_window)) {
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

        const auto show_tile = [&] {
            {
                // TODO: refine...
                ImGui::Button("Scroll to ...");
                // TODO: should belong to left plane...
                if (ImGui::IsItemHovered()) {
                    if (io.MouseWheel < 0) { // scroll down
                        recorder.next();
                    } else if (io.MouseWheel > 0) { // scroll up
                        recorder.prev();
                    }
                }
                // TODO: +1 is clumsy. TODO: -> editor?
                // TODO: pos may not reflect runner's real pos, as recorder can be modified on the way... may not
                // matters
                ImGui::SameLine();
                ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1);

                static char buf_pos[20]{};
                const auto filter = [](ImGuiInputTextCallbackData* data) -> int {
                    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
                };
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200);
                if (ImGui::InputTextWithHint(
                        "##Goto", "GOTO e.g. 2->enter", buf_pos, 20,
                        ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue, filter)) {
                    int val{};
                    if (std::from_chars(buf_pos, buf_pos + strlen(buf_pos), val).ec == std::errc{}) {
                        recorder.set_pos(val - 1); // TODO: -1 is clumsy.
                    }
                    buf_pos[0] = '\0';

                    // Regain focus:
                    ImGui::SetKeyboardFocusHere(-1);
                }
            }

            ImGui::Text("Width:%d,Height:%d,Gen:%d,Density:%f", runner.tile().width(), runner.tile().height(),
                        runner.gen(), float(runner.tile().count()) / runner.tile().area());

            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size = ImGui::GetContentRegionAvail();
            ImDrawList& drawlist = *ImGui::GetWindowDrawList();

            drawlist.PushClipRect(pos, pos + size);
            drawlist.AddRectFilled(pos, pos + size, IM_COL32(20, 20, 20, 255));
            static float zoom = 1;
            static ImVec2 img_off = {0, 0}; // TODO: supposed to be of integer-precision...
            ImVec2 img_pos = pos + img_off;
            ImVec2 img_posz = img_pos + ImVec2(img.width(), img.height()) * zoom;
            img.update(runner.tile());
            drawlist.AddImage(img.texture(), img_pos, img_posz);
            // Experimental: select:
            // TODO: this shall belong to the runner.
            static ImVec2 select_0{}, select_1{}; // tile index.
            // TODO: the range should be invalid if the selected area <= 1*1.
            drawlist.AddRectFilled(img_pos + select_0 * zoom, img_pos + select_1 * zoom, IM_COL32(0, 255, 0, 60));

            drawlist.PopClipRect();

            ImGui::InvisibleButton("Canvas", size);
            const bool active = ImGui::IsItemActive();
            ctrl.pause2 = active;
            if (ImGui::IsItemHovered()) {
                assert(ImGui::IsMousePosValid());
                const ImVec2 mouse_pos = io.MousePos;
                const bool within_img = mouse_pos.x >= img_pos.x && mouse_pos.x <= img_posz.x &&
                                        mouse_pos.y >= img_pos.y && mouse_pos.y <= img_posz.y;
                if (active) {
                    // img_off += io.MouseDelta;
#if 1
                    // TODO: whether to support shifting at all?
                    if (!io.KeyCtrl) {
                        img_off += io.MouseDelta;
                    } else if (within_img) {
                        // TODO: this approach is highly imprecise when zoom != 1, but does this matter?
                        runner.shift(io.MouseDelta.x / zoom, io.MouseDelta.y / zoom);
                    }
#endif
                }
                // TODO: drop within_img constraint?
                if (io.MouseWheel != 0 && within_img) {
                    ImVec2 cellidx = (mouse_pos - img_pos) / zoom;
                    if (io.MouseWheel < 0 && zoom != 1) { // TODO: 0.5?
                        zoom /= 2;
                    }
                    if (io.MouseWheel > 0 && zoom != 8) {
                        zoom *= 2;
                    }
                    img_off = (mouse_pos - cellidx * zoom) - pos;
                    img_off.x = round(img_off.x);
                    img_off.y = round(img_off.y); // TODO: is rounding correct?
                }

                // Experimental: select:
                // TODO: this shall belong to the runner.
                // TODO: move select area...
                // TODO: precedence against left-clicking?
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    // ctrl.pause = true;
                    // TODO: is ceiling correct?
                    if (within_img) {
                        int celx = floor((mouse_pos.x - img_pos.x) / zoom);
                        int cely = floor((mouse_pos.y - img_pos.y) / zoom);
                        select_0 = ImVec2(celx, cely);
                    }
                }
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    if (within_img) {
                        int celx = ceil((mouse_pos.x - img_pos.x) / zoom);
                        int cely = ceil((mouse_pos.y - img_pos.y) / zoom);
                        select_1 = ImVec2(celx, cely);
                    }
                }
            }
            {
                int x1 = select_0.x, x2 = select_1.x;
                int y1 = select_0.y, y2 = select_1.y;
                if (x1 != x2 && y1 != y2) {
                    if (x1 > x2) {
                        std::swap(x1, x2);
                    }
                    if (y1 > y2) {
                        std::swap(y1, y2);
                    }
                    // (x1,x2) (y1,y2)
                    if (imgui_keypressed(ImGuiKey_C, false)) {
                        // TODO: export-as-rle...
                        legacy::tileT t({.width = x2 - x1, .height = y2 - y1});
                        runner.tile().copy_to(x1, y1, x2 - x1, y2 - y1, t, 0, 0);
                        std::string str = std::format("x = {}, y = {}, rule = {}\n{}", t.width(), t.height(),
                                                      legacy::to_MAP_str(ctrl.rule), legacy::to_rle_str(t));
                        ImGui::SetClipboardText(str.c_str());
                    }
                    // TODO: rand-mode (whether reproducible...)
                    // TODO: clear mode (random/all-0,all-1/paste...) / (clear inner/outer)
                    if (imgui_keypressed(ImGuiKey_Backspace, false)) {
                        legacy::tileT& tile = const_cast<legacy::tileT&>(runner.tile());
                        for (int y = y1; y < y2; ++y) {
                            for (int x = x1; x < x2; ++x) {
                                tile.line(y)[x] = 0;
                            }
                        }
                    }
                }
            }
        };

        // TODO: not robust
        // ~ runner.restart(...) shall not happen before rendering.
        bool restart = false;
        int extra = 0;

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        if (auto window = imgui_window("Tile", flags)) {
            ImGui::Text("(%.1f FPS) Frame:%d\n", io.Framerate, ImGui::GetFrameCount());
            ImGui::Separator();

            ImGui::Checkbox("Log window", &show_log_window);
            ImGui::SameLine();
            ImGui::Checkbox("Nav window", &show_nav_window);
            ImGui::SameLine();
            ImGui::Checkbox("Demo window", &show_demo_window);
            ImGui::Separator();

            // TODO: begin-table has return value...
            ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (auto child = imgui_childwindow("Rul")) {
                edit_rule(ctrl.rule, icons, recorder);
            }
            ImGui::TableNextColumn();
            if (auto child = imgui_childwindow("Til")) {
                ImGui::PushItemWidth(200); // TODO: flexible...
                ImGui::BeginGroup();
                {
                    ImGui::Checkbox("Pause", &ctrl.pause);
                    ImGui::SameLine();
                    ImGui::BeginDisabled();
                    ImGui::Checkbox("Pause2", &ctrl.pause2);
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    // ↑ TODO: better visual?
                    // ↓ TODO: imgui_repeatbutton?
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
                        restart = true;
                    }

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("(Actual pergen: %d)", ctrl.actual_pergen());
                    ImGui::SameLine();
                    ImGui::Checkbox("anti-flick", &ctrl.anti_flick);

                    ImGui::SliderInt("Pergen [1-20]", &ctrl.pergen, ctrl.pergen_min, ctrl.pergen_max, "%d",
                                     ImGuiSliderFlags_NoInput);
                    // TODO: Gap-frame shall be really timer-based...
                    ImGui::SliderInt("Gap Frame [0-20]", &ctrl.gap_frame, ctrl.gap_min, ctrl.gap_max, "%d",
                                     ImGuiSliderFlags_NoInput);
                    ImGui::SliderInt("Start gen [0-1000]", &ctrl.start_from, ctrl.start_min, ctrl.start_max, "%d",
                                     ImGuiSliderFlags_NoInput);
                }
                ImGui::EndGroup();
                ImGui::SameLine();
                ImGui::BeginGroup();
                {
                    // TODO: uint32_t...
                    // TODO: want resetting only when +/-/enter...
                    int seed = filler.seed;
                    // TODO: same as "rule_editor"'s... but don't want to affect Label...
                    // ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(2, 0));
                    if (ImGui::InputInt("Seed", &seed)) {
                        seed = std::clamp(seed, 0, 9999);
                        if (seed >= 0 && seed != filler.seed) {
                            filler.seed = seed;
                            restart = true;
                        }
                    }
                    // ImGui::PopStyleVar();
                    // TODO: button <- set to 0.5?
                    if (ImGui::SliderFloat("Init density [0-1]", &filler.density, 0.0f, 1.0f, "%.3f",
                                           ImGuiSliderFlags_NoInput)) {
                        restart = true;
                    }
                }
                ImGui::EndGroup();
                ImGui::PopItemWidth();

                // TODO: shall redesign...
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
                show_tile();
            }
            ImGui::EndTable();
        }

        // TODO: remove this when all done...
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
        }
        if (show_log_window) {
            logger::window("Events", &show_log_window);
        }
        logger::tempwindow();

        // TODO: should this be put before begin-frame?
        if (ctrl.rule != recorder.current()) {
            ctrl.rule = recorder.current();
            restart = true;
        }
        if (restart) {
            runner.restart(filler);
        }
        ctrl.run(runner, extra);
    }

    return 0;
}
