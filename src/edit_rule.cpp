#include "rule_algo.hpp"

#include "common.hpp"

namespace legacy {
    namespace _subsets {
        static const subsetT ignore_q = make_subset({mp_ignore_q});
        static const subsetT ignore_w = make_subset({mp_ignore_w});
        static const subsetT ignore_e = make_subset({mp_ignore_e});
        static const subsetT ignore_a = make_subset({mp_ignore_a});
        static const subsetT ignore_s_z = make_subset({mp_ignore_s}, mask_zero);
        static const subsetT ignore_s_i = make_subset({mp_ignore_s}, mask_identity);
        static const subsetT ignore_d = make_subset({mp_ignore_d});
        static const subsetT ignore_z = make_subset({mp_ignore_z});
        static const subsetT ignore_x = make_subset({mp_ignore_x});
        static const subsetT ignore_c = make_subset({mp_ignore_c});

        static const subsetT ignore_hex = make_subset({mp_hex_ignore});
        static const subsetT ignore_von = make_subset({mp_von_ignore});

        static const subsetT self_complementary = make_subset({mp_dual}, mask_identity);

        static const subsetT native_isotropic = make_subset({mp_refl_wsx, mp_refl_qsc});
        static const subsetT native_refl_wsx = make_subset({mp_refl_wsx});
        static const subsetT native_refl_asd = make_subset({mp_refl_asd});
        static const subsetT native_refl_qsc = make_subset({mp_refl_qsc});
        static const subsetT native_refl_esz = make_subset({mp_refl_esz});
        static const subsetT native_C2 = make_subset({mp_C2});
        static const subsetT native_C4 = make_subset({mp_C4});

        static const subsetT native_tot_exc_s = make_subset({mp_C8, mp_tot_exc_s});
        static const subsetT native_tot_inc_s = make_subset({mp_C8, mp_tot_inc_s});

        static const subsetT hex_isotropic = make_subset({mp_hex_refl_asd, mp_hex_refl_aq});
        static const subsetT hex_refl_asd = make_subset({mp_hex_refl_asd});
        static const subsetT hex_refl_qsc = make_subset({mp_hex_refl_qsc});
        static const subsetT hex_refl_wsx = make_subset({mp_hex_refl_wsx});
        static const subsetT hex_refl_aq = make_subset({mp_hex_refl_aq});
        static const subsetT hex_refl_qw = make_subset({mp_hex_refl_qw});
        static const subsetT hex_refl_wd = make_subset({mp_hex_refl_wd});
        static const subsetT hex_C2 = make_subset({mp_hex_C2});
        static const subsetT hex_C3 = make_subset({mp_hex_C3});
        static const subsetT hex_C6 = make_subset({mp_hex_C6});

        static const subsetT hex_tot_exc_s = make_subset({mp_hex_C6, mp_hex_tot_exc_s});
        static const subsetT hex_tot_inc_s = make_subset({mp_hex_C6, mp_hex_tot_inc_s});

        static const subsetT von_tot_exc_s = make_subset({mp_von_ignore, mp_C4, mp_von_tot_exc_s});
        static const subsetT von_tot_inc_s = make_subset({mp_von_ignore, mp_C4, mp_von_tot_inc_s});
    } // namespace _subsets

#ifdef ENABLE_TESTS
    namespace _tests {
        static const testT test_subsets = [] {
            using namespace _subsets;
            assert(ignore_e.includes(ignore_hex));
            assert(ignore_z.includes(ignore_hex));
            assert(ignore_q.includes(ignore_von));
            assert(ignore_e.includes(ignore_von));
            assert(ignore_z.includes(ignore_von));
            assert(ignore_c.includes(ignore_von));

            assert(native_C2.includes(native_C4));
            for (const subsetT* set :
                 {&native_refl_wsx, &native_refl_asd, &native_refl_qsc, &native_refl_esz, &native_C2, &native_C4}) {
                assert(set->includes(native_isotropic));
            }
            assert(native_isotropic.includes(native_tot_exc_s));
            assert(native_tot_exc_s.includes(native_tot_inc_s));

            assert(hex_C2.includes(hex_C6));
            assert(hex_C3.includes(hex_C6));
            for (const subsetT* set : {&hex_refl_asd, &hex_refl_qsc, &hex_refl_wsx, &hex_refl_aq, &hex_refl_qw,
                                       &hex_refl_wd, &hex_C2, &hex_C3, &hex_C6}) {
                assert(ignore_hex.includes(*set));
                assert(set->includes(hex_isotropic));
            }
            assert(hex_isotropic.includes(hex_tot_exc_s));
            assert(hex_tot_exc_s.includes(hex_tot_inc_s));

            assert(native_isotropic.get_par().k() == 102);
            assert(hex_isotropic.get_par().k() == 26);
            assert((native_isotropic & ignore_von).get_par().k() == 12); // von_isotropic

            assert(native_tot_exc_s.get_par().k() == 9 * 2); // 0...8
            assert(native_tot_inc_s.get_par().k() == 10);    // 0...9
            assert(hex_tot_exc_s.get_par().k() == 7 * 2);    // 0...6
            assert(hex_tot_inc_s.get_par().k() == 8);        // 0...7
            assert(von_tot_exc_s.get_par().k() == 5 * 2);    // 0...4
            assert(von_tot_inc_s.get_par().k() == 6);        // 0...5
        };
    }
#endif // ENABLE_TESTS

} // namespace legacy

class subset_selector {
    legacy::subsetT current;

    struct termT {
        const char* const title;
        const legacy::subsetT* const set;
        const char* const description = "TODO";
        bool selected = false;
        bool disabled = false; // current & set -> empty.
    };

    using termT_list = std::vector<termT>;

    termT_list terms_ignore; // TODO: rename...
    termT_list terms_misc;
    termT_list terms_native;
    termT_list terms_totalistic;
    termT_list terms_hex;
    // TODO: about the plan to support user-defined subsets...

    void for_each_term(const auto& fn) {
        for (termT_list* terms : {&terms_ignore, &terms_misc, &terms_native, &terms_totalistic, &terms_hex}) {
            for (termT& t : *terms) {
                fn(t);
            }
        }
    }

    void update_current() {
        current = legacy::subsetT::universal();

        for_each_term([&](termT& t) {
            assert_implies(t.disabled, !t.selected);
            if (t.selected) {
                current = current & *t.set;
            }
        });

        assert(!current.empty());

        for_each_term([&](termT& t) { //
            t.disabled = !legacy::subsetT::common_rule(*t.set, current);
        });
    }

public:
    subset_selector() : current(legacy::subsetT::universal()) {
        using namespace legacy::_subsets;
        // TODO: add descriptions...

        terms_ignore.emplace_back("q", &ignore_q);
        terms_ignore.emplace_back("w", &ignore_w);
        terms_ignore.emplace_back("e", &ignore_e);
        terms_ignore.emplace_back("a", &ignore_a);
        terms_ignore.emplace_back("s", &ignore_s_z,
                                  "0->1, 1->1 or 0->0, 1->0"); // TODO: whether to put in terms_ignore?
        terms_ignore.emplace_back("d", &ignore_d);
        terms_ignore.emplace_back("z", &ignore_z);
        terms_ignore.emplace_back("x", &ignore_x);
        terms_ignore.emplace_back("c", &ignore_c);

        // TODO: better name...
        terms_misc.emplace_back("S(i)", &ignore_s_i, "0->0, 1->1 or 0->1, 1->0");
        terms_misc.emplace_back("Dual", &self_complementary, "Self-complementary rules.");
        terms_misc.emplace_back("Hex", &ignore_hex);
        terms_misc.emplace_back("Von", &ignore_von);

        terms_native.emplace_back("All", &native_isotropic);
        terms_native.emplace_back("|", &native_refl_wsx);
        terms_native.emplace_back("-", &native_refl_asd);
        terms_native.emplace_back("\\", &native_refl_qsc);
        terms_native.emplace_back("/", &native_refl_esz);
        terms_native.emplace_back("C2", &native_C2);
        terms_native.emplace_back("C4", &native_C4);

        terms_totalistic.emplace_back("Nat", &native_tot_exc_s);
        terms_totalistic.emplace_back("Nat(+s)", &native_tot_inc_s);
        terms_totalistic.emplace_back("Hex", &hex_tot_exc_s);
        terms_totalistic.emplace_back("Hex(+s)", &hex_tot_inc_s);
        terms_totalistic.emplace_back("Von", &von_tot_exc_s);
        terms_totalistic.emplace_back("Von(+s)", &von_tot_inc_s);

        // q w -    q w
        // a s d ~ a s d
        // - x c    x c"
        terms_hex.emplace_back("All", &hex_isotropic);
        terms_hex.emplace_back("a-d", &hex_refl_asd);
        terms_hex.emplace_back("q-c", &hex_refl_qsc);
        terms_hex.emplace_back("w-x", &hex_refl_wsx);
        terms_hex.emplace_back("a|q", &hex_refl_aq);
        terms_hex.emplace_back("q|w", &hex_refl_qw);
        terms_hex.emplace_back("w|d", &hex_refl_wd);
        terms_hex.emplace_back("C2", &hex_C2);
        terms_hex.emplace_back("C3", &hex_C3);
        terms_hex.emplace_back("C6", &hex_C6);

        update_current();
    }

    // TODO: `mold` is the analysis target, rename and explain.
    const legacy::subsetT& select_subset(const legacy::moldT& mold) {
        {
            // https://github.com/ocornut/imgui/issues/6902
            const float extra_w_sameline = ImGui::GetStyle().ItemSpacing.x * 2; // Two SameLine...
            const float extra_w_padding = ImGui::GetStyle().FramePadding.x * 4; // Two Button * two sides...
            const float extra_w = ImGui::CalcTextSize("ClearRecognize").x + extra_w_sameline + extra_w_padding;
            ImGui::SeparatorTextEx(0, "Select subsets", nullptr, extra_w);
            ImGui::SameLine();
            if (ImGui::Button("Clear")) { // TODO: is "Reset" more suitable?
                for_each_term([&](termT& t) { t.disabled = t.selected = false; });
                update_current();
            }
            ImGui::SameLine();
            if (ImGui::Button("Recognize")) {
                for_each_term([&](termT& t) {
                    t.disabled = false; // Will be updated by `update_current`.
                    t.selected = t.set->contains(mold.rule);
                });
                update_current();
            }
        }

        auto check = [&, id = 0, size = square_size()](termT& term) mutable {
            ImGui::PushID(id++);
            if (ImGui::InvisibleButton("Check", size) && !term.disabled) {
                term.selected = !term.selected;
                update_current();
            }
            ImGui::PopID();

            if (ImGui::BeginItemTooltip()) {
                imgui_Str(term.description);
                ImGui::EndTooltip();
            }

            // TODO: better coloring scheme...
            const ImU32 cent_col = term.selected                 ? IM_COL32(65, 150, 255, 255) // Roughly _ButtonHovered
                                   : term.set->includes(current) ? IM_COL32(25, 60, 100, 255)  // Roughly _Button
                                   : term.disabled               ? IM_COL32(120, 30, 0, 255)   // Red
                                                                 : IM_COL32_BLACK_TRANS;
            const ImU32 ring_col = term.set->contains(mold.rule) ? IM_COL32(0, 255, 0, 255)   // Light green
                                   : compatible(*term.set, mold) ? IM_COL32(0, 100, 0, 255)   // Dull green
                                                                 : IM_COL32(200, 45, 0, 255); // Red

            imgui_ItemRectFilled(IM_COL32_BLACK);
            imgui_ItemRectFilled(cent_col, term.disabled ? ImVec2(5, 5) : ImVec2(4, 4));
            imgui_ItemRect(ring_col);
            if (!term.disabled && ImGui::IsItemHovered()) {
                imgui_ItemRectFilled(IM_COL32(255, 255, 255, 45));
            }
        };

        if (ImGui::BeginTable("Checklists", 2, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedFit)) {
            auto put_row = [](const char* l_str, const auto& r_logic) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                imgui_Str(l_str);

                ImGui::TableNextColumn();
                r_logic();
            };

            auto checklist = [&](termT_list& terms) {
                for (bool first = true; termT & t : terms) {
                    if (!std::exchange(first, false)) {
                        ImGui::SameLine();
                    }
                    ImGui::BeginGroup();
                    imgui_Str(t.title);
                    check(t);
                    ImGui::EndGroup();
                }
            };

            put_row("Ignore & Misc", [&] {
                ImGui::BeginGroup();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
                for (int l = 0; l < 3; ++l) {
                    check(terms_ignore[l * 3 + 0]);
                    ImGui::SameLine();
                    check(terms_ignore[l * 3 + 1]);
                    ImGui::SameLine();
                    check(terms_ignore[l * 3 + 2]);
                }
                ImGui::PopStyleVar();
                ImGui::EndGroup();

                ImGui::SameLine();
                checklist(terms_misc);
            });

            put_row("Native", [&] { checklist(terms_native); });
            put_row("Totalistic", [&] { checklist(terms_totalistic); });
            put_row("q w -    q w\n"
                    "a s d ~ a s d\n"
                    "- x c    x c",
                    [&] { checklist(terms_hex); });

            ImGui::EndTable();
        }

        assert(!current.empty());
        return current;
    }
};

std::optional<legacy::moldT> edit_rule(const legacy::moldT& mold, const code_icons& icons) {
    std::optional<legacy::moldT> out = std::nullopt;
    auto return_rule = [&out, &mold](const legacy::ruleT& rule) { out.emplace(rule, mold.lock); };
    auto return_lock = [&out, &mold](const legacy::moldT::lockT& lock) { out.emplace(mold.rule, lock); };
    auto return_mold = [&out](const legacy::moldT& mold) { out.emplace(mold); };
    auto guarded_block = [](const bool enable, const auto& fn) {
        if (!enable) {
            ImGui::BeginDisabled();
        }
        fn();
        if (!enable) {
            ImGui::EndDisabled();
        }
    };

    static subset_selector selector;
    const legacy::subsetT& subset = selector.select_subset(mold);
    assert(!subset.empty());

    ImGui::Separator();

    // Select mask.
    char chr_0 = '0', chr_1 = '1';
    const legacy::maskT& mask = [&] {
        // TODO: finish...
        const char* const about_mask =
            "About mask:\n"
            "A mask is an arbitrary rule used to do XOR masking for other rules...\n"
            "The editions are available only when the mask belongs to the selected subsets .";

        // TODO: support rollbacking custom masks?
        static legacy::maskT mask_custom{{}};

        // TODO: finish descriptions (use cases etc)
        enum maskE { Zero, Identity, Native, Custom };
        static const char* const mask_labels[]{"Zero", "Identity", "Native", "Custom"};
        static const char* const mask_descriptions[]{
            "The all-zero rule.\n"
            "Masked by this you see the actual values, and ...",

            "The rule that maps each situation to the center cell itself, so any pattern will keep unchanged under "
            "this rule. (Click \"<00..\" button to set to it for test.)\n"
            "As the masking rule it shows how \"volatile\" a rule is ...",

            "A specific rule known to belong to the selected subsets, so that it is guaranteed to be able to support "
            "editions...", // (may or may not be the all-zero rule/identity rule depending on the subsets you have
                           // selected...)

            "Custom rule; you can click \"Take current\" button to set this to the current rule.\n"
            "Important tip: ..."};

        // TODO: whether to support mask_mask(bpos_q) etc?
        static maskE mask_tag = Zero;
        const legacy::maskT* const mask_ptrs[]{&legacy::mask_zero, &legacy::mask_identity, &subset.get_mask(),
                                               &mask_custom};

        ImGui::AlignTextToFramePadding();
        imgui_Str("Mask");
        ImGui::SameLine(0, 0);
        imgui_StrTooltip("?", about_mask);

        for (const maskE m : {Zero, Identity, Native, Custom}) {
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());

            if (ImGui::RadioButton(mask_labels[m], mask_tag == m)) {
                mask_tag = m;
            }

            ImGui::SameLine(0, 0);
            imgui_StrTooltip("?", [&] {
                imgui_Str(mask_descriptions[m]);
                imgui_Str(legacy::to_MAP_str(*mask_ptrs[m]));
            });
        }

        ImGui::SameLine();
        if (ImGui::Button("Take current")) {
            mask_custom = {mold.rule};
            mask_tag = Custom;
        }

        switch (mask_tag) {
            case Zero: chr_0 = '0', chr_1 = '1'; break;
            case Identity: chr_0 = '.', chr_1 = 'f'; break;
            default: chr_0 = 'o', chr_1 = 'i'; break;
        }

        return *mask_ptrs[mask_tag];
    }();

    ImGui::Separator();

    // Disable all edit operations if !subset.contains(mask), including those that do not really need
    // the mask to be valid (for example, `trans_mirror`, which does not actually rely on subsets).
    if (!subset.contains(mask)) {
        // TODO: refine message...
        imgui_StrWrapped("This mask does not belong to the selected subsets. Consider trying other masks. "
                         "(The \"Native\" mask is known to belong to the selected subsets and will always work.) "
                         "For more details see the \"Workflow\" part in \"Documents\"");
        return std::nullopt;
    }

    const bool compatible = legacy::compatible(subset, mold);
    const bool contained = subset.contains(mold.rule);
    assert_implies(contained, compatible);

    guarded_block(compatible, [&] {
        sequence::seq(
            "<00..", "dec", "inc", "11..>", //
            [&] { return_rule(legacy::seq_int::min(subset, mask, mold)); },
            [&] { return_rule(legacy::seq_int::dec(subset, mask, mold)); },
            [&] { return_rule(legacy::seq_int::inc(subset, mask, mold)); },
            [&] { return_rule(legacy::seq_int::max(subset, mask, mold)); }, !contained);
    });

    ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
    guarded_block(contained, [&] {
        sequence::seq(
            "<1.0.", "prev", "next", "0.1.>", //
            [&] { return_rule(legacy::seq_perm::first(subset, mask, mold)); },
            [&] { return_rule(legacy::seq_perm::prev(subset, mask, mold)); },
            [&] { return_rule(legacy::seq_perm::next(subset, mask, mold)); },
            [&] { return_rule(legacy::seq_perm::last(subset, mask, mold)); });
    });

    const legacy::partitionT& par = subset.get_par();
    const auto scanlist = legacy::scan(par, mask, mold);

    // TODO: more filtering modes?
    // Will not hide "impure" groups even when there are locks.
    static bool hide_locked = false;
    {
        const int c_group = par.k();
        int c_0 = 0, c_1 = 0, c_x = 0;
        int c_free = 0;
        int c_locked_0 = 0, c_locked_1 = 0, c_locked_x = 0;
        for (const auto& scan : scanlist) {
            if (scan.all_0()) {
                ++c_0;
            } else if (scan.all_1()) {
                ++c_1;
            } else {
                ++c_x; // Number of groups that make `mold.rule` uncontained.
            }

            if (!scan.locked_0 && !scan.locked_1) {
                ++c_free;
            } else if (!scan.locked_0 && scan.locked_1) {
                ++c_locked_1;
            } else if (scan.locked_0 && !scan.locked_1) {
                ++c_locked_0;
            } else {
                ++c_locked_x; // Number of groups that make `mold` incompatible.
            }
        }

        assert(contained == (c_x == 0));
        assert(compatible == (c_locked_x == 0));

        guarded_block(compatible, [&] {
            // dist: The "distance" to the masking rule the randomization want to achieve.
            // (which does not make sense when !compatible)
            static double rate = 0.5;
            int dist = c_locked_1 + round(rate * c_free);

            static bool exact_mode = false;
            if (ImGui::Button(exact_mode ? "Exactly###Mode" : "Around ###Mode")) {
                exact_mode = !exact_mode;
            }

            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            ImGui::SetNextItemWidth(item_width);
            if (imgui_StepSliderInt("##Quantity", &dist, c_locked_1, c_locked_1 + c_free) && c_free != 0) {
                rate = double(dist - c_locked_1) / c_free;
                assert(c_locked_1 + round(rate * c_free) == dist);
            }
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            if (button_with_shortcut("Randomize", ImGuiKey_Enter)) {
                // TODO: bind to global undo/redo?
                if (exact_mode) {
                    return_rule(legacy::randomize_c(subset, mask, mold, global_mt19937(), dist - c_locked_1));
                } else {
                    return_rule(legacy::randomize_p(subset, mask, mold, global_mt19937(), rate));
                }
            }
        });

        guarded_block(true /* Unconditional */, [&] {
            if (ImGui::Button("Mir")) {
                return_mold(legacy::trans_mirror(mold));
            }
        });
        ImGui::SameLine();
        guarded_block(compatible, [&] {
            if (ImGui::Button("Approximate")) {
                return_rule(legacy::approximate(subset, mold));
            }
        });
        ImGui::SameLine();
        // TODO: this is hiding "clear lock" so it's less obvious what to do when !compatible...
        ImGui::Button("Locks");
        if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
            // TODO: say something...
            imgui_Str("...");

            guarded_block(contained, [&] {
                if (ImGui::Button("Enhance")) {
                    return_lock(legacy::enhance_lock(subset, mold));
                }
            });
            ImGui::SameLine();
            guarded_block(true /* Unconditional */, [&] {
                if (ImGui::Button("Clear")) {
                    return_lock({});
                }
            });
            ImGui::SameLine();
            ImGui::Checkbox("Hide locked groups", &hide_locked);
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            imgui_StrTooltip("(!)", "The \"?TODO?\" groups will not be hidden even when there are locks.");
            ImGui::EndPopup();
        }

        // TODO: refine...
        std::string summary = std::format("Groups:{} ({}:{} {}:{} x:{})", c_group, chr_0, c_0, chr_1, c_1, c_x);
        if (c_free != c_group) {
            summary += std::format(" [Locked:{} ({}:{} {}:{} x:{})]", c_group - c_free, chr_0, c_locked_0, chr_1,
                                   c_locked_1, c_locked_x);
        }
        imgui_Str(summary);
    }

    ImGui::Separator();

    if (auto child = imgui_ChildWindow("Details")) {
        const char labels[2][3]{{'-', chr_0, '\0'}, {'-', chr_1, '\0'}};
        const legacy::ruleT_masked masked = mask ^ mold.rule;

        // Precise vertical alignment:
        // https://github.com/ocornut/imgui/issues/2064
        const auto align_text = [](float height) {
            const float off = std::max(0.0f, (height - ImGui::GetTextLineHeight()) / 2);
            ImGui::SetCursorPosY(floor(ImGui::GetCursorPos().y + off));
        };

        const int zoom = 7;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        int n = 0;
        par.for_each_group([&](int j, const legacy::groupT& group) {
            const bool has_lock = scanlist[j].any_locked();
            const bool pure = scanlist[j].all_0() || scanlist[j].all_1();
            if (hide_locked && has_lock && pure) {
                return;
            }
            if (n % 8 != 0) {
                ImGui::SameLine(0, 12);
            }
            if (n != 0 && n % 64 == 0) {
                ImGui::Separator();
            }
            ++n;

            const bool incomptible = scanlist[j].locked_0 != 0 && scanlist[j].locked_1 != 0;
            const legacy::codeT head = group[0];

            // TODO: better color... (will be ugly if using green colors...)
            // _ButtonHovered: ImVec4(0.26f, 0.59f, 0.98f, 1.00f)
            // [0]:Button, [1]:Hover, [2]:Active
            static const ImVec4 button_col_normal[3]{
                {0.26f, 0.59f, 0.98f, 0.70f}, {0.26f, 0.59f, 0.98f, 0.85f}, {0.26f, 0.59f, 0.98f, 1.00f}};
            static const ImVec4 button_col_impure[3]{
                {0.26f, 0.59f, 0.98f, 0.30f}, {0.26f, 0.59f, 0.98f, 0.40f}, {0.26f, 0.59f, 0.98f, 0.50f}};
            static const ImVec4 button_col_incomptible[3]{{0.6f, 0, 0, 1}, {0.8f, 0, 0, 1}, {0.9f, 0, 0, 1}};
            const ImVec4* const button_color = pure           ? button_col_normal
                                               : !incomptible ? button_col_impure
                                                              : button_col_incomptible;

            ImGui::PushStyleColor(ImGuiCol_Button, button_color[0]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_color[1]);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_color[2]);
            if (icons.button(head, zoom)) {
                legacy::ruleT rule = mold.rule;
                for (legacy::codeT code : group) {
                    rule[code] = !rule[code];
                }
                return_rule(rule);
            } else if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                legacy::moldT::lockT lock = mold.lock;
                for (legacy::codeT code : group) {
                    lock[code] = !has_lock;
                }
                return_lock(lock);
            }
            ImGui::PopStyleColor(3);

            const bool show_group = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);

            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            align_text(ImGui::GetItemRectSize().y);
            imgui_Str(labels[masked[head]]);
            if (has_lock) {
                const ImU32 col = scanlist[j].all_locked() ? IM_COL32_WHITE : IM_COL32(128, 128, 128, 255);
                imgui_ItemRect(col, ImVec2(-2, -2));
            }

            if (show_group && ImGui::BeginTooltip()) {
                ImGui::Text("Group size: %d", (int)group.size());
                const int max_to_show = 48;
                for (int x = 0; const legacy::codeT code : group) {
                    if (x++ % 8 != 0) {
                        ImGui::SameLine();
                    }
                    icons.image(code, zoom, ImVec4(1, 1, 1, 1), ImVec4(0.5, 0.5, 0.5, 1));
                    ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                    align_text(ImGui::GetItemRectSize().y);
                    imgui_Str(labels[masked[code]]);
                    if (mold.lock[code]) {
                        imgui_ItemRect(IM_COL32_WHITE, ImVec2(-2, -2));
                    }
                    if (x == max_to_show) {
                        break;
                    }
                }
                if (group.size() > max_to_show) {
                    imgui_Str("...");
                }
                ImGui::EndTooltip();
            }
        });
        ImGui::PopStyleVar(1);
        if (n == 0) {
            imgui_Str("No free groups");
        }
    }

    return out;
}

std::optional<legacy::moldT> static_constraints() {
    enum stateE { Any_background, O, I, O_background, I_background };
    const int r = 9;
    static stateE board[r][r]{/* Any_background... */};
    static stateE state_lbutton = I;
    const stateE state_rbutton = Any_background;
    static const ImU32 cols[5]{IM_COL32(100, 100, 100, 255), //
                               IM_COL32(0, 0, 0, 255),       //
                               IM_COL32(255, 255, 255, 255), //
                               IM_COL32(80, 0, 80, 255),     //
                               IM_COL32(200, 0, 200, 255)};

    const bool ret = ImGui::Button("Done");
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        for (auto& l : board) {
            for (auto& s : l) {
                s = Any_background;
            }
        }
    }

    // Display-only; the value of `state_lbutton` is controlled by mouse-scrolling.
    ImGui::BeginDisabled();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2, 2});
    for (const stateE s : {O, I, O_background, I_background}) {
        if (s != O) {
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        }
        // No need for unique ID here (as the return value is not used).
        ImGui::RadioButton("##Radio", s == state_lbutton);
        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        ImGui::Dummy(square_size());
        imgui_ItemRectFilled(cols[s]);
        imgui_ItemRect(IM_COL32(200, 200, 200, 255));
    }
    ImGui::PopStyleVar();
    ImGui::EndDisabled();

    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    for (int y = 0; y < r; ++y) {
        for (int x = 0; x < r; ++x) {
            if (x != 0) {
                ImGui::SameLine();
            }

            // No need for unique ID here (as IsItemHovered + IsMouseDown doesn't reply on ID).
            ImGui::InvisibleButton("##Invisible", square_size());

            const bool editable = y >= 1 && y < r - 1 && x >= 1 && x < r - 1;
            stateE& state = board[y][x];
            imgui_ItemRectFilled(!editable ? IM_COL32(80, 80, 80, 255) : cols[state]);
            imgui_ItemRect(IM_COL32(200, 200, 200, 255));

            if (editable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    state = state_rbutton;
                } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    state = state_lbutton;
                }
            }
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    if (ImGui::IsItemHovered()) {
        if (imgui_MouseScrollingDown()) {
            state_lbutton = (stateE)std::min((int)I_background, state_lbutton + 1);
        } else if (imgui_MouseScrollingUp()) {
            state_lbutton = (stateE)std::max((int)O, state_lbutton - 1);
        }
    }

    if (ret) {
        legacy::moldT mold{};
        for (int y = 1; y < r - 1; ++y) {
            for (int x = 1; x < r - 1; ++x) {
                if (board[y][x] == O || board[y][x] == I) {
                    // For example:
                    //  O   O_b  I                  001       001
                    // [Any] O   O  will result in [0]00 and [1]00 being set.
                    //  I_b  I  I_b                 111       111
                    legacy::for_each_code([&](legacy::codeT code) {
                        auto imbue = [](bool& b, stateE state) {
                            if (state == O || state == O_background) {
                                b = 0;
                            } else if (state == I || state == I_background) {
                                b = 1;
                            }
                        };

                        legacy::situT situ = legacy::decode(code);

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
        }
        return mold;
    }
    return std::nullopt;
}
