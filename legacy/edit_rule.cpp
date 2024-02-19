#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "rule_algo.hpp"

#include "common.hpp"

// TODO: support rollbacking diff rules?
// TODO: for editing opt, support in-lock and outof-lock mode?

// TODO: currently poorly designed...
#define ENABLE_STATIC_CONSTRAINTS

// TODO: consider other approaches (native nav etc) if possible...
// TODO: e.g. toggle between buttons by left/right... / clear binding...
inline bool enter_button(const char* label) {
    static ImGuiID bind_id = 0;
    bool ret = ImGui::Button(label);
    const ImGuiID button_id = ImGui::GetItemID();
    if (ret) {
        bind_id = button_id;
    }
    // TODO: are there public ways (not relying on im gui_internal.h)
    // to detect whether in disabled block?
    if (bind_id == button_id && (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0) {
        if (imgui_keypressed(ImGuiKey_Enter, false)) {
            ret = true;
        }
        const ImU32 col = ret ? IM_COL32(128, 128, 128, 255) : IM_COL32_WHITE;
        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), col);
    }
    return ret;
}

class subset_selector {
    legacy::subsetT current;

    struct termT {
        const char* title;
        legacy::subsetT set;
        bool selected = false;
        bool includes_cur = false;
        bool disabled = false; // current & set -> empty.
        const char* description = nullptr;
    };

    using termT_vec = std::vector<termT>;

    termT_vec terms_ignore; // TODO: rename...
    termT_vec terms_misc;
    termT_vec terms_native;
    termT_vec terms_totalistic;
    termT_vec terms_hex;
    // TODO: about the plan to support user-defined subsets...

    void for_each_term(const auto& fn) {
        for (termT_vec* terms : {&terms_ignore, &terms_misc, &terms_native, &terms_totalistic, &terms_hex}) {
            for (termT& t : *terms) {
                fn(t);
            }
        }
    }

    void reset_current() {
        current = legacy::subsetT::universalT{};

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
    subset_selector() : current(legacy::subsetT::universalT{}) {
        using namespace legacy::recipes;
        // TODO: add some tests after the construction...

        terms_ignore.emplace_back("q", make_subset({mp_ignore_q}));
        terms_ignore.emplace_back("w", make_subset({mp_ignore_w}));
        terms_ignore.emplace_back("e", make_subset({mp_ignore_e}));
        terms_ignore.emplace_back("a", make_subset({mp_ignore_a}));
        terms_ignore.emplace_back("s", make_subset({mp_ignore_s}, mask_zero)).description =
            "0->1, 1->1 or 0->0, 1->0"; // TODO: whether to place here?
        terms_ignore.emplace_back("d", make_subset({mp_ignore_d}));
        terms_ignore.emplace_back("z", make_subset({mp_ignore_z}));
        terms_ignore.emplace_back("x", make_subset({mp_ignore_x}));
        terms_ignore.emplace_back("c", make_subset({mp_ignore_c}));

        terms_misc.emplace_back("S(f)", make_subset({mp_ignore_s}, mask_identity)).description =
            "0->0, 1->1 or 0->1, 1->0";
        terms_misc.emplace_back("Hex", make_subset({mp_hex_ignore}));
        // TODO: or define mp_von_ignore?
        terms_misc.emplace_back("Von", make_subset({mp_ignore_q, mp_ignore_e, mp_ignore_z, mp_ignore_c}));
        terms_misc.emplace_back("Dual", make_subset({mp_dual}, mask_identity)); // Self-complementary
        terms_misc.emplace_back("'C8'", make_subset({mp_C8}));                  // TODO: whether to expose this?

        terms_native.emplace_back("All", make_subset({mp_refl_wsx, mp_refl_qsc}));
        terms_native.emplace_back("|", make_subset({mp_refl_wsx}));
        terms_native.emplace_back("-", make_subset({mp_refl_asd}));
        terms_native.emplace_back("\\", make_subset({mp_refl_qsc}));
        terms_native.emplace_back("/", make_subset({mp_refl_esz}));
        terms_native.emplace_back("C2", make_subset({mp_C2}));
        terms_native.emplace_back("C4", make_subset({mp_C4})); // TODO: add explanations in the gui

        terms_totalistic.emplace_back("Tot", make_subset({mp_C8, mp_tot_exc_s}));
        terms_totalistic.emplace_back("Tot(+s)", make_subset({mp_C8, mp_tot_inc_s}));
        terms_totalistic.emplace_back("Hex_Tot", make_subset({mp_hex_C6, mp_hex_tot_exc_s}));
        terms_totalistic.emplace_back("Hex_Tot(+s)", make_subset({mp_hex_C6, mp_hex_tot_inc_s}));

        terms_hex.emplace_back("All", make_subset({mp_hex_refl_asd, mp_hex_refl_aq}));
        terms_hex.emplace_back("a-d", make_subset({mp_hex_refl_asd}));
        terms_hex.emplace_back("q-c", make_subset({mp_hex_refl_qsc}));
        terms_hex.emplace_back("w-x", make_subset({mp_hex_refl_wsx}));
        terms_hex.emplace_back("a|q", make_subset({mp_hex_refl_aq}));
        terms_hex.emplace_back("q|w", make_subset({mp_hex_refl_qw}));
        terms_hex.emplace_back("w|d", make_subset({mp_hex_refl_wd}));
        terms_hex.emplace_back("C2", make_subset({mp_hex_C2}));
        terms_hex.emplace_back("C3", make_subset({mp_hex_C3}));
        terms_hex.emplace_back("C6", make_subset({mp_hex_C6}));

        reset_current();
    }

    legacy::subsetT& select_subset(const legacy::moldT& mold) {
        bool need_reset = false;
        auto set_select = [&need_reset](termT& t, bool sel) {
            if (t.selected != sel) {
                t.selected = sel;
                need_reset = true;
            }
        };

        if (ImGui::Button("Clear")) {
            for_each_term([&](termT& t) { set_select(t, false); });
        }
        ImGui::SameLine();
        if (ImGui::Button("Recognize")) {
            for_each_term([&](termT& t) { set_select(t, t.set.contains(mold.rule)); });
        }

        // TODO: drop mutable id... use manually specified ids
        // TODO: tooltip...
        auto check = [&, id = 0, size = square_size()](termT& term) mutable {
            ImGui::PushID(id++);
            // ImGui::PushID(&term); // TODO: is ptr id reliable? (will this possibly cause collisions?)
            if (ImGui::InvisibleButton("Check", size) && !term.disabled) {
                set_select(term, !term.selected);
            }
            ImGui::PopID();

            if (term.description) {
                static bool toggle = true;
                if (auto tooltip = imgui_itemtooltip(toggle)) {
                    imgui_str(term.description);
                }
            }

            // TODO: change color when hovered?
            // const bool hovered = ImGui::IsItemHovered();

            // TODO: explain coloring scheme; redesign if necessary (especially ring col)
            // TODO: find better color for "disabled"/incompatible etc... currently too ugly.
            const ImU32 cen_col = term.selected       ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                  : term.includes_cur ? ImGui::GetColorU32(ImGuiCol_FrameBg)
                                  : term.disabled     ? IM_COL32(150, 0, 0, 255)
                                                      : IM_COL32_BLACK;
            const ImU32 ring_col = term.set.contains(mold.rule) ? IM_COL32(0, 255, 0, 255)
                                   : compatible(term.set, mold) ? IM_COL32(0, 100, 0, 255)
                                                                : IM_COL32(255, 0, 0, 255);

            const ImVec2 pos_min = ImGui::GetItemRectMin();
            const ImVec2 pos_max = ImGui::GetItemRectMax();
            assert(pos_min + size == pos_max);
            assert(size.x > 8 && size.y > 8);
            ImGui::GetWindowDrawList()->AddRectFilled(pos_min + ImVec2(4, 4), pos_max - ImVec2(4, 4), cen_col);
            ImGui::GetWindowDrawList()->AddRect(pos_min, pos_max, ring_col);
        };

        // TODO: the layout is still horrible...
        const ImGuiTableFlags flags_outer = ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedFit;
        const ImGuiTableFlags flags_inner =
            ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

        if (ImGui::BeginTable("Checklists", 2, flags_outer)) {
            auto checklist = [&](termT_vec& terms, const char* label) {
                if (ImGui::BeginTable(label, terms.size(), flags_inner)) {
                    ImGui::TableNextRow();
                    for (termT& t : terms) {
                        ImGui::TableNextColumn();
                        imgui_str(t.title);
                        check(t);
                    }
                    ImGui::EndTable();
                }
            };

            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                imgui_str("Ignore&Misc");

                ImGui::TableNextColumn();
                if (ImGui::BeginTable("Checklist##Ignore&Misc", 1, flags_inner)) {
                    ImGui::TableNextRow();
                    // TODO: slightly confusing; light color should represent "take-into-account" instead of
                    // "ignore" Is this solvable by applying specific coloring scheme?
                    ImGui::TableNextColumn();
                    ImGui::BeginGroup();
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1)); // TODO: too tight...
                    for (int l = 0; l < 3; ++l) {
                        check(terms_ignore[l * 3 + 0]);
                        ImGui::SameLine();
                        check(terms_ignore[l * 3 + 1]);
                        ImGui::SameLine();
                        check(terms_ignore[l * 3 + 2]);
                    }
                    ImGui::PopStyleVar();
                    ImGui::EndGroup();

                    for (termT& t : terms_misc) {
                        ImGui::SameLine();
                        ImGui::BeginGroup();
                        imgui_str(t.title);
                        check(t);
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
                imgui_str("Totalistic");

                ImGui::TableNextColumn();
                checklist(terms_totalistic, "Checklist##Totalistic");
            }

            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                imgui_str("q w -    q w\n"
                          "a s d ~ a s d\n"
                          "- x c    x c");

                ImGui::TableNextColumn();
                checklist(terms_hex, "Checklist##Hex");
            }

            ImGui::EndTable();
        }

        if (need_reset) {
            reset_current();
        }
        return current;
    }
};

#ifdef ENABLE_STATIC_CONSTRAINTS
static std::optional<legacy::moldT> static_constraints() {
    enum stateE { Any, O, I, O_Cond, I_Cond }; // TODO: rename; explain
    const int r = 9;
    static stateE board[r][r]{/*Any...*/};
    static bool cond = false;

    auto check = [id = 0, size = square_size()](stateE& state, bool enable) mutable {
        const ImVec2 pos_min = ImGui::GetCursorScreenPos();
        const ImVec2 pos_max = pos_min + size;
        static const ImU32 cols[5]{IM_COL32(100, 100, 100, 255), //
                                   IM_COL32(0, 0, 0, 255),       //
                                   IM_COL32(255, 255, 255, 255), //
                                   IM_COL32(80, 0, 80, 255),     //
                                   IM_COL32(200, 0, 200, 255)};

        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, !enable ? IM_COL32(80, 80, 80, 255) : cols[state]);
        ImGui::GetWindowDrawList()->AddRect(pos_min, pos_max, IM_COL32(200, 200, 200, 255));

        ImGui::PushID(id++);
        ImGui::InvisibleButton("Button", size);
        ImGui::PopID();

        if (enable && ImGui::IsItemHovered()) {
            // TODO: the ctrl is still awkward...
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                state = Any;
            } else {
                if (imgui_scrollup()) {
                    state = cond ? O_Cond : O;
                }
                if (imgui_scrolldown()) {
                    state = cond ? I_Cond : I;
                }
            }
        }
    };

    ImGui::Checkbox("Cond", &cond);
    ImGui::SameLine();
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
        legacy::moldT mold{};
        for (int y = 1; y < r - 1; ++y) {
            for (int x = 1; x < r - 1; ++x) {
                if (board[y][x] != O && board[y][x] != I) {
                    continue;
                }

                legacy::for_each_code([&](legacy::codeT code) {
                    legacy::situT situ = legacy::decode(code);
                    auto imbue = [](bool& b, stateE state) {
                        if (state == O || state == O_Cond) {
                            b = 0;
                        }
                        if (state == I || state == I_Cond) {
                            b = 1;
                        }
                    };

                    imbue(situ.q, board[y - 1][x - 1]);
                    imbue(situ.w, board[y - 1][x]);
                    imbue(situ.e, board[y - 1][x + 1]);

                    imbue(situ.a, board[y][x - 1]);
                    imbue(situ.s, board[y][x]);
                    imbue(situ.d, board[y][x + 1]);

                    imbue(situ.z, board[y + 1][x - 1]);
                    imbue(situ.x, board[y + 1][x]);
                    imbue(situ.c, board[y + 1][x + 1]);

                    mold.rule[legacy::encode(situ)] = board[y][x] == O ? 0 : 1;
                    mold.lock[legacy::encode(situ)] = true;
                });
            }
        }
        return mold;
    }
    return std::nullopt;
}
#endif // ENABLE_STATIC_CONSTRAINTS

// TODO: there must be an [obvious] way to support "dial" mode.
std::optional<legacy::moldT> edit_rule(const legacy::moldT& mold, const code_image& icons) {
    std::optional<legacy::moldT> out;
    auto return_rule = [&out, &mold](const legacy::ruleT& rule) { out.emplace(rule, mold.lock); };
    auto return_lock = [&out, &mold](const legacy::moldT::lockT& lock) { out.emplace(mold.rule, lock); };
    auto return_mold = [&out](const legacy::moldT& mold) { out.emplace(mold); };
    // TODO: these setters look awkward...

    static subset_selector selector;

    // TODO: non-const for set_mask. this is conceptually unsafe as it also allows assignments...
    // TODO: move mask selection logic into selector as well?
    legacy::subsetT& subset = selector.select_subset(mold);
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
                imgui_str(description);
                imgui_str(to_MAP_str(mask));
                ImGui::PopTextWrapPos();
            }
        };

        // TODO: should not be here?
        constexpr auto make_id = [](legacy::codeT::bposE bpos) -> legacy::maskT {
            return {legacy::make_rule([bpos](legacy::codeT c) { return legacy::get(c, bpos); })};
        };
        static const legacy::maskT mask_ids[]{
            make_id(legacy::codeT::bpos_q), make_id(legacy::codeT::bpos_w), make_id(legacy::codeT::bpos_e),
            make_id(legacy::codeT::bpos_a), make_id(legacy::codeT::bpos_s), make_id(legacy::codeT::bpos_d),
            make_id(legacy::codeT::bpos_z), make_id(legacy::codeT::bpos_x), make_id(legacy::codeT::bpos_c)};
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
                mask_custom = {mold.rule};
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
    const bool redispatch_avail = mask_avail && subset.contains(mold.rule);

    // TODO: this is disabling all the operations, including mirror, clear-lock etc...
    // What can be allowed when the selected mask doesn't belong to the set?
    if (!mask_avail) {
        ImGui::BeginDisabled();
    }

    {
        if (!redispatch_avail) {
            ImGui::BeginDisabled();
        }

        // TODO: refine (better names etc)...
        static bool exact_mode = false;

        if (ImGui::Button(std::format("Mode = {}###Mode", exact_mode ? "Exact" : "Dens ").c_str())) {
            exact_mode = !exact_mode;
        }

        // TODO: imgui_innerx...
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        if (exact_mode) {
            // TODO: support different rand mode here ("density" and "exact")
            // TODO: still unstable between partition switches...
            // TODO: the range should be scoped by locks... so, what should rcount be?
            static int rcount = 0.5 * par.k();
            const int freec = legacy::count_free(par, mold.lock); // TODO: still wasteful...

            ImGui::SetNextItemWidth(item_width);
            imgui_int_slider("##Quantity", &rcount, 0, par.k());
            rcount = std::clamp(rcount, 0, freec);
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            if (enter_button("Randomize")) {
                return_rule(legacy::randomize(subset, mold, global_mt19937(), rcount, rcount));
            }
        } else {
            ImGui::SetNextItemWidth(item_width);
            static float density = 0.5;
            ImGui::SliderFloat("##Density", &density, 0, 1, std::format("Around {}", round(density * par.k())).c_str(),
                               ImGuiSliderFlags_NoInput);
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            if (enter_button("Randomize")) {
                return_rule(legacy::randomize_v2(subset, mold, global_mt19937(), density));
            }
        }

        iter_pair(
            "<00..", "dec", "inc", "11..>", //
            [&] { return_rule(legacy::act_int::first(subset, mold)); },
            [&] { return_rule(legacy::act_int::prev(subset, mold)); },
            [&] { return_rule(legacy::act_int::next(subset, mold)); },
            [&] { return_rule(legacy::act_int::last(subset, mold)); }, true, enter_button);
        ImGui::SameLine(), imgui_str("|"), ImGui::SameLine();
        iter_pair(
            "<1.0.", "pprev", "pnext", "0.1.>", //
            [&] { return_rule(legacy::act_perm::first(subset, mold)); },
            [&] { return_rule(legacy::act_perm::prev(subset, mold)); },
            [&] { return_rule(legacy::act_perm::next(subset, mold)); },
            [&] { return_rule(legacy::act_perm::last(subset, mold)); }, true, enter_button);
        ImGui::SameLine(), imgui_str("|"), ImGui::SameLine();
        if (enter_button("Shuffle")) {
            return_rule(legacy::shuffle(subset, mold, global_mt19937()));
        }

        // TODO: (temp) new line begins here...
        // TODO: enhance might be stricter than necessary...
        if (ImGui::Button("Enhance lock")) {
            return_lock(legacy::enhance_lock(subset, mold));
        }
        if (!redispatch_avail) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear locks")) {
            return_lock({});
        }
        ImGui::SameLine();
        if (ImGui::Button("Approximate")) {
            if (legacy::compatible(subset, mold)) {
                return_rule(legacy::approximate(subset, mold));
            } else {
                messenger::add_msg("Incompatible ..."); // TODO refine...
            }
        }
        ImGui::SameLine();
        // TODO: move elsewhere
        if (ImGui::Button("Mir")) {
            return_mold(legacy::mirror(mold));
        }

#ifdef ENABLE_STATIC_CONSTRAINTS
        static bool show_static = false;
        if (!show_static) {
            ImGui::SameLine();
            if (ImGui::Button("Static")) {
                show_static = true;
            }
        }
        if (show_static) {
            if (auto window = imgui_window("Static constraints", &show_static,
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
                if (auto out = static_constraints()) {
                    return_mold(*out);
                }
            }
        }
#endif
    }

    // TODO: support filtering?
    {
        const char labels[2][3]{{'-', chr_0, '\0'}, {'-', chr_1, '\0'}};

        // TODO: find a better name for `ruleT_masked` and the variables...
        const legacy::ruleT_masked masked = mask ^ mold.rule;
        const auto scanlist = legacy::scan(par, masked, mold.lock);
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
                    legacy::moldT::lockT lock = mold.lock;
                    for (auto code : group) {
                        lock[code] = !has_lock; // TODO: not reversible; is this ok?
                    }
                    return_lock(lock);
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
                            if (mold.lock[code]) {
                                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin() - ImVec2(2, 2),
                                                                    ImGui::GetItemRectMax() + ImVec2(2, 2),
                                                                    IM_COL32_WHITE);
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
                    legacy::ruleT r = mold.rule;
                    if (ImGui::GetIO().KeyCtrl) {
                        for (legacy::codeT c : group) {
                            r[c] = mask[c];
                        }
                    } else {
                        for (legacy::codeT c : group) {
                            r[c] = !r[c];
                        }
                    }
                    return_rule(r);
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
