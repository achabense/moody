#include "app.hpp"

#include "rule_algo.hpp"

// TODO: Right-click must either to open a submenu, or to toggle on/off the tooltip.
// TODO: Generalize typical behavior patterns to find new rules.

// TODO: should not belong to namespace legacy...
namespace legacy {
    class subset_selector {
        subsetT current;

        struct termT {
            const char* title;
            subsetT set;
            bool selected = false;
            bool includes_cur = false;
            bool disabled = false; // current & set -> empty.
            const char* description = nullptr;
        };

        using termT_vec = std::vector<termT>;

        termT_vec terms_ignore;
        termT_vec terms_native;
        termT_vec terms_misc;
        termT_vec terms_hex;
        // TODO: about the plan to support user-defined subsets...

        void for_each_term(auto fn) {
            for (termT_vec* terms : {&terms_ignore, &terms_native, &terms_misc, &terms_hex}) {
                for (termT& t : *terms) {
                    fn(t);
                }
            }
        }

        void reset_current() {
            current = subsetT::universalT{};

            for_each_term([&](termT& t) {
                assert(!t.disabled || !t.selected); // disabled -> !selected
                if (t.selected) {
                    current = current & t.set;
                }
            });

            assert(!current.empty());

            for_each_term([&](termT& t) {
                t.includes_cur = t.set.includes(current);
                assert(!t.selected || t.includes_cur); // selected -> includes_cur
                // TODO: involves a lot of unneeded calculations...
                t.disabled = !t.includes_cur && (t.set & current).empty();
            });
        }

    public:
        subset_selector() : current(subsetT::universalT{}) {
            // TODO: add more subsets (von, direct iso, etc)
            // TODO: add some tests after the construction...
            const auto mk = [](std::initializer_list<mapperT> ms, const maskT& mask = mask_zero) {
                equivT eq{};
                for (const mapperT& m : ms) {
                    add_eq(eq, {mp_identity, m});
                }
                return subsetT{mask, eq};
            };

            terms_ignore.emplace_back("q", mk({mp_ignore_q}));
            terms_ignore.emplace_back("w", mk({mp_ignore_w}));
            terms_ignore.emplace_back("e", mk({mp_ignore_e}));
            terms_ignore.emplace_back("a", mk({mp_ignore_a}));

            // TODO: add {mask_identity, {mp_ignore_s}}? it's sensible but totally different.
            terms_ignore.emplace_back("s", mk({mp_ignore_s}));
            terms_ignore.emplace_back("d", mk({mp_ignore_d}));
            terms_ignore.emplace_back("z", mk({mp_ignore_z}));
            terms_ignore.emplace_back("x", mk({mp_ignore_x}));
            terms_ignore.emplace_back("c", mk({mp_ignore_c}));

            // TODO: temp...
            terms_ignore.emplace_back("S'", mk({mp_ignore_s}, mask_identity));
            terms_ignore.emplace_back("Hex", mk({mp_hex_ignore}));
            // TODO: or define mp_von_ignore?
            terms_ignore.emplace_back("Von", mk({mp_ignore_q, mp_ignore_e, mp_ignore_z, mp_ignore_c}));

            terms_native.emplace_back("All", mk({mp_refl_wsx, mp_refl_qsc}));
            terms_native.emplace_back("|", mk({mp_refl_wsx}));
            terms_native.emplace_back("-", mk({mp_refl_asd}));
            terms_native.emplace_back("\\", mk({mp_refl_qsc}));
            terms_native.emplace_back("/", mk({mp_refl_esz}));
            terms_native.emplace_back("C2", mk({mp_C2}));
            terms_native.emplace_back("C4", mk({mp_C4})); // TODO: add explanations in the gui

            terms_misc.emplace_back("'C8'", mk({mp_C8}));
            terms_misc.emplace_back("Tot", mk({mp_C8, mp_tot_exc_s}));
            terms_misc.emplace_back("Tot(+s)", mk({mp_C8, mp_tot_inc_s}));

            terms_misc.emplace_back("Dual", mk({mp_dual}, mask_identity)); // <-------

            terms_misc.emplace_back("Hex_Tot", mk({mp_hex_C6, mp_hex_tot_exc_s}));
            terms_misc.emplace_back("Hex_Tot(+s)", mk({mp_hex_C6, mp_hex_tot_inc_s}));

            terms_hex.emplace_back("All", mk({mp_hex_refl_asd, mp_hex_refl_aq}));
            terms_hex.emplace_back("a-d", mk({mp_hex_refl_asd}));
            terms_hex.emplace_back("q-c", mk({mp_hex_refl_qsc}));
            terms_hex.emplace_back("w-x", mk({mp_hex_refl_wsx}));
            terms_hex.emplace_back("a|q", mk({mp_hex_refl_aq}));
            terms_hex.emplace_back("q|w", mk({mp_hex_refl_qw}));
            terms_hex.emplace_back("w|d", mk({mp_hex_refl_wd}));

            terms_hex.emplace_back("C2", mk({mp_hex_C2}));
            terms_hex.emplace_back("C3", mk({mp_hex_C3}));
            terms_hex.emplace_back("C6", mk({mp_hex_C6}));

            reset_current();
        }

        subsetT& select_subset(const ruleT& target, const lockT& locked) {
            bool need_reset = false;
            const float r = ImGui::GetFrameHeight();
            const ImVec2 sqr{r, r};

            // TODO: tooltip...
            // TODO: recheck id & tid logic... (& imagebutton)
            auto check = [&, id = 0](termT& term, const ImVec2& size) mutable {
                // TODO: change color when hovered?
                // bool hovered = false;
                ImGui::PushID(id++);
                if (!term.disabled) {
                    if (ImGui::InvisibleButton("Check", size)) {
                        term.selected = !term.selected;
                        // TODO: No need to reset if the newly selected term already included current.
                        need_reset = true;

                        // TODO: need to recheck all invisible buttons etc if to enable Navigation.
                        // e.g. RenderNavHighlight
                    }
                    // hovered = ImGui::IsItemHovered();
                } else {
                    ImGui::Dummy(size);
                }
                ImGui::PopID();

                // TODO: explain coloring scheme; redesign if necessary (especially ring col)
                // TODO: find better color for "disabled"/incompatible etc... currently too ugly.
                const ImU32 cen_col = term.selected       ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                      : term.includes_cur ? ImGui::GetColorU32(ImGuiCol_FrameBg)
                                      : term.disabled     ? IM_COL32(150, 0, 0, 255)
                                                          : IM_COL32_BLACK;
                const ImU32 ring_col = term.set.contains(target)              ? IM_COL32(0, 255, 0, 255)
                                       : compatible(term.set, target, locked) ? IM_COL32(0, 100, 0, 255)
                                                                              : IM_COL32(255, 0, 0, 255);

                const ImVec2 pos_min = ImGui::GetItemRectMin();
                const ImVec2 pos_max = ImGui::GetItemRectMax();
                assert(pos_min.x + size.x == pos_max.x);
                assert(pos_min.y + size.y == pos_max.y);
                assert(size.x > 8 && size.y > 8);
                ImGui::GetWindowDrawList()->AddRectFilled(pos_min + ImVec2(4, 4), pos_max - ImVec2(4, 4), cen_col);
                ImGui::GetWindowDrawList()->AddRect(pos_min, pos_max, ring_col);
            };

            // TODO: the layout is still horrible...
            const ImGuiTableFlags flags_outer =
                ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
            const ImGuiTableFlags flags_inner =
                ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

            if (ImGui::BeginTable("Checklists", 2, flags_outer)) {
                auto checklist = [&](termT_vec& terms, const char* label) {
                    if (ImGui::BeginTable(label, terms.size(), flags_inner)) {
                        ImGui::TableNextRow();
                        for (termT& t : terms) {
                            ImGui::TableNextColumn();
                            imgui_str(t.title);
                            check(t, sqr);
                        }
                        ImGui::EndTable();
                    }
                };

                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    imgui_str("Ignore");

                    ImGui::TableNextColumn();
                    if (ImGui::BeginTable("Checklist##Ignore", 1, flags_inner)) {
                        ImGui::TableNextRow();
                        // TODO: for terms_ignore, use smaller button instead?
                        // const ImVec2 sqr_small{floor(r * 0.9f), floor(r * 0.9f)};

                        // TODO: slightly confusing; light color should represent "take-into-account" instead of
                        // "ignore" Is this solvable by applying specific coloring scheme?
                        ImGui::TableNextColumn();
                        ImGui::BeginGroup();
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1)); // TODO: too tight...
                        for (int l = 0; l < 3; ++l) {
                            check(terms_ignore[l * 3 + 0], sqr);
                            ImGui::SameLine();
                            check(terms_ignore[l * 3 + 1], sqr);
                            ImGui::SameLine();
                            check(terms_ignore[l * 3 + 2], sqr);
                        }
                        ImGui::PopStyleVar();
                        ImGui::EndGroup();

                        // TODO (temp) experimental and unstable...
                        for (int i = 9; i < terms_ignore.size(); ++i) {
                            ImGui::SameLine();
                            ImGui::BeginGroup();
                            imgui_str(terms_ignore[i].title);
                            check(terms_ignore[i], sqr);
                            ImGui::EndGroup();
                        }

                        ImGui::EndTable();
                    }
                }

                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    imgui_str("Native");

                    ImGui::TableNextColumn();
                    checklist(terms_native, "Checklist##Native");
                }

                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    imgui_str("Misc");

                    ImGui::TableNextColumn();
                    checklist(terms_misc, "Checklist##Misc");
                }

                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    imgui_str("qw-    q w\n"
                              "asd ~ a s d\n"
                              "-xc    x c");

                    ImGui::TableNextColumn();
                    checklist(terms_hex, "Checklist##Hex");
                }

                ImGui::EndTable();

                // TODO: or just clear on a per-line basis?
                // TODO: better layout... or right-click menu?
                ImGui::SameLine();
                if (ImGui::Button("Clear")) {
                    for_each_term([&](termT& t) {
                        if (t.selected) {
                            t.selected = false;
                            need_reset = true;
                        }
                    });
                }
            }

            if (need_reset) {
                reset_current();
            }
            return current;
        }
    };
} // namespace legacy

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

// TODO: rename; redesign...
std::optional<std::pair<legacy::ruleT, legacy::lockT>> stone_constraints() {
    enum stateE { Any, F, T, F_Cond, T_Cond }; // TODO: rename; explain
    const int r = 9;
    static stateE board[r][r]{/*Any...*/};

    // modified from partition_collection check button
    auto check = [id = 0, r = ImGui::GetFrameHeight()](stateE& state, bool enable) mutable {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 pos_max = pos + ImVec2{r, r};
        static const ImU32 cols[5]{IM_COL32(100, 100, 100, 255), //
                                   IM_COL32(0, 0, 0, 255),       //
                                   IM_COL32(255, 255, 255, 255), //
                                   IM_COL32(80, 0, 80, 255),     //
                                   IM_COL32(200, 0, 200, 255)};

        ImGui::GetWindowDrawList()->AddRectFilled(pos, pos_max, !enable ? IM_COL32(80, 80, 80, 255) : cols[state]);
        ImGui::GetWindowDrawList()->AddRect(pos, pos_max, IM_COL32(200, 200, 200, 255));

        ImGui::PushID(id++);
        ImGui::InvisibleButton("Button", ImVec2{r, r});
        if (enable && ImGui::IsItemHovered()) {
            // TODO: the ctrl is awkward here...
            if (imgui_scrollup()) {
                switch (state) {
                case Any: state = F; break;
                case F: state = T; break;
                default: state = Any; break;
                }
            }
            if (imgui_scrolldown()) {
                switch (state) {
                case Any: state = F_Cond; break;
                case F_Cond: state = T_Cond; break;
                default: state = Any; break;
                }
            }
        }
        ImGui::PopID();
    };

    if (auto window = imgui_window("Constraints", ImGuiWindowFlags_AlwaysAutoResize)) {
        const bool hit = ImGui::Button("Done");
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            for (auto& l : board) {
                for (auto& s : l) {
                    s = Any;
                }
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
        for (int y = 0; y < r; ++y) {
            bool f = true;
            for (int x = 0; x < r; ++x) {
                if (!f) {
                    ImGui::SameLine();
                }
                f = false;
                check(board[y][x], y >= 1 && y < r - 1 && x >= 1 && x < r - 1);
            }
        }
        ImGui::PopStyleVar();

        if (hit) {
            legacy::ruleT rule{}; // recorder::current?
            legacy::lockT locked{};
            for (int y = 1; y < r - 1; ++y) {
                for (int x = 1; x < r - 1; ++x) {
                    if (board[y][x] != F && board[y][x] != T) {
                        continue;
                    }

                    for_each_code(code) {
                        // TODO: (temp) Eh, took a while to find the [...,x,...] error...
                        // auto [q, w, e, a, s, d, z, X, c] = decode(code);
                        legacy::envT env = decode(code);
                        auto imbue = [](bool& b, stateE state) {
                            if (state == F || state == F_Cond) {
                                b = 0;
                            }
                            if (state == T || state == T_Cond) {
                                b = 1;
                            }
                        };

                        imbue(env.q, board[y - 1][x - 1]);
                        imbue(env.w, board[y - 1][x]);
                        imbue(env.e, board[y - 1][x + 1]);

                        imbue(env.a, board[y][x - 1]);
                        imbue(env.s, board[y][x]);
                        imbue(env.d, board[y][x + 1]);

                        imbue(env.z, board[y + 1][x - 1]);
                        imbue(env.x, board[y + 1][x]);
                        imbue(env.c, board[y + 1][x + 1]);
                        rule[legacy::encode(env)] = board[y][x] == F ? 0 : 1;
                        locked[legacy::encode(env)] = true;
                    }
                }
            }
            return {{rule, locked}};
        }
    }
    return std::nullopt;
}

// TODO: there must be an [obvious] way to support "dial" mode.
// TODO: ideally, `locked` doesn't belong to editor...
// TODO: should be a class... how to decouple? ...
// TODO: the target and lock belongs to a single sync point...
// It's undesirable to return new rule while doing in-place modification on the lock...
std::optional<legacy::ruleT> edit_rule(const legacy::ruleT& target, legacy::lockT& locked, const code_image& icons) {
    std::optional<legacy::ruleT> out;

    static legacy::subset_selector selector;

    // TODO: non-const for set_mask. this is conceptually unsafe as it also allows assignments...
    // TODO: move mask selection logic into selector as well?
    legacy::subsetT& subset = selector.select_subset(target, locked);
    assert(!subset.empty());
    const legacy::partitionT& par = subset.get_par();

    ImGui::Separator();

    // TODO: this part is fairly poorly designed and implemented... redesign...
    // TODO: add more selections...
    // TODO: enable testing masking rule instead of target rule when hovered...
    const legacy::maskT* mask_ptr = nullptr;
    char chr_0 = '0', chr_1 = '1';
    {
        const auto mask_tooltip = [](const legacy::maskT& mask, const char* description) {
            static bool toggle = true;
            if (auto tooltip = imgui_itemtooltip(toggle)) {
                ImGui::PushTextWrapPos(280); // TODO: how to decide wrap pos properly?
                imgui_str("Right click to turn on/off the tooltip");
                imgui_str(description);
                imgui_str(to_MAP_str(mask));
                ImGui::PopTextWrapPos();
            }
        };

        // TODO: should not be here?
        constexpr auto make_id = [](legacy::codeT::bposE bpos) {
            return legacy::make_rule([bpos](legacy::codeT c) { return legacy::get(c, bpos); });
        };
        static const legacy::maskT mask_ids[]{
            make_id(legacy::codeT::env_q), make_id(legacy::codeT::env_w), make_id(legacy::codeT::env_e),
            make_id(legacy::codeT::env_a), make_id(legacy::codeT::env_s), make_id(legacy::codeT::env_d),
            make_id(legacy::codeT::env_z), make_id(legacy::codeT::env_x), make_id(legacy::codeT::env_c)};
        static int id_tag = 4; // s...

        static legacy::maskT mask_custom{{}};

        // TODO: better name...
        static const char* const mask_labels[]{"Zero", "Identity", "????", "Custom"};
        static const char* const mask_descriptions[]{"...", //
                                                     "...", //
                                                     "...", // TODO...
                                                     "..."};
        static int mask_tag = 0;

        const legacy::maskT* mask_ptrs[]{&legacy::mask_zero, &mask_ids[id_tag], &subset.get_mask(), &mask_custom};

        ImGui::AlignTextToFramePadding();
        imgui_str("Mask");
        for (int i = 0; i < 4; ++i) {
            ImGui::SameLine();
            // ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);

            if (i != 1) {
                if (ImGui::RadioButton(mask_labels[i], mask_tag == i)) {
                    mask_tag = i;
                }
            } else {
                if (ImGui::RadioButton(std::format("Identity({})###Identity", "qweasdzxc"[id_tag]).c_str(),
                                       mask_tag == i)) {
                    mask_tag = i;
                }
                // TODO: awkward; use popup window instead?
                if (ImGui::IsItemHovered()) {
                    if (imgui_scrollup()) {
                        id_tag = std::max(id_tag - 1, 0);
                    }
                    if (imgui_scrolldown()) {
                        id_tag = std::min(id_tag + 1, 8);
                    }
                }
            }
            mask_tooltip(*mask_ptrs[i], mask_descriptions[i]);
        }

        ImGui::SameLine();
        if (mask_tag == 3) {
            if (ImGui::Button("Take current rule")) {
                mask_custom = {target};
            }
            mask_tooltip(mask_custom, mask_descriptions[3]);
        } else {
            if (ImGui::Button("Try custom")) {
                mask_tag = 3;
            }
        }

        mask_ptr = mask_ptrs[mask_tag];
        // TODO: horrible...
        if (mask_tag != 0) {
            chr_0 = '.', chr_1 = '!';
        }
    }

    // TODO: (temp) about the lifetime of mask:
    // mask points at either static objects or par.mask, so this should be safe here...
    // still this is a horrible design; need redesign...
    const legacy::maskT& mask = *mask_ptr;

    // TODO: make what to do obvious when !mask_avail !redispatch_avail etc...
    const bool mask_avail = subset.set_mask(mask);
    const bool redispatch_avail = mask_avail && subset.contains(target);

    // TODO: this is disabling all the operations, including mirror, clear-lock etc...
    // What can be allowed when the selected mask doesn't belong to the set?
    if (!mask_avail) {
        ImGui::BeginDisabled();
    }

    {
        if (!redispatch_avail) {
            ImGui::BeginDisabled();
        }

        // TODO: support different rand mode here ("density" and "exact")
        // TODO: still unstable between partition switches...
        // TODO: the range should be scoped by locks... so, what should rcount be?
        static int rcount = 0.5 * par.k();
        const int freec = legacy::count_free(par, locked); // TODO: still wasteful...

        ImGui::SetNextItemWidth(FixedItemWidth);
        imgui_int_slider("##Quantity", &rcount, 0, par.k());
        rcount = std::clamp(rcount, 0, freec);

        // TODO: imgui_innerx...
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        if (imgui_enterbutton("Randomize")) {
            out = legacy::randomize(subset, target, locked, global_mt19937(), rcount, rcount);
        }
        ImGui::SameLine(), imgui_str("|"), ImGui::SameLine();
        if (imgui_enterbutton("Shuffle")) {
            out = legacy::shuffle(subset, target, locked, global_mt19937());
        }

        {
            // TODO; experimental...
            ImGui::SetNextItemWidth(FixedItemWidth);
            static float density = 0.5;
            ImGui::SliderFloat("##Density", &density, 0, 1, std::format("Around {}", round(density * par.k())).c_str(),
                               ImGuiSliderFlags_NoInput);
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            if (imgui_enterbutton("Randomize(V2)")) {
                out = legacy::randomize_v2(subset, target, locked, global_mt19937(), density);
            }
        }

        iter_pair(
            "<00..", "dec", "inc", "11..>", //
            [&] { out = legacy::act_int::first(subset, target, locked); },
            [&] { out = legacy::act_int::prev(subset, target, locked); },
            [&] { out = legacy::act_int::next(subset, target, locked); },
            [&] { out = legacy::act_int::last(subset, target, locked); });
        ImGui::SameLine(), imgui_str("|"), ImGui::SameLine();
        iter_pair(
            "<1.0.", "pprev", "pnext", "0.1.>", //
            [&] { out = legacy::act_perm::first(subset, target, locked); },
            [&] { out = legacy::act_perm::prev(subset, target, locked); },
            [&] { out = legacy::act_perm::next(subset, target, locked); },
            [&] { out = legacy::act_perm::last(subset, target, locked); });

        // TODO: (temp) new line begins here...
        // TODO: enhance might be stricter than necessary...
        if (ImGui::Button("Enhance locks")) {
            legacy::enhance(subset, target, locked);
        }
        if (!redispatch_avail) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear locks")) {
            locked = {};
        }
        ImGui::SameLine();
        if (ImGui::Button("Approximate")) {
            if (legacy::compatible(subset, target, locked)) {
                out = legacy::approximate(subset, target, locked);
            } else {
                logger::log_temp(300ms, "Incompatible ... TODO"); // TODO refine...
            }
        }
        ImGui::SameLine();
        // TODO: move elsewhere
        if (ImGui::Button("Mir")) {
            out = legacy::mirror_v2(target, locked);
        }
    }

    // TODO: support filtering?
    {
        const char labels[2][3]{{'-', chr_0, '\0'}, {'-', chr_1, '\0'}};

        // TODO: find a better name for `ruleT_masked` and the variables...
        const legacy::ruleT_masked masked = mask ^ target;
        const auto scanlist = legacy::scan(par, masked, locked);
        {
            // TODO: add more statistics... e.g. full vs partial lock...
            const int c_group = par.k();
            int c_locked = 0;
            int c_0 = 0, c_1 = 0;
            int c_inconsistent = 0;
            for (const auto& scan : scanlist) {
                if (scan.any_locked()) {
                    ++c_locked;
                }
                if (scan.all_0()) {
                    ++c_0;
                }
                if (scan.all_1()) {
                    ++c_1;
                }
                if (scan.inconsistent()) {
                    ++c_inconsistent;
                }
            }
            // TODO: locked{all-0,all-1,inc}, unlocked{all-0,all-1...} etc...
            ImGui::Text("Groups:%d (Locked:%d) [%c:%d] [%c:%d] [%c:%d]", c_group, c_locked, chr_0, c_0, chr_1, c_1, 'x',
                        c_inconsistent);
        }

        if (auto child = imgui_childwindow("Details")) {
            const int zoom = 7;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            par.for_each_group([&](int j, const legacy::groupT& group) {
                if (j % 8 != 0) {
                    ImGui::SameLine();
                }
                if (j != 0 && j % 64 == 0) {
                    ImGui::Separator(); // TODO: refine...
                }
                const bool inconsistent = scanlist[j].inconsistent();
                const legacy::codeT head = group[0];
                const bool has_lock = scanlist[j].any_locked();

                if (inconsistent) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0, 0, 1));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0, 0, 1));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0, 0, 1));
                }
                const bool button_hit = icons.button(head, zoom);
                const bool button_hover = ImGui::IsItemHovered();
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                imgui_str(labels[masked[head]]);
                // (wontfix) The vertical alignment is imprecise here. For precise alignment see:
                // https://github.com/ocornut/imgui/issues/2064

                if (has_lock) {
                    // TODO: -> widget func... (addborder)
                    const ImU32 col = scanlist[j].all_locked() ? IM_COL32_WHITE : IM_COL32(128, 128, 128, 255);
                    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin() - ImVec2(2, 2),
                                                        ImGui::GetItemRectMax() + ImVec2(2, 2), col);
                }
                if (inconsistent) {
                    ImGui::PopStyleColor(3);
                }

                if (button_hover && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    for (auto code : group) {
                        locked[code] = !has_lock; // TODO: not reversible; is this ok?
                    }
                }
                ImGui::SameLine();
                imgui_strdisabled("?");
                {
                    // TODO: (temp) the application of imgui_itemtooltip unnecessarily brought End/Begin Disabled
                    // to each group...

                    // TODO: transparency of the tooltip is also affected if in disabled block... Is the effect
                    // intentional / configurable?
                    if (!mask_avail) {
                        ImGui::EndDisabled();
                    }
                    static bool toggle = true;
                    if (auto tooltip = imgui_itemtooltip(toggle)) {
                        imgui_str("Right click to turn on/off the tooltip");
                        ImGui::Text("Group size: %d", (int)group.size());
                        const int max_to_show = 40;
                        for (int x = 0; auto code : group) {
                            if (x++ % 8 != 0) {
                                ImGui::SameLine();
                            }
                            // TODO: change color?
                            // ImGui::GetStyle().Colors[ImGuiCol_Button]
                            icons.image(code, zoom, ImVec4(1, 1, 1, 1), ImVec4(0.5, 0.5, 0.5, 1));
                            ImGui::SameLine();
                            ImGui::AlignTextToFramePadding();
                            imgui_str(labels[masked[code]]);
                            if (locked[code]) {
                                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin() - ImVec2(2, 2),
                                                                    ImGui::GetItemRectMax() + ImVec2(2, 2), -1);
                            }
                            if (x == max_to_show) {
                                break;
                            }
                        }
                        if (group.size() > max_to_show) {
                            imgui_str("...");
                        }
                    }
                    if (!mask_avail) {
                        ImGui::BeginDisabled();
                    }
                }
                if (button_hit) {
                    // TODO: document this behavior... (keyctrl->resolve conflicts)
                    // TODO: reconsider how to deal with conflicts... (especially via masking rule...)
                    legacy::ruleT r = target;
                    if (ImGui::GetIO().KeyCtrl) {
                        legacy::copy(group, mask, r);
                    } else {
                        legacy::flip(group, r);
                    }
                    out = r;
                }
            });
            ImGui::PopStyleVar(2);
        }
    }
    if (!mask_avail) {
        ImGui::EndDisabled();
    }
    return out;
}

// TODO: "paste" should have a similar widget...
class file_nav_with_recorder {
    file_nav m_nav;
    std::optional<std::filesystem::path> m_file;
    rule_recorder m_recorder;

public:
    void display(rule_recorder& out) {
        // TODO: refresh... / reset(clear)...
        // TODO: set as current_path... ./ copy_path...
        // TODO: notify that all the operations are [read-only...]
        bool hit = false;

        if (!m_file) {
            ImGui::BeginDisabled();
        }
        // TODO: using childwindow only for padding...
        if (auto child = imgui_childwindow("Recorder", {},
                                           ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY)) {
            if (m_file) {
                // TODO: or wrapped full path?
                imgui_str(cpp17_u8string(m_file->filename()));
            } else {
                imgui_str("...");
            }

            // TODO: +1 is clumsy
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Total:%d At:%d", m_recorder.size(), m_recorder.pos() + 1);
            ImGui::SameLine();

            iter_pair(
                "<|", "prev", "next", "|>", //
                [&] { m_recorder.set_first(), hit = true; }, [&] { m_recorder.set_prev(), hit = true; },
                [&] { m_recorder.set_next(), hit = true; }, [&] { m_recorder.set_last(), hit = true; });
        }
        if (!m_file) {
            ImGui::EndDisabled();
        }
        if (ImGui::IsItemClicked()) {
            assert(m_file);
            hit = true;
        }

        if (hit) {
            assert(m_file);
            out.take(m_recorder.current());
        }

        {
            // TODO: find better ways to show sync...
            const bool sync = m_file && (m_recorder.current() == out.current());
            const ImU32 col = sync ? IM_COL32_WHITE : ImGui::GetColorU32(ImGuiCol_Separator);
            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), col);
        }

        // TODO: better layout...
        if (auto sel = m_nav.display()) {
            auto result = legacy::extract_rules(load_binary(*sel, 1'000'000));
            if (!result.empty()) {
                logger::log_temp(500ms, "found {} rules", result.size());
                m_recorder.replace(std::move(result));
                out.take(m_recorder.current()); // Sync with `out`.
                m_file = std::move(sel);
            } else {
                logger::log_temp(500ms, "found nothing");
            }
        }
    }
};

// TODO: reconsider: where should "current-rule" be located...
struct runner_ctrl {
    legacy::ruleT rule;
    legacy::lockT locked;
    // TODO: temporal pos; the sync point ((ruleT,lockT) pair) shouldn't belong to runner_ctrl...

    // TODO: better name?
    static constexpr int pace_min = 1, pace_max = 20;
    int pace = 1;
    bool anti_flick = true; // TODO: add explanation
    int actual_pace() const {
        if (anti_flick && legacy::will_flick(rule) && pace % 2) {
            return pace + 1;
        }
        return pace;
    }

    // TODO: remove this feature?
    static constexpr int start_min = 0, start_max = 200;
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
                    runner.run(rule, actual_pace());
                }
            }
        }
    }
};

// TODO: are there portable ways to convert argv to a valid filesystem::path (without messing up encodings)?
int main(int argc, char** argv) {
    app_backend::init();
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
        // TODO: IniFilename and LogFilename should be unconditionally fixed (even if not using base-path)
        // (wontfix) These memory leaks are negligible.
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

    rule_recorder recorder;

    // TODO: redesign...
    tileT_filler filler{.use_seed = true, .seed = 0, .density = 0.5};
    torusT runner({.width = 480, .height = 360});
    runner.restart(filler);

    runner_ctrl ctrl{.rule = recorder.current(),
                     .locked = {},
                     .pace = 1,
                     .anti_flick = true,
                     .start_from = 0,
                     .gap_frame = 0,
                     .pause = false};

    bool show_nav_window = true;
    file_nav_with_recorder nav;

    code_image icons;

    tile_image img;
    ImVec2 img_off = {0, 0}; // TODO: supposed to be of integer-precision...
    int img_zoom = 1;

    struct pasteT : public std::optional<legacy::tileT> {
        legacy::tileT::posT pos{0, 0}; // dbegin for copy... (TODO: this is confusing...)
    };
    pasteT paste;

    // TODO: rename; move elsewhere...
    // TODO: ctrl to move selected area?
    struct selectT {
        // []
        legacy::tileT::posT select_0{0, 0}, select_1{0, 0}; // cell index, not pixel.

        void clear() { select_0 = select_1 = {0, 0}; }
        void toggle_select_all(legacy::tileT::sizeT size) {
            // all-selected ? clear : select-all
            const auto [begin, end] = range();
            if (begin.x == 0 && begin.y == 0 && end.x == size.width && end.y == size.height) {
                clear();
            } else {
                select_0 = {0, 0};
                select_1 = {.x = size.width - 1, .y = size.height - 1};
            }
        }

        legacy::tileT::rangeT range() const {
            int x0 = select_0.x, x1 = select_1.x;
            int y0 = select_0.y, y1 = select_1.y;
            if (x0 > x1) {
                std::swap(x0, x1);
            }
            if (y0 > y1) {
                std::swap(y0, y1);
            }
            return {.begin{x0, y0}, .end{x1 + 1, y1 + 1}};
        }
    };
    selectT sel{};

    while (app_backend::begin_frame()) {
        // TODO: applying following logic; consider refining it.
        // (there should be a single sync point to represent current rule (and lock)...)
        // recorder is modified during display, but will synchronize with runner's before next frame.
        assert(ctrl.rule == recorder.current());

        if (show_nav_window) {
            if (auto window = imgui_window("File nav", &show_nav_window)) {
                nav.display(recorder);
            }
        }

        // TODO: let right+ctrl move the block?

        // TODO: set pattern as init state? what if size is already changed?
        // TODO: specify mouse-dragging behavior (especially, no-op must be an option)
        // TODO: range-selected randomization don't need fixed seed. However, there should be a way to specify density.
        // TODO: support drawing as a dragging behavior if easy.
        // TODO: copy vs copy to clipboard; paste vs paste from clipboard? (don't want to pollute clipboard with small
        // rle strings...
        // TODO: should be able to recognize "rule = " part in the rle string.

        // TODO: "periodical tile" feature is generally not too useful without boundless space, and torus is
        // enough for visual feedback.
        auto show_tile = [&] {
            // TODO: refine "resize" gui and logic...
            static char input_width[20]{}, input_height[20]{};
            const float s = ImGui::GetStyle().ItemInnerSpacing.x;
            {
                const auto filter = [](ImGuiInputTextCallbackData* data) -> int {
                    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
                };
                const float w = (ImGui::CalcItemWidth() - s) / 2;
                ImGui::SetNextItemWidth(w);
                ImGui::InputTextWithHint("##Width", "width", input_width, 20, ImGuiInputTextFlags_CallbackCharFilter,
                                         filter);
                ImGui::SameLine(0, s);
                ImGui::SetNextItemWidth(w);
                ImGui::InputTextWithHint("##Height", "height", input_height, 20, ImGuiInputTextFlags_CallbackCharFilter,
                                         filter);
                ImGui::SameLine(0, s);
            }
            const bool resize = ImGui::Button("Resize");

            // TODO: move elsewhere in the gui?
            ImGui::Text("Width:%d,Height:%d,Gen:%d,Density:%.4f", runner.tile().width(), runner.tile().height(),
                        runner.gen(), float(legacy::count(runner.tile())) / runner.tile().area());
            // TODO: canvas size, tile size, selected size, paste size...

            const bool corner = ImGui::Button("Corner"); // TODO: move elsewhere...
            ImGui::SameLine();
            const bool center = ImGui::Button("Center");
            ImGui::SameLine();
            imgui_str("Zoom");
            assert(img_zoom == 1 || img_zoom == 2 || img_zoom == 4 || img_zoom == 8);
            for (const int z : {1, 2, 4, 8}) {
                ImGui::SameLine(0, s);
                // TODO: avoid usage of format...
                if (ImGui::RadioButton(std::format("{}##Z{}", z, z).c_str(), img_zoom == z)) {
                    img_off = {8, 8}; // TODO: temporarily intentional...
                    img_zoom = z;
                }
            }

            ImGui::SameLine();
            const bool fit = ImGui::Button("Fit"); // TODO: size preview?
            // TODO: select all, unselect button...

            if (paste) {
                ImGui::SameLine(), imgui_str("|"), ImGui::SameLine();
                if (ImGui::Button("Drop paste")) {
                    paste.reset();
                }
            }

            const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
            const ImVec2 canvas_size = ImGui::GetContentRegionAvail();

            // TODO: this can be negative wtf... investigate...
            // TODO: (regression?) some key ctrls are skipped by this...
            if (canvas_size.x <= 0 || canvas_size.y <= 0) {
                // TODO: show something when fit/etc are hit?
                ctrl.pause2 = true;
                // ImGui::Text("canvas: x=%f y=%f", canvas_size.x,canvas_size.y);
                return;
            }
            // TODO: or like this?
            // const ImVec2 canvas_size = [] {
            //     // Values of GetContentRegionAvail() can be negative...
            //     ImVec2 size = ImGui::GetContentRegionAvail();
            //     return ImVec2(std::max(size.x, 50.0f), std::max(size.y, 50.0f));
            // }();

            // TODO: the constraint is arbitrary; are there more sensible ways to decide size constraint?
            const auto size_clamped = [](int width, int height) {
                return legacy::tileT::sizeT{.width = std::clamp(width, 64, 1200),
                                            .height = std::clamp(height, 64, 1200)};
            };
            if (fit) {
                img_off = {0, 0};

                const legacy::tileT::sizeT size =
                    size_clamped((int)canvas_size.x / img_zoom, (int)canvas_size.y / img_zoom);
                if (runner.tile().size() != size) {
                    runner.restart(filler, size);
                    // TODO: how to support background period then?
                    assert(runner.tile().size() == size); // ???
                }
            }
            if (resize) {
                // TODO: support using current screen/tilesize/zoom?
                int iwidth = 0, iheight = 0;
                const auto [ptr, ec] = std::from_chars(input_width, input_width + 20, iwidth);
                const auto [ptr2, ec2] = std::from_chars(input_height, input_height + 20, iheight);
                if (ec == std::errc{} && ec2 == std::errc{}) {
                    img_off = {0, 0};
                    img_zoom = 1; // <-- TODO: whether to reset zoom here?
                    const legacy::tileT::sizeT size = size_clamped(iwidth, iheight);
                    if (runner.tile().size() != size) {
                        runner.restart(filler, size);
                    }
                }
                // else ...
                input_width[0] = '\0';
                input_height[0] = '\0';
            }

            // Size is fixed now:
            const legacy::tileT::sizeT tile_size = runner.tile().size();
            const ImVec2 img_size(tile_size.width * img_zoom, tile_size.height * img_zoom);

            // Validity of paste is fixed now:
            // This can happen when e.g. paste -> resize...
            if (paste && (paste->width() > tile_size.width || paste->height() > tile_size.height)) {
                paste.reset();
            }

            if (corner) {
                img_off = {0, 0};
            }
            if (center) {
                img_off = canvas_size / 2 - img_size / 2;
                img_off.x = floor(img_off.x);
                img_off.y = floor(img_off.y); // TODO: is flooring correct?
            }

            const ImVec2 img_pos = canvas_pos + img_off;

            ImDrawList* const drawlist = ImGui::GetWindowDrawList();
            drawlist->PushClipRect(canvas_pos, canvas_pos + canvas_size);
            drawlist->AddRectFilled(canvas_pos, canvas_pos + canvas_size, IM_COL32(20, 20, 20, 255));

            if (!paste) {
                drawlist->AddImage(img.update(runner.tile()), img_pos, img_pos + img_size);
            } else {
                // TODO: displays poorly with miniwindow...

                assert(paste->width() <= tile_size.width && paste->height() <= tile_size.height);
                paste.pos.x = std::clamp(paste.pos.x, 0, tile_size.width - paste->width());
                paste.pos.y = std::clamp(paste.pos.y, 0, tile_size.height - paste->height());
                // TODO: rename...
                const legacy::tileT::rangeT range = {paste.pos, paste.pos + paste->size()};

                legacy::tileT temp(paste->size()); // TODO: wasteful...
                legacy::copy(temp, {0, 0}, runner.tile(), range);
                legacy::copy<legacy::copyE::Or>(runner.tile(), paste.pos, *paste);

                drawlist->AddImage(img.update(runner.tile()), img_pos, img_pos + img_size);

                legacy::copy(runner.tile(), paste.pos, temp);

                const ImVec2 paste_min = img_pos + ImVec2(range.begin.x, range.begin.y) * img_zoom;
                const ImVec2 paste_max = img_pos + ImVec2(range.end.x, range.end.y) * img_zoom;
                drawlist->AddRectFilled(paste_min, paste_max, IM_COL32(255, 0, 0, 60));
            }

            if (const auto range = sel.range(); range.width() > 1 || range.height() > 1) {
                const ImVec2 sel_min = img_pos + ImVec2(range.begin.x, range.begin.y) * img_zoom;
                const ImVec2 sel_max = img_pos + ImVec2(range.end.x, range.end.y) * img_zoom;
                drawlist->AddRectFilled(sel_min, sel_max, IM_COL32(0, 255, 0, 40));
                drawlist->AddRect(sel_min, sel_max, IM_COL32(0, 255, 0, 160));
            }
            drawlist->PopClipRect();

            ImGui::InvisibleButton("Canvas", canvas_size);
            const bool active = ImGui::IsItemActive();
            ctrl.pause2 = paste || active; // TODO: won't work if |= active... should ctrl.run clear pause2?
            if (ImGui::IsItemHovered()) {
                // TODO: reorganize...
                const ImGuiIO& io = ImGui::GetIO();

                assert(ImGui::IsMousePosValid());
                // It turned out that, this will work well even if outside of the image...
                const ImVec2 mouse_pos = io.MousePos;
                if (active) {
                    if (!io.KeyCtrl) {
                        img_off += io.MouseDelta;
                    } else if (img_zoom == 1) {
                        runner.shift(io.MouseDelta.x, io.MouseDelta.y);
                    }
                }

                // TODO: refine...
                // TODO: zoom window temporarily conflicts with range selection... (both use Rclick)
                if (!paste && img_zoom <= 2 && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                    const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);
                    if (celx >= -10 && celx < tile_size.width + 10 && cely >= -10 && cely < tile_size.height + 10) {
                        static bool toggle = true;
                        if (auto tooltip = imgui_itemtooltip(toggle)) {
                            // int minx = std::max(celx - 20, 0);
                            // int miny = std::max(cely - 20, 0);
                            // int maxx = std::min(celx + 20, tile_size.width);
                            // int maxy = std::min(cely + 20, tile_size.height);

                            // TODO: simplify...
                            assert(tile_size.width >= 40);
                            assert(tile_size.height >= 40);
                            int minx = celx - 20, miny = cely - 20;
                            int maxx = celx + 20, maxy = cely + 20;
                            if (minx < 0) {
                                minx = 0, maxx = 40;
                            }
                            if (miny < 0) {
                                miny = 0, maxy = 40;
                            }
                            if (maxx > tile_size.width) {
                                minx = tile_size.width - 40, maxx = tile_size.width;
                            }
                            if (maxy > tile_size.height) {
                                miny = tile_size.height - 40, maxy = tile_size.height;
                            }
                            assert(maxx - minx == 40 && maxy - miny == 40);

                            const int w = maxx - minx, h = maxy - miny;
                            ImGui::PushTextWrapPos(200);
                            imgui_str("Right click to turn on/off the tooltip");
                            ImGui::PopTextWrapPos();
                            ImGui::Image(img.texture(), ImVec2(40 * 4, 40 * 4),
                                         {(float)minx / tile_size.width, (float)miny / tile_size.height},
                                         {(float)maxx / tile_size.width, (float)maxy / tile_size.height});
                        }
                    }
                }

                // TODO: this shall belong to the runner.
                // TODO: precedence against left-clicking?
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    // ctrl.pause = true;
                    const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                    const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);

                    sel.select_0.x = std::clamp(celx, 0, tile_size.width - 1);
                    sel.select_0.y = std::clamp(cely, 0, tile_size.height - 1);
                }
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                    const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);

                    sel.select_1.x = std::clamp(celx, 0, tile_size.width - 1);
                    sel.select_1.y = std::clamp(cely, 0, tile_size.height - 1);
                }
                if (paste) {
                    const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                    const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);

                    // TODO: can width<paste.width here?
                    paste.pos.x = std::clamp(celx, 0, tile_size.width - paste->width());
                    paste.pos.y = std::clamp(cely, 0, tile_size.height - paste->height());

                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        legacy::copy<legacy::copyE::Or>(runner.tile(), paste.pos, *paste);
                        paste.reset();
                    }
                }

                // TODO (temp) moved here to avoid affecting other utils (which rely on img_pos)
                if (imgui_scrolling()) {
                    const ImVec2 cellidx = (mouse_pos - img_pos) / img_zoom;
                    if (imgui_scrolldown() && img_zoom != 1) {
                        img_zoom /= 2;
                    }
                    if (imgui_scrollup() && img_zoom != 8) {
                        img_zoom *= 2;
                    }
                    img_off = (mouse_pos - cellidx * img_zoom) - canvas_pos;
                    img_off.x = round(img_off.x);
                    img_off.y = round(img_off.y); // TODO: is rounding correct?
                }
            }

            // TODO: "shrink selection" utility?
            if (imgui_keypressed(ImGuiKey_A, false)) {
                sel.toggle_select_all(tile_size);
            }
            if (imgui_keypressed(ImGuiKey_V, false)) {
                if (const char* text = ImGui::GetClipboardText()) {
                    try {
                        // TODO: or ask whether to resize runner.tile?
                        paste.emplace(legacy::from_RLE_str(text, tile_size));
                    } catch (const std::exception& err) {
                        logger::log_temp(2500ms, "{}", err.what());
                    }
                }
            }
            if (const auto range = sel.range(); range.height() > 1 || range.width() > 1) {
                if (imgui_keypressed(ImGuiKey_C, false) || imgui_keypressed(ImGuiKey_X, false)) {
                    ImGui::SetClipboardText(legacy::to_RLE_str(ctrl.rule, runner.tile(), range).c_str());
                }
                if (imgui_keypressed(ImGuiKey_Backspace, false) || imgui_keypressed(ImGuiKey_X, false)) {
                    // TODO: 0/1.../ agar...
                    legacy::clear_inside(runner.tile(), range, 0);
                }
                if (imgui_keypressed(ImGuiKey_Equal, false)) {
                    // TODO: specify density etc... or share with tileT_filler?
                    legacy::random_fill(runner.tile(), global_mt19937(), 0.5, range);
                }
                // TODO: redesign keyboard ctrl...
                if (imgui_keypressed(ImGuiKey_0, false)) {
                    legacy::clear_outside(runner.tile(), range, 0);
                }

                // TODO: refine capturing...
                if (imgui_keypressed(ImGuiKey_P, false)) {
                    // TODO: support specifying padding area...
                    legacy::tileT cap(range.size()), cap2(range.size());
                    legacy::copy(cap, {0, 0}, runner.tile(), range);
                    legacy::lockT locked{};
                    auto rulx = [&](legacy::codeT code) {
                        locked[code] = true;
                        return ctrl.rule[code];
                    };
                    // TODO: how to decide generation?
                    for (int g = 0; g < 50; ++g) {
                        cap.gather(cap, cap, cap, cap, cap, cap, cap, cap);
                        cap.apply(rulx, cap2);
                        cap.swap(cap2);
                    }
                    ctrl.locked = locked;
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
            ImGui::Checkbox("Nav window", &show_nav_window);
            ImGui::SameLine();
            // TODO: change color when is too fps is too low...
            ImGui::Text("   (%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());

            show_target_rule(ctrl.rule, recorder);
            ImGui::Separator();

            if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (auto child = imgui_childwindow("Rul")) {
                    if (auto out = stone_constraints()) {
                        auto& [rule, lock] = *out;
                        recorder.take(rule);
                        ctrl.locked = lock;
                    }
                    if (auto out = edit_rule(ctrl.rule, ctrl.locked, icons)) {
                        recorder.take(*out);
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
                    ImGui::PushItemWidth(FixedItemWidth);
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

                        // TODO: visual feedback...
                        if (ImGui::Button("+1")) {
                            extra = 1;
                        }
                        ImGui::SameLine();
                        // TODO: is this usage of ### correct?
                        // (Correct, but usage of format might be a bad idea here...)
                        if (ImGui::Button(std::format("+p({})###+p", ctrl.actual_pace()).c_str())) {
                            extra = ctrl.actual_pace();
                        }
                        ImGui::PopButtonRepeat();
                        ImGui::SameLine();
                        if (ImGui::Button("Restart") || imgui_keypressed(ImGuiKey_R, false)) {
                            restart = true;
                        }

                        // TODO: Gap-frame shall be really timer-based...
                        ImGui::SliderInt("Gap Frame (0~20)", &ctrl.gap_frame, ctrl.gap_min, ctrl.gap_max, "%d",
                                         ImGuiSliderFlags_NoInput);
                        // TODO: move elsewhere...
                        imgui_int_slider("Start gen (0~200)", &ctrl.start_from, ctrl.start_min, ctrl.start_max);
                        imgui_int_slider("Pace (1~20)", &ctrl.pace, ctrl.pace_min, ctrl.pace_max);
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("(Actual pace: %d)", ctrl.actual_pace());
                        ImGui::SameLine();
                        ImGui::Checkbox("anti-flick", &ctrl.anti_flick);
                    }
                    ImGui::EndGroup();
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    {
                        // TODO: use radio instead?
                        if (ImGui::Checkbox("Use seed", &filler.use_seed)) {
                            // TODO: unconditional?
                            if (filler.use_seed) {
                                restart = true;
                            }
                        }
                        if (!filler.use_seed) {
                            ImGui::BeginDisabled();
                        }
                        if (int seed = filler.seed; imgui_int_slider("Seed (0~99)", &seed, 0, 99)) {
                            filler.seed = seed;
                            restart = true;
                        }
                        if (!filler.use_seed) {
                            ImGui::EndDisabled();
                        }

                        // TODO: integer(ratio) density?
                        if (ImGui::SliderFloat("Init density (0~1)", &filler.density, 0.0f, 1.0f, "%.3f",
                                               ImGuiSliderFlags_NoInput)) {
                            restart = true;
                        }
                    }
                    ImGui::EndGroup();

                    show_tile();
                    ImGui::PopItemWidth();

                    // TODO: enable/disable keyboard ctrl (enable by default)
                    // TODO: redesign keyboard ctrl...
                    if (imgui_keypressed(ImGuiKey_1, true)) {
                        ctrl.gap_frame = std::max(ctrl.gap_min, ctrl.gap_frame - 1);
                    }
                    if (imgui_keypressed(ImGuiKey_2, true)) {
                        ctrl.gap_frame = std::min(ctrl.gap_max, ctrl.gap_frame + 1);
                    }
                    if (imgui_keypressed(ImGuiKey_3, true)) {
                        ctrl.pace = std::max(ctrl.pace_min, ctrl.pace - 1);
                    }
                    if (imgui_keypressed(ImGuiKey_4, true)) {
                        ctrl.pace = std::min(ctrl.pace_max, ctrl.pace + 1);
                    }
                    // TODO: explain... apply to other ctrls?
                    if ((ctrl.pause2 || !ImGui::GetIO().WantCaptureKeyboard) &&
                        ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
                        ctrl.pause = !ctrl.pause;
                    }
                    // TODO: temp (this function turns out to be necessary...)
                    if (imgui_keypressed(ImGuiKey_M, true)) {
                        if (ctrl.pause) {
                            extra = ctrl.actual_pace();
                        }
                        ctrl.pause = true;
                    }
                }
                ImGui::EndTable();
            }
        }

        ImGui::ShowDemoWindow(); // TODO: remove (or comment-out) this when all done...
        logger::tempwindow();
        app_backend::end_frame();

        if (ctrl.rule != recorder.current()) {
            ctrl.rule = recorder.current();
            restart = true;
            ctrl.pause = false; // TODO: this should be configurable...
        }
        if (restart) {
            runner.restart(filler);
        }
        ctrl.run(runner, extra); // TODO: able to result in low fps...
    }

    app_backend::clear();
    return 0;
}
