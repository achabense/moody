#define IMGUI_DEFINE_MATH_OPERATORS

#include "app2.hpp"
#include "app_sdl.hpp"
#include "imgui_internal.h" // TODO: for `RenderNavHighlight`

#include "rule_pp.hpp"

#if 0
//12/24 temporal; used to transform qc_emulation to ez_emulation...
namespace legacy {
    inline equivT make_equiv(const mapperT_pair& q) {
        equivT eq{};
        eq.add_eq(q);
        return eq;
    }

    // what about
    // 010
    // 111
    // 010 rule?
    inline ruleT switch_hex(const ruleT& r) {
        static equivT hex1 = make_equiv({mp_identity, mp_hex_ignore});  // ez
        static equivT hex2 = make_equiv({mp_identity, mp_hex2_ignore}); // qc
        constexpr interT inter{};
        if (hex1.test(inter.from_rule(r))) {
            // ez->qc
            ruleT ret{};
            for_each_code(code) {
                auto [q, w, e, a, s, d, z, x, c] = decode(code);
                ret.set(code, r(encode(w, e, 0, a, s, d, 0, z, x))); // ???
            }
            return ret;
        } else if (hex2.test(inter.from_rule(r))) {
            // qc->ez
            ruleT ret{};
            for_each_code(code) {
                auto [q, w, e, a, s, d, z, x, c] = decode(code);
                ret.set(code, r(encode(0, q, w, a, s, d, x, c, 0))); // ???
            }
            return ret;
        }
        return r;
    }
} // namespace legacy
#endif

namespace legacy {
    // TODO: move elsewhere
    // TODO: proper name...
    inline ruleT mirror(const ruleT& rule) {
        ruleT mir{};
        for_each_code(code) {
            codeT codex = flip_all(code);
            const bool flip = decode_s(codex) != rule(codex);
            mir.set(code, flip ? !decode_s(code) : decode_s(code));
        }
        return mir;
    }

    struct termT {
        const char* msg;
        equivT eq;

        bool selected = false;
        bool covered = false;
    };
    // TODO: refine analyzer...
    class partition_collection {
        using termT_vec = std::vector<termT>;

        termT_vec terms_ignore;
        termT_vec terms_native;
        termT_vec terms_misc;
        termT_vec terms_hex;

        // TODO: customized...

        std::optional<partitionT> par;

        void reset_par() {
            equivT q{};
            auto set = [&q](auto& terms) {
                for (auto& t : terms) {
                    if (t.selected) {
                        q.add_eq(t.eq);
                    }
                }
            };
            set(terms_ignore);
            set(terms_native);
            set(terms_misc);
            set(terms_hex);

            auto test = [&q](auto& terms) {
                for (auto& t : terms) {
                    t.covered = q.has_eq(t.eq);
                }
            };

            test(terms_ignore);
            test(terms_native);
            test(terms_misc);
            test(terms_hex);

            par.emplace(q);
        }

    public:
        partition_collection() {
            auto mk = [](std::initializer_list<mapperT> ms) {
                equivT eq{};
                for (const mapperT& m : ms) {
                    eq.add_eq({mp_identity, m}); // TODO: expose mp_identity?
                }
                return eq;
            };

            terms_ignore.emplace_back("q", mk({mp_ignore_q}));
            terms_ignore.emplace_back("w", mk({mp_ignore_w}));
            terms_ignore.emplace_back("e", mk({mp_ignore_e}));
            terms_ignore.emplace_back("a", mk({mp_ignore_a}));
            terms_ignore.emplace_back("s", mk({mp_ignore_s}));
            terms_ignore.emplace_back("d", mk({mp_ignore_d}));
            terms_ignore.emplace_back("z", mk({mp_ignore_z}));
            terms_ignore.emplace_back("x", mk({mp_ignore_x}));
            terms_ignore.emplace_back("c", mk({mp_ignore_c}));

            terms_native.emplace_back("|", mk({mp_refl_wsx}));
            terms_native.emplace_back("-", mk({mp_refl_asd}));
            terms_native.emplace_back("\\", mk({mp_refl_qsc}));
            terms_native.emplace_back("/", mk({mp_refl_esz}));
            terms_native.emplace_back("C2(180)", mk({mp_C2}));
            terms_native.emplace_back("C4(90)", mk({mp_C4}));

            terms_misc.emplace_back("*R45", mk({mp_ro_45}));
            terms_misc.emplace_back("*Tota", mk({mp_ro_45, mp_tot_a}));
            terms_misc.emplace_back("*Totb", mk({mp_ro_45, mp_tot_b}));
            terms_misc.emplace_back("Dual", mk({mp_dual}));

            terms_hex.emplace_back("Hex", mk({mp_hex_ignore}));
            terms_hex.emplace_back("/", mk({mp_hex_refl_wsx}));
            terms_hex.emplace_back("\\", mk({mp_hex_refl_qsc}));
            terms_hex.emplace_back("-", mk({mp_hex_refl_asd}));
            terms_hex.emplace_back("aq", mk({mp_hex_refl_aq}));
            terms_hex.emplace_back("qw", mk({mp_hex_refl_qw}));
            terms_hex.emplace_back("wd", mk({mp_hex_refl_wd}));

            terms_hex.emplace_back("C2(180)", mk({mp_hex_C2}));
            terms_hex.emplace_back("C3(120)", mk({mp_hex_C3}));
            terms_hex.emplace_back("C6(60)", mk({mp_hex_C6}));

            terms_hex.emplace_back("*Tota", mk({mp_hex_C6, mp_hex_tot_a}));
            terms_hex.emplace_back("*Totb", mk({mp_hex_C6, mp_hex_tot_b}));

            reset_par();
        }

        const partitionT& get_par(const ruleT& target) {
            // TODO: tooltip...
            /*
            const auto show_pair = [](const mapperT_pair& q) {
                std::string up, cn, dw;
                // TODO: too clumsy...
                static const char* const strs[]{" 0", " 1", " q", " w", " e", " a", " s", " d", " z", " x",
                                                " c", "!q", "!w", "!e", "!a", "!s", "!d", "!z", "!x", "!c"};

                up += strs[q.a.q2];
                up += strs[q.a.w2];
                up += strs[q.a.e2];
                up += "  ";
                up += strs[q.b.q2];
                up += strs[q.b.w2];
                up += strs[q.b.e2];

                cn += strs[q.a.a2];
                cn += strs[q.a.s2];
                cn += strs[q.a.d2];
                cn += " ~";
                cn += strs[q.b.a2];
                cn += strs[q.b.s2];
                cn += strs[q.b.d2];

                dw += strs[q.a.z2];
                dw += strs[q.a.x2];
                dw += strs[q.a.c2];
                dw += "  ";
                dw += strs[q.b.z2];
                dw += strs[q.b.x2];
                dw += strs[q.b.c2];

                imgui_str(up + " \n" + cn + " \n" + dw + " ");
            };
            */
            bool sel = false;

            // TODO: recheck id & tid logic... (& imagebutton)
            auto check = [&, id = 0, r = ImGui::GetFrameHeight()](termT& term) mutable {
                // TODO: which should come first? rendering or dummy button?
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImVec2 pos_max = pos + ImVec2{r, r};
                // TODO: a bit ugly...
                ImGui::GetWindowDrawList()->AddRectFilled(pos, pos_max,
                                                          term.selected  ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                                          : term.covered ? ImGui::GetColorU32(ImGuiCol_FrameBg)
                                                                         : 0);
                ImGui::GetWindowDrawList()->AddRect(pos, pos_max, ImGui::GetColorU32(ImGuiCol_Button));

                // TODO: Flip...
                if (satisfies(target, {}, term.eq)) {
                    ImGui::GetWindowDrawList()->AddRect(pos, pos_max, ImGui::GetColorU32(ImVec4{0, 1, 0, 1}));
                }

                ImGui::PushID(id++);
                bool hit = ImGui::InvisibleButton("Check", ImVec2{r, r});
                // TODO: this is in imgui_internal.h...
                // TODO: Ask is it intentional to make InvisibleButton highlight-less?
                // TODO: use normal buttons instead?
                ImGui::RenderNavHighlight({ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}, ImGui::GetItemID());
                ImGui::PopID();

                // if (ImGui::BeginItemTooltip()) {
                //     show_pair(term.eq);
                //     ImGui::EndTooltip();
                // }
                if (hit) {
                    term.selected = !term.selected;
                    sel = true;
                }
            };

            // TODO: slightly confusing; light color should represent "take-into-account" instead of "ignore"
            // Is this solvable by applying specific coloring scheme?
            ImGui::BeginGroup();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int l = 0; l < 3; ++l) {
                check(terms_ignore[l * 3 + 0]);
                ImGui::SameLine();
                check(terms_ignore[l * 3 + 1]);
                ImGui::SameLine();
                check(terms_ignore[l * 3 + 2]);
            }
            ImGui::PopStyleVar();
            ImGui::EndGroup();

            auto table = [&check, tid = 0](auto& terms) mutable {
                ImGui::PushID(tid++);
                if (ImGui::BeginTable("Table", terms.size(), ImGuiTableFlags_BordersInner)) {
                    ImGui::TableNextRow();
                    for (termT& t : terms) {
                        ImGui::TableNextColumn();
                        imgui_str(t.msg);
                        check(t);
                        // ImGui::SameLine();
                        // ImGui::AlignTextToFramePadding();
                        // imgui_str(t.msg);
                    }
                    ImGui::EndTable();
                }
                ImGui::PopID();
            };

            table(terms_native);
            ImGui::Separator();
            table(terms_misc);
            ImGui::Separator();
            table(terms_hex);

            if (sel) {
                reset_par();
            }
            return *par;
        }
    };
} // namespace legacy

void show_target_rule(const legacy::ruleT& target, rule_recorder& recorder) {
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
                    recorder.replace(std::move(rules));
                } else {
                    logger::log_temp(300ms, "Same rule");
                }
            }
        }
    }

    // TODO: +1 is clumsy. TODO: -> editor?
    // TODO: pos may not reflect runner's real pos, as recorder can be modified on the way... may not
    // matters
    // ImGui::SameLine();
    // if (ImGui::Button("|<")) {
    //     recorder.set_first();
    // }
    // ImGui::SameLine();
    // if (ImGui::Button(">|")) {
    //     recorder.set_last();
    // }
    // ImGui::SameLine();
    // imgui_str(std::format("Total:{} At:{}", recorder.size(), recorder.pos() + 1));
    // TODO: Moved near to other iter widgets...
    // ImGui::Button(std::format("Total:{} At:{}###...", recorder.size(), recorder.pos() + 1).c_str());
    // if (ImGui::IsItemHovered()) {
    //     if (imgui_scrolldown()) {
    //         recorder.next();
    //     } else if (imgui_scrollup()) {
    //         recorder.prev();
    //     }
    // }
#if 0
    // TODO: is random-access useful?
    static char buf_pos[20]{};
    const auto filter = [](ImGuiInputTextCallbackData* data) -> int {
        return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
    };
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputTextWithHint("##Goto", "GOTO e.g. 2->enter", buf_pos, 20,
                                 ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue,
                                 filter)) {
        int val{};
        if (std::from_chars(buf_pos, buf_pos + strlen(buf_pos), val).ec == std::errc{}) {
            recorder.set_pos(val - 1); // TODO: -1 is clumsy.
        }
        buf_pos[0] = '\0';

        // Regain focus:
        ImGui::SetKeyboardFocusHere(-1);
    }
#endif

    // TODO: re-implement file-saving
    imgui_str(rule_str);
}

// TODO: should be a class... how to decouple? ...
void edit_rule(const legacy::ruleT& target, const code_image& icons, rule_recorder& recorder) {
    static legacy::interT inter = {};
    {
        int itag = inter.tag;
        const auto tooltip = [](legacy::interT::tagE tag, const char* msg = "View from:") {
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

        inter.tag = legacy::interT::tagE{itag};
        if (inter.tag == inter.Diff) {
            ImGui::SameLine();
            if (ImGui::Button("Take current rule")) {
                inter.custom = target;
            }
            tooltip(inter.Diff);
        } else {
            // TODO: demonstration-only (this function should be explicit). Redesign...
            ImGui::SameLine();
            if (ImGui::Button("Try diff")) {
                inter.tag = inter.Diff;
            }
        }
    }

    // TODO: rename...
    static legacy::partition_collection parcol;
    const auto& part = parcol.get_par(target);
    const int k = part.k();
    {
        // TODO: unstable between base/extr switchs; ratio-based approach is on-trivial though... (double has
        // inaccessible values)
        static int rcount = 0.3 * k;
        rcount = std::clamp(rcount, 0, k);

        ImGui::SetNextItemWidth(200); // TODO...
        ImGui::SliderInt("##Active", &rcount, 0, k, "%d", ImGuiSliderFlags_NoInput);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0)); // TODO: ?? ImGui::GetStyle().ItemInnerSpacing
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
            recorder.take(random_flip(inter.get_viewer(), part, rcount, rcount, global_mt19937)); // TODO: range...
        }

        // TODO: iteration should be the key concept...
        // TODO: redesign...
        {
            auto iter_pair = [](const char* tag_first, const char* tag_prev, const char* tag_next, const char* tag_last,
                                auto act_first, auto act_prev, auto act_next, auto act_last) {
                // TODO: temporal; [[exceptions should be avoided]]
                auto consume_exception = [](auto& act) {
                    try {
                        act();
                    } catch (...) {
                        logger::log_temp(300ms, "X_X");
                    }
                };

                if (ImGui::Button(tag_first)) {
                    consume_exception(act_first);
                }

                ImGui::SameLine();
                ImGui::BeginGroup();
                if (ImGui::Button(tag_prev)) {
                    consume_exception(act_prev);
                }
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0) /*ImGui::GetStyle().ItemInnerSpacing*/);
                ImGui::SameLine();
                if (ImGui::Button(tag_next)) {
                    consume_exception(act_next);
                }
                ImGui::PopStyleVar();
                ImGui::EndGroup();
                if (ImGui::IsItemHovered()) {
                    if (imgui_scrollup()) {
                        consume_exception(act_prev);
                    } else if (imgui_scrolldown()) {
                        consume_exception(act_next);
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button(tag_last)) {
                    consume_exception(act_last);
                }
            };

            ImGui::AlignTextToFramePadding();
            imgui_str(std::format("Total:{} At:{}", recorder.size(), recorder.pos() + 1));
            ImGui::SameLine();
            iter_pair(
                "<|", "prev", "next", "|>", //
                [&] { recorder.set_first(); }, [&] { recorder.prev(); }, [&] { recorder.next(); },
                [&] { recorder.set_last(); });

            iter_pair(
                "<00..", "dec", "inc", "11..>", //
                [&] { recorder.take(legacy::act_int::first(inter, part, target)); },
                [&] { recorder.take(legacy::act_int::prev(inter, part, target)); },
                [&] { recorder.take(legacy::act_int::next(inter, part, target)); },
                [&] { recorder.take(legacy::act_int::last(inter, part, target)); });
            ImGui::SameLine(), imgui_str("|");
            ImGui::SameLine();
            iter_pair(
                "<1.0.", "pprev", "pnext", "0.1.>", //
                [&] { recorder.take(legacy::act_perm::first(inter, part, target)); },
                [&] { recorder.take(legacy::act_perm::prev(inter, part, target)); },
                [&] { recorder.take(legacy::act_perm::next(inter, part, target)); },
                [&] { recorder.take(legacy::act_perm::last(inter, part, target)); });
            ImGui::SameLine(), imgui_str("|");
            ImGui::SameLine();
            if (ImGui::Button("Mir")) {
                recorder.replace_current(legacy::mirror(target));
            }
        }
    }
    {
        static const char* const strss[3][3]{{"-0", "-1", "-x"}, //
                                             {"-.", "-f", "-x"},
                                             {"-.", "-d", "-x"}};
        const auto strs = strss[inter.tag];

        // TODO: should be foldable; should be able to set max height...
        const legacy::ruleT_data drule = inter.from_rule(target);
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
                    recorder.take(r); // replace_current?
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
// TODO: how to capture certain patterns? (editor++)...

// TODO: reconsider: where should "current-rule" be located...
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
    app_backend::init();

    {
        // Avoid "imgui.ini" (and maybe also "imgui_log.txt") sprinking everywhere.
        // TODO: As to "strdup": temporary; the leaks are negligible here.
        // TODO: eh... strdup is not always available...
        // https://en.cppreference.com/w/c/experimental/dynamic/strdup
        // TODO: maybe setting to SDL_GetBasePath / ...
        const auto cur = std::filesystem::current_path();
        ImGui::GetIO().IniFilename = strdup(cpp17_u8string(cur / "imgui.ini").c_str());
        ImGui::GetIO().LogFilename = strdup(cpp17_u8string(cur / "imgui_log.txt").c_str());

        // TODO: remove when finished...
        std::filesystem::current_path(R"(C:\*redacted*\Desktop\rulelists_new)");
    }

    // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // TODO: works but blurry, and how to apply in project?
    // {
    //     const char* fnpath = R"(C:\*redacted*\Desktop\Deng.ttf)";
    //     ImGui::GetIO().Fonts->AddFontFromFileTTF(fnpath, 13, nullptr,
    //                                                ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());
    // }

    rule_recorder recorder;
    // TODO: what encoding?
    if (argc == 2) {
        recorder.replace(read_rule_from_file(argv[1]));
        // TODO: set as working path when the path is valid?
        // TODO: what if the path is invalid?
    }

    // TODO: redesign...
    tileT_fill_arg filler{.use_seed = true, .seed = 0, .density = 0.5};
    torusT runner({.width = 480, .height = 360});
    runner.restart(filler);

    runner_ctrl ctrl{
        .rule = recorder.current(), .pergen = 1, .anti_flick = true, .start_from = 0, .gap_frame = 0, .pause = false};

    bool show_nav_window = true;
    file_navT nav;

    tile_image img(runner.tile());
    code_image icons;
    while (app_backend::new_frame()) {
        // TODO: applying following logic; consider refining it.
        // recorder is modified during display, but will synchronize with runner's before next frame.
        assert(ctrl.rule == recorder.current());

        if (show_nav_window) {
            if (auto sel = nav.window("File Nav", &show_nav_window)) {
                // TODO: record filename?
                auto result = read_rule_from_file(*sel);
                if (!result.empty()) {
                    logger::log_temp(500ms, "found {} rules", result.size());
                    recorder.replace(std::move(result));
                } else {
                    logger::log_temp(300ms, "found nothing");
                }
            }
        }

        const auto show_tile = [&] {
            // TODO: lift "Width..." to before the ctrl widgets?
            ImGui::Text("Width:%d,Height:%d,Gen:%d,Density:%f", runner.tile().width(), runner.tile().height(),
                        runner.gen(), float(legacy::count(runner.tile())) / runner.tile().area());

            const ImVec2 screen_pos = ImGui::GetCursorScreenPos();
            const ImVec2 screen_size = ImGui::GetContentRegionAvail();
            ImDrawList& drawlist = *ImGui::GetWindowDrawList();

            drawlist.PushClipRect(screen_pos, screen_pos + screen_size);
            drawlist.AddRectFilled(screen_pos, screen_pos + screen_size, IM_COL32(20, 20, 20, 255));

            static float zoom = 1; // TODO: mini window when zoom == 1?
            // It has been proven that `img_off` works better than using `corner_idx` (cell idx in the corner)
            static ImVec2 img_off = {0, 0}; // TODO: supposed to be of integer-precision...
            ImVec2 img_pos = screen_pos + img_off;
            img.update(runner.tile());
            drawlist.AddImage(img.texture(), img_pos, img_pos + ImVec2(img.width(), img.height()) * zoom);
            // Experimental: select:
            // TODO: this shall belong to the runner.
            static ImVec2 select_0{}, select_1{}; // tile index, not pixel.
            // TODO: shaky...
            // TODO: show selected size...
            // TODO: ctrl to move selected area?
            struct sel_info {
                int x1, y1; // [
                int x2, y2; // )

                int width() const { return x2 - x1; }
                int height() const { return y2 - y1; }
                ImVec2 min() const { return ImVec2(x1, y1); }
                ImVec2 max() const { return ImVec2(x2, y2); }
                explicit operator bool() const { return width() > 1 || height() > 1; }
            };
            const auto get_select = []() -> sel_info {
                // TODO: rephrase...
                // select_0/1 denotes []; convert to [):
                int x1 = select_0.x, x2 = select_1.x;
                int y1 = select_0.y, y2 = select_1.y;
                if (x1 > x2) {
                    std::swap(x1, x2);
                }
                if (y1 > y2) {
                    std::swap(y1, y2);
                }
                return {x1, y1, x2 + 1, y2 + 1};
            };
            // TODO: whether to limit this way? (maybe better to check click pos instead...)
            if (sel_info sel = get_select()) {
                drawlist.AddRectFilled(img_pos + sel.min() * zoom, img_pos + sel.max() * zoom, IM_COL32(0, 255, 0, 60));
            }
            drawlist.PopClipRect();

            ImGui::InvisibleButton("Canvas", screen_size);
            const bool active = ImGui::IsItemActive();
            ctrl.pause2 = active;
            if (ImGui::IsItemHovered()) {
                const ImGuiIO& io = ImGui::GetIO();

                assert(ImGui::IsMousePosValid());
                // It turned out that, this will work well even if outside of the image...
                const ImVec2 mouse_pos = io.MousePos;
                if (active) {
                    // TODO: whether to support shifting at all?
                    if (!io.KeyCtrl) {
                        img_off += io.MouseDelta;
                    } else {
                        // TODO: this approach is highly imprecise when zoom != 1, but does this matter?
                        runner.shift(io.MouseDelta.x / zoom, io.MouseDelta.y / zoom);
                    }
                }
                if (imgui_scrolling()) {
                    ImVec2 cellidx = (mouse_pos - img_pos) / zoom;
                    if (imgui_scrolldown() && zoom != 1) { // TODO: 0.5?
                        zoom /= 2;
                    }
                    if (imgui_scrollup() && zoom != 8) {
                        zoom *= 2;
                    }
                    img_off = (mouse_pos - cellidx * zoom) - screen_pos;
                    img_off.x = round(img_off.x);
                    img_off.y = round(img_off.y); // TODO: is rounding correct?
                }

                // Experimental: select:
                // TODO: this shall belong to the runner.
                // TODO: precedence against left-clicking?
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    // ctrl.pause = true;
                    int celx = floor((mouse_pos.x - img_pos.x) / zoom);
                    int cely = floor((mouse_pos.y - img_pos.y) / zoom);

                    celx = std::clamp(celx, 0, img.width() - 1);
                    cely = std::clamp(cely, 0, img.height() - 1); // TODO: shouldn't be img.xxx()...
                    select_0 = ImVec2(celx, cely);
                }
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    int celx = floor((mouse_pos.x - img_pos.x) / zoom);
                    int cely = floor((mouse_pos.y - img_pos.y) / zoom);

                    celx = std::clamp(celx, 0, img.width() - 1);
                    cely = std::clamp(cely, 0, img.height() - 1); // TODO: shouldn't be img.xxx()...
                    select_1 = ImVec2(celx, cely);
                }
            }
            if (sel_info sel = get_select()) {
                if (imgui_keypressed(ImGuiKey_C, false)) {
                    legacy::tileT t({.width = sel.width(), .height = sel.height()});
                    legacy::copy(runner.tile(), sel.x1, sel.y1, sel.width(), sel.height(), t, 0, 0);
                    std::string str = std::format("x = {}, y = {}, rule = {}\n{}", t.width(), t.height(),
                                                  legacy::to_MAP_str(ctrl.rule), legacy::to_rle_str(t));
                    ImGui::SetClipboardText(str.c_str());
                }
                // TODO: rand-mode (whether reproducible...)
                // TODO: clear mode (random/all-0,all-1/paste...) / (clear inner/outer)
                // TODO: horrible; redesign (including control) ...
                // TODO: 0/1/other textures are subject to agar settings...
                if (imgui_keypressed(ImGuiKey_Backspace, false)) {
                    legacy::tileT& tile = const_cast<legacy::tileT&>(runner.tile());
                    for (int y = sel.y1; y < sel.y2; ++y) {
                        for (int x = sel.x1; x < sel.x2; ++x) {
                            tile.line(y)[x] = 0;
                        }
                    }
                }
                if (imgui_keypressed(ImGuiKey_Equal, false)) {
                    legacy::tileT& tile = const_cast<legacy::tileT&>(runner.tile());
                    constexpr uint32_t c = std::mt19937::max() * 0.5;
                    for (int y = sel.y1; y < sel.y2; ++y) {
                        for (int x = sel.x1; x < sel.x2; ++x) {
                            tile.line(y)[x] = global_mt19937() < c;
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
            // ImGui::Checkbox("Log window", &show_log_window);
            // ImGui::SameLine();
            ImGui::Checkbox("Nav window", &show_nav_window);
            ImGui::SameLine();
            ImGui::Text("   (%.1f FPS) Frame:%d\n", ImGui::GetIO().Framerate, ImGui::GetFrameCount());

            show_target_rule(ctrl.rule, recorder);
            ImGui::Separator();

            if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
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
                        // TODO: should allow keyboard control...
                        if (ImGui::Button("+1")) {
                            extra = 1;
                            logger::log_temp(200ms, "+1"); // TODO: useful?
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("+p")) { // TODO: Button("+p(...)")?
                            extra = ctrl.actual_pergen();
                            logger::log_temp(200ms, "+p({})", ctrl.actual_pergen());
                        }
                        ImGui::PopButtonRepeat();
                        ImGui::SameLine();
                        if (ImGui::Button("Restart") || imgui_keypressed(ImGuiKey_R, false)) {
                            restart = true;
                        }

                        // TODO: Gap-frame shall be really timer-based...
                        ImGui::SliderInt("Gap Frame [0-20]", &ctrl.gap_frame, ctrl.gap_min, ctrl.gap_max, "%d",
                                         ImGuiSliderFlags_NoInput);
                        ImGui::SliderInt("Start gen [0-1000]", &ctrl.start_from, ctrl.start_min, ctrl.start_max, "%d",
                                         ImGuiSliderFlags_NoInput);
                        ImGui::SliderInt("Pergen [1-20]", &ctrl.pergen, ctrl.pergen_min, ctrl.pergen_max, "%d",
                                         ImGuiSliderFlags_NoInput);
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("(Actual pergen: %d)", ctrl.actual_pergen());
                        ImGui::SameLine();
                        ImGui::Checkbox("anti-flick", &ctrl.anti_flick);
                    }
                    ImGui::EndGroup();
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    {
                        // TODO: use radio instead...
                        // TODO: imgui_binaryradio???
                        if (ImGui::Checkbox("Use seed", &filler.use_seed)) {
                            // TODO: unconditional?
                            if (filler.use_seed) {
                                restart = true;
                            }
                        }
                        if (!filler.use_seed) {
                            ImGui::BeginDisabled();
                        }
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
                        if (!filler.use_seed) {
                            ImGui::EndDisabled();
                        }

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
                    // TODO: want to allow setting hard pausing when dragging (soft locking)...
                    if (imgui_keypressed(ImGuiKey_Space, true)) {
                        ctrl.pause = !ctrl.pause;
                    }
                    show_tile();
                }
                ImGui::EndTable();
            }
        }

        ImGui::ShowDemoWindow(); // TODO: remove (or comment-out) this when all done...
        logger::tempwindow();
        app_backend::render(); // !!! TODO: recheck

        if (ctrl.rule != recorder.current()) {
            ctrl.rule = recorder.current();
            restart = true;
            ctrl.pause = false; // TODO: this should be configurable...
        }
        if (restart) {
            runner.restart(filler);
        }
        ctrl.run(runner, extra);
    }

    app_backend::clear();
    return 0;
}

// TODO: tile: rewind; fullscreen...
