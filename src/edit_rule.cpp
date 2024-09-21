#include <deque>

#include "rule_algo.hpp"

#include "common.hpp"

// TODO: incorporate moldT into the subset system (let the working set be the intersection of subsetT and moldT)?
// How to support an editable moldT that can be incompatible with the current rule?

namespace aniso {
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

        static const subsetT self_complementary = make_subset({mp_reverse}, mask_identity);

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

} // namespace aniso

// `aniso::trans_reverse` cannot be directly declared and called in other TUs, as "the definition of
// an inline function must be reachable in the translation unit where it is accessed".
aniso::ruleT rule_algo::trans_reverse(const aniso::ruleT& rule) { //
    // TODO: temporarily going through a useless moldT.
    return aniso::trans_reverse({rule, {}}).rule;
}

bool rule_algo::is_hexagonal_rule(const aniso::ruleT& rule) { //
    return aniso::_subsets::ignore_hex.contains(rule);
}

#if 0
// (Not using trailing "reason" parameter, as the clang-format result will be very ugly...)
static void guarded_block(const bool enable, const auto& fn /*, const char* reason = nullptr*/) {
    if (!enable) {
        ImGui::BeginDisabled();
        ImGui::BeginGroup();
    }
    fn();
    if (!enable) {
        ImGui::EndGroup();
        ImGui::EndDisabled();
    }
};

// It seems that, the tooltip shown within the group will be hidden by the outer one.
static void guarded_block(const bool enable, const char* reason_disabled, const auto& fn) {
    ::guarded_block(enable, fn);
    if (!enable) {
        imgui_ItemTooltip(reason_disabled);
    }
};
#endif

static int fit_count(int avail, int size, int spacing) { //
    return std::max(1, (avail + spacing) / (size + spacing));
}

// `subsetT` (and `mapperT` pair) are highly customizable. However, for sanity there is no plan to
// support user-defined subsets in the gui part.
class subset_selector {
    aniso::subsetT current;

    struct termT {
        const char* const title;
        const aniso::subsetT* const set;
        const char* const description;

        bool selected = false;

        bool including = false; // set.includes(current).
        bool disabled = false;  // current & set -> empty.
    };

    using termT_list = std::vector<termT>;

    termT_list terms_ignore;
    termT_list terms_misc;
    termT_list terms_native;
    termT_list terms_totalistic;
    termT_list terms_hex;

    void for_each_term(const auto& fn) {
        for (termT_list* terms : {&terms_ignore, &terms_misc, &terms_native, &terms_totalistic, &terms_hex}) {
            for (termT& t : *terms) {
                fn(t);
            }
        }
    }

    void update_current() {
        current = aniso::subsetT::universal();

        for_each_term([&](termT& t) {
            assert_implies(t.disabled, !t.selected);
            if (t.selected) {
                current = current & *t.set;
            }
        });

        assert(!current.empty());

        for_each_term([&](termT& t) { //
            t.including = t.set->includes(current);
            t.disabled = !aniso::has_common(*t.set, current);

            assert_implies(t.selected, t.including);
            assert_implies(t.disabled, !t.selected);
        });
    }

public:
    subset_selector() : current(aniso::subsetT::universal()) {
        using namespace aniso::_subsets;

        // TODO: refine descriptions.
        terms_ignore.emplace_back(
            "q", &ignore_q,
            "Rules whose values are independent of 'q'. That is, for any two cases where only 'q' is different, "
            "the mapped values are the same.\n"
            "(Therefore, these rules will behave as if the neighborhood does not include 'q'. The same applies to "
            "'w/e/a/d/z/x/c'.)\n\n"
            "('q/w/e/a/s/d/z/x/c' are named after the keys in 'qwerty' keyboard.)");
        terms_ignore.emplace_back("w", &ignore_w, "See 'q' for details.");
        terms_ignore.emplace_back("e", &ignore_e, "See 'q' for details.");
        terms_ignore.emplace_back("a", &ignore_a, "See 'q' for details.");
        terms_ignore.emplace_back(
            "s", &ignore_s_z,
            "For any two cases where only 's' is different, the mapped values are the same.\n"
            "(When 'q/w/e/a/d/z/x/c' are the same, there must be: either s:0->1, s:1->1 or s:0->0, s:1->0.)\n\n"
            "(This is defined in the same way as 'q', but it's strange to treat this as \"independent of 's'\".)");
        terms_ignore.emplace_back("d", &ignore_d, "See 'q' for details.");
        terms_ignore.emplace_back("z", &ignore_z, "See 'q' for details.");
        terms_ignore.emplace_back("x", &ignore_x, "See 'q' for details.");
        terms_ignore.emplace_back("c", &ignore_c, "See 'q' for details.");

        terms_misc.emplace_back("s(*)", &ignore_s_i,
                                "Similar to 's' - for any two cases where only 's' is different, the \"flip-ness\" of "
                                "the values are the same (either s:0->0, s:1->1 or s:0->1, s:1->0).");
        terms_misc.emplace_back("Hex", &ignore_hex,
                                "Rules that emulate hexagonal neighborhood, by making the values independent of 'e/z'. "
                                "See the last line for demonstration.\n\n"
                                "For windows displaying hexagonal rules, you can hover on them and press '6' to see "
                                "what \"actually\" happens in the corresponding hexagonal space.");
        terms_misc.emplace_back(
            "Von", &ignore_von,
            "Rules in the von-Neumann neighborhood. (Rules whose values are independent of 'q/e/z/c'.)\n\n"
            "(For symmetric von-Neumann rules you can directly combine this with native-symmetry terms.)");
        terms_misc.emplace_back("S.c.", &self_complementary, "Self-complementary rules.");

        terms_native.emplace_back("All", &native_isotropic,
                                  "Isotropic MAP rules. (Rules that preserve all symmetries.)\n\n"
                                  "(This is equal to the intersection of the following subsets in this line.)",
                                  true /* Selected */);
        terms_native.emplace_back("|", &native_refl_wsx,
                                  "Rules that preserve reflection symmetry, taking '|' as the axis.");
        terms_native.emplace_back("-", &native_refl_asd, "Ditto, the reflection axis is '-'.");
        terms_native.emplace_back("\\", &native_refl_qsc, "Ditto, the reflection axis is '\\'.");
        terms_native.emplace_back("/", &native_refl_esz, "Ditto, the reflection axis is '/'.");
        terms_native.emplace_back("C2", &native_C2, "Rules that preserve C2 symmetry (2-fold rotational symmetry).");
        terms_native.emplace_back("C4", &native_C4,
                                  "C4 symmetry (4-fold rotational symmetry). This is a strict subset of C2.");

        terms_totalistic.emplace_back(
            "Tot", &native_tot_exc_s,
            "Outer-totalistic MAP rules. That is, the values are dependent on 's' and the sum of other cells "
            "('q+w+...').\n\n"
            "(This is also known as life-like rules, and is where the B/S notation applies.)");
        terms_totalistic.emplace_back(
            "Tot(+s)", &native_tot_inc_s,
            "Inner-totalistic MAP rules. That is, the values are only dependent on the sum "
            "of all cells (including 's'). This is a strict subset of outer-totalistic rules.");
        terms_totalistic.emplace_back("Hex", &hex_tot_exc_s, "Outer-totalistic hexagonal rules.");
        terms_totalistic.emplace_back("Hex(+s)", &hex_tot_inc_s, "Inner-totalistic hexagonal rules.");
        terms_totalistic.emplace_back("Von", &von_tot_exc_s, "Outer-totalistic von-Neumann rules.");
        terms_totalistic.emplace_back("Von(+s)", &von_tot_inc_s, "Inner-totalistic von-Neumann rules.");

        // q w -    q w
        // a s d ~ a s d
        // - x c    x c
        terms_hex.emplace_back("All", &hex_isotropic,
                               "Rules that emulate isotropic hexagonal rules. "
                               "You can hover on the displaying windows for such rules and press '6' to "
                               "better view the symmetries in the corresponding hexagonal space.\n\n"
                               "(Remember to unselect native-symmetry terms when working with this line. "
                               "These subsets have no direct relation with native symmetries, and their "
                               "intersection with native-symmetry terms will typically be very small.)");
        terms_hex.emplace_back(
            "a-d", &hex_refl_asd,
            "Rules that emulate reflection symmetry in the hexagonal tiling, taking the axis from 'a' to 'd'.");
        terms_hex.emplace_back("q-c", &hex_refl_qsc, "Ditto, the reflection axis is 'q to c'.");
        terms_hex.emplace_back("w-x", &hex_refl_wsx, "Ditto, the reflection axis is 'w to x'.");
        terms_hex.emplace_back("a|q", &hex_refl_aq, "Ditto, the reflection axis is vertical to 'a to q'.");
        terms_hex.emplace_back("q|w", &hex_refl_qw, "Ditto, the reflection axis is vertical to 'q to w'.");
        terms_hex.emplace_back("w|d", &hex_refl_wd, "Ditto, the reflection axis is vertical to 'w to d'.");
        terms_hex.emplace_back("C2", &hex_C2,
                               "Rules that emulate C2 symmetry in the hexagonal tiling.\n"
                               "(Not to be confused with native C2 rules.)");
        terms_hex.emplace_back("C3", &hex_C3, "C3 symmetry.");
        terms_hex.emplace_back("C6", &hex_C6, "C6 symmetry. This is a strict subset of C2/C3.");

        for_each_term([](const termT& t) { assert(t.title && t.set && t.description); });
        update_current();
    }

private:
    enum centerE { Selected, Including, Disabled, None }; // TODO: add "equals" relation?
    static bool put_term(bool contained, centerE center, const char* title /* Optional */, bool interactive) {
        const ImVec2 size = square_size();
        ImGui::Dummy(size);
        const bool hit = interactive && center != Disabled && ImGui::IsItemClicked(ImGuiMouseButton_Left);

        const ImU32 cent_col = center == Selected    ? IM_COL32(65, 150, 255, 255) // Roughly _ButtonHovered
                               : center == Including ? IM_COL32(25, 60, 100, 255)  // Roughly _Button
                                                     : IM_COL32_BLACK_TRANS;
        const ImU32 ring_col = contained ? IM_COL32(0, 255, 0, 255)  // Light green
                                         : IM_COL32(0, 100, 0, 255); // Dull green
        ImU32 title_col = IM_COL32_WHITE;
        if (center == Disabled) {
            title_col = IM_COL32_GREY(150, 255);
            if (!title) {
                title = "-";
            }
        }

        imgui_ItemRectFilled(IM_COL32_BLACK);
        if (title && (center == None || center == Disabled)) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 sz = ImGui::CalcTextSize(title, title + 1);
            const ImVec2 pos(min.x + floor((size.x - sz.x) / 2),
                             min.y + floor((size.y - sz.y) / 2) - 1 /* -1 for better visual effect */);
            ImGui::GetWindowDrawList()->AddText(pos, title_col, title, title + 1);
        } else {
            imgui_ItemRectFilled(cent_col, ImVec2(4, 4));
        }
        imgui_ItemRect(ring_col);
        if (interactive && center != Disabled && ImGui::IsItemHovered()) {
            imgui_ItemRectFilled(IM_COL32_GREY(255, 45));
        }

        return hit;
    }

public:
    // Mainly about `select`.
    // TODO: some descriptions rely too much on "current rule"...
    static void about() {
        auto explain = [&](bool contained, centerE center, const char* desc) {
            put_term(contained, center, nullptr, false);
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            ImGui::AlignTextToFramePadding(); // `Dummy` does not align automatically.
            imgui_Str(": ");
            ImGui::SameLine(0, 0);
            imgui_Str(desc);
        };

        imgui_Str("The buttons represent subsets of MAP rules. You can select them freely - the program "
                  "will calculate the intersection of selected subsets (with the entire MAP set), "
                  "and help you explore rules in it.\n\n"
                  "The intersection is later called \"working set\". For example, if a rule is said to belong to the "
                  "working set, it should also belong to every selected subset. If nothing is selected, the "
                  "working set will be the entire MAP set.");
        ImGui::Separator();
        imgui_Str("The ring color reflects the relation between the subset and the current rule:");
        explain(true, None, "The rule belongs to this subset.");
        explain(false, None, "The rule does not belong to this subset.");
        // v (Preserved for reference.)
#if 0
        // TODO: this is quite user-unfriendly... (Cannot improve unless the subset system is
        // extended to incorporate value constraints.)
        explain(Compatible, None,
                "The rule does not belong to this subset, but there exist rules in the subset that meet the "
                "constraints (locked values) posed by rule-lock pair.\n"
                "(Notice that even though some subsets may meet this condition individually, their "
                "intersection may still contain no rules that satisfy the constraints.)");
        explain(Incompatible, None,
                "The rule does not belong to this subset, and the constraints cannot be satisfied by any rule "
                "in this subset.");
#endif

        ImGui::Separator();
        imgui_Str("The center color is irrelevant to the ring color, and reflects the selection details:");
        explain(false, None, "Not selected.");
        explain(false, Selected, "Selected.");
        explain(false, Including,
                "Not selected, but the working set already belongs to this subset, so it will behave "
                "as if this is selected too.");
        explain(false, Disabled, "Not selectable. (If selected, the resulting working set will be empty.)");
    }

    const aniso::subsetT& get() const {
        assert(!current.empty());
        return current;
    }

    void clear() {
        for_each_term([&](termT& t) { t.selected = false; });
        update_current();
    }

    void match(const sync_point& target) {
        for_each_term([&](termT& t) {
            t.disabled = false; // Will be updated by `update_current`.
            t.selected = t.set->contains(target.rule);
        });
        update_current();
    }

    void show_working(const sync_point& target) {
        ImGui::AlignTextToFramePadding();
        imgui_Str("Working set ~");
        ImGui::SameLine();
        put_term(current.contains(target.rule), None, nullptr, false);
        guide_mode::item_tooltip("This will be light green if the current rule belongs to every selected "
                                 "subset. See '(...)' for details.");
    }

    void select(const sync_point& target) {
        if (ImGui::BeginTable("Checklists", 2, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedFit)) {
            auto check = [&](termT& term, bool show_title = false) {
                if (put_term(term.set->contains(target.rule),
                             term.selected    ? Selected
                             : term.including ? Including
                             : term.disabled  ? Disabled
                                              : None,
                             show_title ? term.title : nullptr, true)) {
                    assert(!term.disabled);
                    term.selected = !term.selected;
                    update_current();
                }

                imgui_ItemTooltip(term.description);
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

            auto put_row = [](const char* l_str, const auto& r_logic) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                imgui_Str(l_str);

                ImGui::TableNextColumn();
                r_logic();
            };

            put_row("Neighborhood\n/ Misc", [&] {
                ImGui::BeginGroup();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
                for (int l = 0; l < 3; ++l) {
                    check(terms_ignore[l * 3 + 0], true);
                    ImGui::SameLine();
                    check(terms_ignore[l * 3 + 1], true);
                    ImGui::SameLine();
                    check(terms_ignore[l * 3 + 2], true);
                }
                ImGui::PopStyleVar();
                ImGui::EndGroup();

                ImGui::SameLine();
                checklist(terms_misc);
            });

            put_row("Native\nsymmetry", [&] { checklist(terms_native); });
            put_row("Totalistic", [&] { checklist(terms_totalistic); });
            put_row("q w -    q w\n"
                    "a s d ~ a s d\n"
                    "- x c    x c",
                    [&] { checklist(terms_hex); });

            ImGui::EndTable();
        }
    }
};

class mask_selector {
    enum maskE { Zero, Identity, Fallback, Custom };

    struct termT {
        const char* label;
        const char* desc;
        char chr_0, chr_1;
    };
    static constexpr termT mask_terms[]{
        {"Zero",
         "The all-zero rule. (Every cell will become 0, regardless of its neighbors.)\n\n"
         "The distance to this rule is equal to the number of groups that returns 1, and the masked "
         "values can directly represent the actual values of the rule:\n"
         "Different:'1' ~ the cell will become 1 in this case.\n"
         "Same:'0' ~ the cell will become 0 in this case.",
         '0', '1'},

        {"Identity",
         "The rule that preserves the values in all cases. (Every cell will stay unchanged, "
         "regardless of its neighbors.)\n\n"
         "Masked value:\n"
         "Different:'f' ~ the cell will \"flip\" in this case.\n"
         "Same:'.' ~ the cell will stay unchanged in this case.",
         '.', 'f'},

        {"Fallback",
         "A rule calculated by the program that belongs to the working set. Depending on what subsets "
         "are selected, it may be the same as zero-rule, or identity-rule, or just an ordinary rule in the set.\n"
         "This is provided in case there are no other rules known to belong to the working set. It will not "
         "change unless the working set is updated.\n\n"
         "Masked value: different:'i', same:'o'.",
         'o', 'i'},

        {"Custom",
         "You can click '<< Cur' to set this to the current rule.\n\n"
         "This is initially the Game of Life rule, and will not change until you click '<< Cur' next time. "
         "So for example, if you want to get some random rules with a small distance to the current rule, you "
         "can do '<< Cur' and generate random rules with a low distance in the 'Random' window.\n\n"
         "Masked value: different:'i', same:'o'.",
         'o', 'i'}};

    aniso::maskT mask_custom{aniso::game_of_life()};
    maskE mask_tag = Zero;

public:
    // TODO: improve...
    static void about() {
        imgui_Str(
            "The working set divides all cases into different groups. For any two rules in the working set, "
            "they must have either all-the-same or all-the-different values in each group.\n\n"
            "A mask is an arbitrary rule in the working set to compare with other rules (XOR masking). The values "
            "of the current rule are viewed through the mask in the random-access section. The 'Zero' and 'Identity' "
            "rules are special in the sense that the values masked by them have natural interpretations.\n\n"
            "When talking about the \"distance\" between two rules (in the working set), it means the number of "
            "groups where they have different values.\n\n"
            "(For more details see the 'Subset, mask ...' section in 'Documents'.)");
    }

    // `subset` must not be a temporary.
    const aniso::maskT& select(const aniso::subsetT& subset, const sync_point& sync) {
        const aniso::maskT* const mask_ptrs[]{&aniso::mask_zero, &aniso::mask_identity, &subset.get_mask(),
                                              &mask_custom};

        if (!subset.contains(*mask_ptrs[mask_tag])) {
            assert(mask_tag != Fallback);
            mask_tag = Fallback;
        }

        ImGui::AlignTextToFramePadding();
        imgui_Str("Mask ~");
        for (const maskE m : {Zero, Identity, Fallback, Custom}) {
            const bool m_avail = subset.contains(*mask_ptrs[m]);

            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            ImGui::BeginDisabled(!m_avail);
            imgui_RadioButton(mask_terms[m].label, &mask_tag, m);
            ImGui::EndDisabled();

            imgui_ItemTooltip([&] {
                if (!m_avail) {
                    imgui_Str("This rule does not belong to the working set.");
                    ImGui::Separator();
                }

                imgui_Str(mask_terms[m].desc);
                previewer::preview(-1, previewer::configT::_220_160, *mask_ptrs[m], false);
            });

            if (m == Custom) {
                ImGui::SameLine();
                const int radio_id = ImGui::GetItemID();
                // TODO: share with `edit_rule`?
                const bool contained = subset.contains(sync.rule);
                ImGui::BeginDisabled(!contained);
                if (ImGui::Button("<< Cur")) {
                    mask_custom = {sync.rule};
                    mask_tag = Custom;

                    // It will be strange to call a shortcut function here.
                    // shortcuts::highlight(radio_id);
                    ImGui::NavHighlightActivated(radio_id);
                }
                ImGui::EndDisabled();
                guide_mode::item_tooltip("Set the custom masking rule to the current rule.");
                if (!contained) {
                    imgui_ItemTooltip(guide_mode::enabled() ? "\n(The current rule does not belong to the working set.)"
                                                            : "The current rule does not belong to the working set.");
                }
            }
        }

        assert(subset.contains(*mask_ptrs[mask_tag]));
        return *mask_ptrs[mask_tag];
    }

    std::array<char, 2> masked_char() const { //
        return {mask_terms[mask_tag].chr_0, mask_terms[mask_tag].chr_1};
    }
};

struct page_adapter {
    int page_size = 6, perline = 3;

    // `page_resized` should be able to access *this someway.
    void display(const previewer::configT& config, sync_point& out, ImVec2& min_req_size,
                 std::function<void()> page_resized, std::function<const aniso::ruleT*(int)> access) {
        const ImVec2 avail_size = ImGui::GetContentRegionAvail();

        // Background
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + avail_size,
                                                  IM_COL32_GREY(24, 255));

        // Resize the page so all previewers can fit into the area. This should be done before the loop.
        if (!ImGui::IsWindowAppearing()) {
            const ImVec2 button_size = ImGui::CalcTextSize(">> Cur") + ImGui::GetStyle().FramePadding * 2;
            const ImVec2 previewer_size = config.size_imvec();
            const float item_spacing_y = ImGui::GetStyle().ItemSpacing.y;
            const float group_size_x = std::max(button_size.x, previewer_size.x);
            const float group_size_y = item_spacing_y /*separator*/ + button_size.y +
                                       item_spacing_y /*between the button and previewer*/ + previewer_size.y;

            const int xc = fit_count(avail_size.x, group_size_x, 16 /*SameLine(0, 16)*/);
            const int yc = fit_count(avail_size.y, group_size_y, item_spacing_y);
            perline = xc;
            if (page_size != xc * yc) {
                page_size = xc * yc;
                page_resized();
            }
        }

        for (int j = 0; j < page_size; ++j) {
            if (j % perline != 0) {
                ImGui::SameLine(0, 16); // (The same value as in `edit_rule`.)
            } else {
                ImGui::Separator();
            }

            ImGui::BeginGroup();
            if (const aniso::ruleT* rule = access(j); rule != nullptr) {
                ImGui::PushID(j);
                if (ImGui::Button(">> Cur")) {
                    out.set(*rule);
                }
                ImGui::PopID();
                previewer::preview(j, config, *rule);
            } else {
                ImGui::BeginDisabled();
                ImGui::Button(">> Cur");
                ImGui::EndDisabled();
                imgui_ItemTooltip("Empty.");
                previewer::dummy(config, IM_COL32_BLACK);
            }
            ImGui::EndGroup();

            if (j == 0) {
                // The enclosing window should be able to fully contain at least one previewer.
                // The code looks fairly fragile but works...
                min_req_size = ImGui::GetCurrentWindowRead()->DC.CursorMaxPos + ImGui::GetStyle().WindowPadding -
                               ImGui::GetWindowPos();
            }
        }
    }
};

static void traverse_window(bool& show_trav, sync_point& sync, const aniso::subsetT& subset, const aniso::maskT& mask) {
    assert(show_trav);
    static ImVec2 size_constraint_min{};
    ImGui::SetNextWindowSizeConstraints(size_constraint_min, ImVec2(FLT_MAX, FLT_MAX));
    // !!TODO: better title...
    if (auto window = imgui_Window("Traverse the working set", &show_trav,
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar)) {
        static std::deque<aniso::ruleT> page;
        static page_adapter adapter;
        static previewer::configT config{previewer::configT::_220_160};

        auto fill_next = [&](int size) {
            assert(!page.empty());
            for (int i = 0; i < size; ++i) {
                const aniso::ruleT rule = aniso::seq_mixed::next(subset, mask, page.back());
                if (rule == page.back()) {
                    break;
                }
                page.push_back(rule);
            }
        };

        auto fill_prev = [&](int size) {
            assert(!page.empty());
            for (int i = 0; i < size; ++i) {
                const aniso::ruleT rule = aniso::seq_mixed::prev(subset, mask, page.front());
                if (rule == page.front()) {
                    break;
                }
                page.push_front(rule);
            }
        };

        // (Moved from outer scope to avoid missing the change when this window is closed.)
        const bool working_set_or_mask_is_changed = [&] {
            static aniso::subsetT cmp_set{};
            static aniso::maskT cmp_mask{};
            bool changed = false;
            if (cmp_set != subset) {
                changed = true;
                cmp_set = subset;
            }
            if (cmp_mask != mask) {
                changed = true;
                cmp_mask = mask;
            }
            return changed;
        }();

        // !!TODO: it's not a good idea to clear without confirmation...
        if (working_set_or_mask_is_changed && !page.empty()) {
            // !!TODO: whether to show message here?
            page.clear();
        }

        // !!TODO: not clear enough...
        ImGui::AlignTextToFramePadding();
        imgui_StrTooltip("(...)",
                         "Iterate through the entire working set, by firstly iterating through all rules with "
                         "distance = 1 to the masking rule, then 2, 3, ..., until max distance.\n\n"
                         "For example, suppose the current rule belongs to the working set. To iterate through all "
                         "rules with distance = 1 to the current rule, you can:\n"
                         "1. '<< Cur' to set it as the custom mask.\n"
                         "2. 'Next' to iterate. The left/right arrow key will be bound to 'Prev/Next' after you "
                         "click the button.");
        ImGui::SameLine();

        // TODO: improve...
        static input_int input_dist{};
        imgui_Str("Seek to dist ~ ");
        ImGui::SameLine(0, 0);
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Max:0000").x + ImGui::GetStyle().FramePadding.x * 2);
        if (const auto dist = input_dist.input("##Seek", std::format("Max:{}", subset.get_par().k()).c_str())) {
            page.clear();
            page.push_back(aniso::seq_mixed::seek_n(subset, mask, *dist));
            fill_next(adapter.page_size - 1);
            // TODO: whether to try to fill the page in this (and other similar) case?
            if (page.size() < adapter.page_size) {
                fill_prev(adapter.page_size - page.size());
            }
        }
        ImGui::SameLine();
        imgui_StrTooltip("(?)", "The max distance is actually the number of groups in the working set.");

        // TODO: share with `edit_rule`?
        const bool contained = subset.contains(sync.rule);
        ImGui::BeginDisabled(!contained);
        if (ImGui::Button("Locate")) {
            assert(subset.contains(sync.rule));
            page.clear();
            page.push_back(sync.rule);
            fill_next(adapter.page_size - 1);
        }
        ImGui::EndDisabled();
        guide_mode::item_tooltip("Seek to the position where the current rule belongs.");
        if (!contained) {
            imgui_ItemTooltip(guide_mode::enabled() ? "\n(The current rule does not belong to the working set.)"
                                                    : "The current rule does not belong to the working set.");
        }
        ImGui::SameLine();
        sequence::seq(
            "<00..", "Prev", "Next", "11..>",
            [&] {
                page.clear();
                page.push_back(aniso::seq_mixed::first(subset, mask));
                fill_next(adapter.page_size - 1);
            },
            [&] {
                fill_prev(adapter.page_size);
                while (page.size() > adapter.page_size) {
                    page.pop_back();
                }
            },
            [&] {
                fill_next(adapter.page_size);
                while (page.size() > adapter.page_size) {
                    page.pop_front();
                }
            },
            [&] {
                page.clear();
                page.push_back(aniso::seq_mixed::last(subset, mask));
                fill_prev(adapter.page_size - 1);
            },
            page.empty() ? "Click 'Locate', '<00..' or '11..>' to get somewhere in the sequence." : nullptr);
        // TODO: add guide-tooltip.

        ImGui::SameLine();
        if (page.empty()) {
            imgui_Str("Dist:N/A");
        } else {
            const int min_dist = aniso::distance(subset, mask, page.front());
            const int max_dist = aniso::distance(subset, mask, page.back());
            assert(min_dist <= max_dist);
            if (min_dist == max_dist) {
                ImGui::Text("Dist:%d", min_dist);
            } else {
                ImGui::Text("Dist:%d~%d", min_dist, max_dist);
            }
        }
        if (imgui_ItemClickableDouble()) {
            page.clear();
            // messenger::set_msg("Cleared.");
        }
        imgui_ItemTooltip_StrID = "Clear";
        guide_mode::item_tooltip("Double right-click to clear.");

        ImGui::SameLine(0, 16);
        config.set("Preview settings");

        adapter.display(
            config, sync, size_constraint_min,
            [&] {
                if (page.size() > adapter.page_size) {
                    while (page.size() > adapter.page_size) {
                        page.pop_back();
                    }
                } else if (!page.empty() && page.size() < adapter.page_size) {
                    fill_next(adapter.page_size - page.size());
                    // (This may happen when the page reaches the end of the sequence.)
                    if (page.size() < adapter.page_size) [[unlikely]] {
                        fill_prev(adapter.page_size - page.size());
                    }
                }
            },
            [](int j) { return j < page.size() ? &page[j] : nullptr; });
    }
}

static void random_rule_window(bool& show_rand, sync_point& sync, const aniso::subsetT& subset,
                               const aniso::maskT& mask) {
    assert(show_rand);
    static ImVec2 size_constraint_min{};
    ImGui::SetNextWindowSizeConstraints(size_constraint_min, ImVec2(FLT_MAX, FLT_MAX));
    if (auto window =
            imgui_Window("Random rules", &show_rand, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar)) {
        const int c_group = subset.get_par().k();
        const int c_free = c_group; // TODO: temporarily preserved.
        const bool has_lock = false;

        static double rate = 0.29;
        int free_dist = round(rate * c_free); // Intended distance.

        static bool exact_mode = false;
        if (ImGui::Button(exact_mode ? "Exactly###Mode" : "Around ###Mode")) {
            exact_mode = !exact_mode;
        }
        guide_mode::item_tooltip("Click to switch between 'Around' / 'Exactly'.");
        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        ImGui::SetNextItemWidth(item_width);
        if (imgui_StepSliderInt("Dist", &free_dist, 0, c_free, has_lock ? "(Free) %d" : "%d") && c_free != 0) {
            rate = double(free_dist) / c_free;
            assert(round(rate * c_free) == free_dist);
        }
        ImGui::SameLine();
        imgui_StrTooltip("(?)",
                         "Generate random rules (in the working set) with intended distance around/exactly to the "
                         "masking rule, as specified by the button and slider.\n\n"
                         "For example, if you are using the 'Zero' mask and distance = 'Around' 30, (when at the "
                         "last page) '>>>' will generate pages of rules with around 30 groups having '1'.\n\n"
                         "(Also, suppose the current rule belongs to the working set, you can set it to "
                         "the custom mask, and generate in a low distance to get random rules that are \"close\" "
                         "to it.)");
        // TODO: where to record this?
        // The window can be resized to fit the page size by double-clicking the resize border.

        static std::vector<aniso::ruleT> rules{};
        static page_adapter adapter{};
        static int page_no = 0;
        static previewer::configT config{previewer::configT::_220_160};

        auto calc_page = [&]() -> int { return (rules.size() + adapter.page_size - 1) / adapter.page_size; };
        auto set_page = [&](int p, bool make_page = false) {
            if (p < 0) {
                return;
            } else if (p < calc_page()) {
                page_no = p;
            } else if (make_page) {
                const int count =
                    (rules.size() / adapter.page_size) * adapter.page_size + adapter.page_size - rules.size();
                for (int i = 0; i < count; ++i) {
                    rules.push_back(exact_mode ? aniso::randomize_c(subset, mask, global_mt19937(), free_dist)
                                               : aniso::randomize_p(subset, mask, global_mt19937(), rate));
                }
                assert((rules.size() % adapter.page_size) == 0);
                page_no = (rules.size() / adapter.page_size) - 1;
            }
        };
        auto set_last_page = [&] { set_page(rules.empty() ? 0 : calc_page() - 1); };

        sequence::seq(
            "<|", "<<", ">>>", "|>", //
            [&] { set_page(0); }, [&] { set_page(page_no - 1); }, [&] { set_page(page_no + 1, true); }, set_last_page);
        // TODO: add guide-tooltip
        ImGui::SameLine();
        if (!rules.empty()) {
            // TODO: will this be confusing when the page is resized?
            ImGui::Text("Pages:%d At:%d", calc_page(), page_no + 1);
        } else {
            ImGui::Text("Pages:%d At:N/A", calc_page());
        }
        if (imgui_ItemClickableDouble()) {
            rules = std::vector<aniso::ruleT>{};
            page_no = 0;
            // messenger::set_msg("Cleared.");
        }
        imgui_ItemTooltip_StrID = "Clear";
        guide_mode::item_tooltip("Double right-click to clear all pages.");

        ImGui::SameLine(0, 16);
        config.set("Preview settings");

        // TODO: reconsider page-resized logic (seeking to the last page may still be confusing).
        adapter.display(config, sync, size_constraint_min, set_last_page, [&](int j) {
            const int r = page_no * adapter.page_size + j;
            assert(r >= 0);
            return r < rules.size() ? &rules[r] : nullptr;
        });
    }
}

// TODO: rename some variables for better clarity. (e.g. `subset` -> `working_set`)
void edit_rule(sync_point& sync) {
    // Select subsets.
    static subset_selector select_set;
    {
        ImGui::AlignTextToFramePadding();
        imgui_StrTooltip("(...)", subset_selector::about);
        ImGui::SameLine();
        select_set.show_working(sync);

        static bool collapse = false;
        ImGui::SameLine();
        ImGui::Checkbox("Collapse", &collapse);

        auto select = [&] {
            if (ImGui::Button("Clear##Sets")) {
                select_set.clear();
            }
            guide_mode::item_tooltip("Unselect the subsets. The resulting working set will be the entire MAP set.");
            ImGui::SameLine();
            if (ImGui::Button("Match")) {
                select_set.match(sync);
            }
            guide_mode::item_tooltip("Select every subset that contains the current rule.");

            ImGui::Separator();
            select_set.select(sync);
        };

        if (!collapse) {
            ImGui::SameLine();
            select();
        } else {
            ImGui::SameLine();
            ImGui::Button("Select");
            if (begin_menu_for_item()) {
                select();
                ImGui::EndPopup();
            }
        }
    }
    const aniso::subsetT& subset = select_set.get();
    assert(!subset.empty());
    const bool contained = subset.contains(sync.rule);

    ImGui::Separator();

    // Select mask.
    static mask_selector select_mask;
    ImGui::AlignTextToFramePadding();
    imgui_StrTooltip("(...)", mask_selector::about);
    ImGui::SameLine();
    const aniso::maskT& mask = select_mask.select(subset, sync);
    const auto [chr_0, chr_1] = select_mask.masked_char();

    ImGui::Separator();

    const aniso::partitionT& par = subset.get_par();
    const auto scanlist = aniso::scanT::scanlist(par, mask, sync.rule);

    static bool show_trav = false;
    static bool show_rand = false;
    static bool preview_mode = false;
    static previewer::configT config{previewer::configT::_220_160};
    {
        const bool clicked = ImGui::Checkbox("Traverse", &show_trav);
        guide_mode::item_tooltip("Iterate through all rules in the working set.");
        if (clicked && show_trav) {
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImGui::GetItemRectMax() + ImVec2(30, -120), ImGuiCond_FirstUseEver);
        }
        if (show_trav) {
            traverse_window(show_trav, sync, subset, mask);
        }
    }
    ImGui::SameLine();
    {
        const bool clicked = ImGui::Checkbox("Random", &show_rand);
        guide_mode::item_tooltip("Get random rules in the working set.");
        if (clicked && show_rand) {
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImGui::GetItemRectMax() + ImVec2(30, -120), ImGuiCond_FirstUseEver);
        }
        if (show_rand) {
            random_rule_window(show_rand, sync, subset, mask);
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Preview", &preview_mode);
    guide_mode::item_tooltip("Try this!");
    if (preview_mode) {
        ImGui::SameLine();
        config.set("Settings");
    }

    {
        const int c_group = par.k();

        if (contained) {
            const int dist = aniso::distance(subset, mask, sync.rule);
            std::string str = std::format("Groups:{} ({}:{} {}:{})", c_group, chr_1, dist, chr_0, c_group - dist);

            // v (Preserved for reference.)
#if 0
            if (c_free != c_group) {
                const int c_free_1 = c_1 - c_locked_1, c_free_0 = c_0 - c_locked_0;
                str += std::format(" Free:{} ({}:{} {}:{}) Locked:{} ({}:{} {}:{})", c_free, chr_1, c_free_1, chr_0,
                                   c_free_0, c_locked_1 + c_locked_0, chr_1, c_locked_1, chr_0, c_locked_0);
            }
#endif
            imgui_Str(str);
        } else {
            ImGui::Text("Groups:%d !contained", c_group);
            ImGui::SameLine();
            imgui_StrTooltip("(?)", [&] {
                imgui_Str("The current rule does not belong to the working set.\n\n"
                          "Check the dull-blue groups for details - no matter which mask is selected, for any rule in "
                          "the working set, the masked values should be all the same in any group.\n\n"
                          "You can get rules in the working set from the 'Traverse' or 'Random' window. Or optionally, "
                          "you can double right-click this '(?)' to get the following rule in the working set.");
                ImGui::Separator();
                imgui_Str("Preview:");
                ImGui::SameLine();
                previewer::preview(-1, previewer::configT::_220_160, aniso::approximate(subset, sync.rule), false);
            });
            // TODO: maybe it's better just to remove this feature...
            if (imgui_ItemClickableDouble()) {
                sync.set(aniso::approximate(subset, sync.rule));
            }
            guide_mode::highlight();
        }
    }

    // TODO: by default, the program should not allow random-access edit for rules not included in
    // the working set. (This should still be supported, but should be an opt-in feature...)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32_GREY(24, 255));
    if (auto child = imgui_ChildWindow("Groups")) {
        // TODO: document the behavior.
        set_scroll_by_up_down(preview_mode ? floor(config.height() * 0.5) : ImGui::GetFrameHeight());

        const char labels_normal[2][3]{{'-', chr_0, '\0'}, {'-', chr_1, '\0'}};
        const char labels_preview[2][9]{{'-', chr_0, ' ', '-', '>', ' ', chr_1, ':', '\0'},
                                        {'-', chr_1, ' ', '-', '>', ' ', chr_0, ':', '\0'}};
        const aniso::ruleT_masked masked = mask ^ sync.rule;

        // Precise vertical alignment:
        // https://github.com/ocornut/imgui/issues/2064
        const auto align_text = [](float height) {
            const float off = std::max(0.0f, (height - ImGui::GetTextLineHeight()) / 2);
            ImGui::SetCursorPosY(floor(ImGui::GetCursorPos().y + off));
        };

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        const int zoom = 7;
        const int spacing = preview_mode ? 16 : 12;
        const int perline = [&] {
            // button-size + inner-spacing + label-size
            int group_size = (2 * 2 + 3 * zoom) + imgui_ItemInnerSpacingX() +
                             ImGui::CalcTextSize(preview_mode ? labels_preview[0] : labels_normal[0]).x;
            if (preview_mode) {
                group_size = std::max(group_size, config.width());
            }
            return fit_count(ImGui::GetContentRegionAvail().x, group_size, spacing);
        }();

        int n = 0;
        assert(par.k() <= 512);
        std::array<bool, 512> shown{};

        // v (Preserved for reference.)
#if 0
        for (const auto select : {+[](const aniso::scanT& c) { return !c.all_0() && !c.all_1(); }, //
                                  +[](const aniso::scanT& c) { return !c.any_locked(); },          //
                                  +[](const aniso::scanT& c) { return c.any_locked(); }}) {
#else
        for (const auto select : {+[](const aniso::scanT& c) { return !c.pure(); }, //
                                  +[](const aniso::scanT& c) { return c.pure(); }}) {
#endif
            par.for_each_group([&](const int j, const aniso::groupT& group) {
                if (shown[j] || !select(scanlist[j])) {
                    return;
                }
                shown[j] = true;

                const bool has_lock = false; // TODO: temporarily preserved.
                const bool pure = scanlist[j].pure();
                if (n % perline != 0) {
                    ImGui::SameLine(0, spacing);
                } else {
                    ImGui::Separator();
                }
                ++n;

                const aniso::codeT head = group[0];
                const auto get_adjacent_rule = [&] {
                    aniso::ruleT rule = sync.rule;
                    for (aniso::codeT code : group) {
                        rule[code] = !rule[code];
                    }
                    return rule;
                };
                const auto show_group = [&] {
                    imgui_Str("Left-click to flip the values.");
                    ImGui::Separator();
                    ImGui::Text("Group size: %d", (int)group.size());
                    const int max_to_show = 48;
                    for (int x = 0; const aniso::codeT code : group) {
                        if (x++ % 8 != 0) {
                            ImGui::SameLine();
                        }
                        code_image(code, zoom, ImVec4(1, 1, 1, 1), ImVec4(0.5, 0.5, 0.5, 1));
                        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                        align_text(ImGui::GetItemRectSize().y);
                        imgui_Str(labels_normal[masked[code]]);
                        if (x == max_to_show) {
                            break;
                        }
                    }
                    if (group.size() > max_to_show) {
                        imgui_Str("...");
                    }
                };

                // TODO: better color... (will be ugly if using green colors...)
                // _ButtonHovered: ImVec4(0.26f, 0.59f, 0.98f, 1.00f)
                // [0]:Button, [1]:Hover, [2]:Active
                static const ImVec4 button_col_normal[3]{
                    {0.26f, 0.59f, 0.98f, 0.70f}, {0.26f, 0.59f, 0.98f, 0.85f}, {0.26f, 0.59f, 0.98f, 1.00f}};
                static const ImVec4 button_col_impure[3]{
                    {0.26f, 0.59f, 0.98f, 0.30f}, {0.26f, 0.59f, 0.98f, 0.40f}, {0.26f, 0.59f, 0.98f, 0.50f}};
                const ImVec4* const button_color = pure ? button_col_normal : button_col_impure;

                if (preview_mode) {
                    ImGui::BeginGroup();
                }
                ImGui::PushStyleColor(ImGuiCol_Button, button_color[0]);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_color[1]);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_color[2]);
                if (code_button(head, zoom)) {
                    sync.set(get_adjacent_rule());
                }
                ImGui::PopStyleColor(3);
                imgui_ItemTooltip(show_group);

                ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                align_text(ImGui::GetItemRectSize().y);
                imgui_Str(preview_mode ? labels_preview[masked[head]] : labels_normal[masked[head]]);
                if (has_lock) {
                    imgui_ItemRect(IM_COL32_WHITE, ImVec2(-2, -2));
                }

                if (preview_mode) {
                    previewer::preview(j, config, get_adjacent_rule /*()*/);
                    ImGui::EndGroup();
                }
            });
        }
        assert(n == par.k());

        ImGui::PopStyleVar(1);
    }
    ImGui::PopStyleColor();
}

// TODO: temporarily preserved.
// TODO: move to "apply_rule.cpp"? (as this is a special type of capture...)
static void static_constraints(aniso::moldT& out) {
    enum stateE { Any_background, O, I, O_background, I_background };

    // (Follows `ImGui::Dummy` or `ImGui::InvisibleButton`.)
    static const auto put_col = [](stateE state, bool disabled = false) {
        static const ImU32 cols[5]{IM_COL32_GREY(80, 255),   //
                                   IM_COL32_BLACK,           //
                                   IM_COL32_WHITE,           //
                                   IM_COL32(80, 0, 80, 255), //
                                   IM_COL32(200, 0, 200, 255)};
        assert_implies(disabled, state == Any_background);
        imgui_ItemRectFilled(disabled ? IM_COL32_GREY(60, 255) : cols[state]);
        imgui_ItemRect(IM_COL32_GREY(160, 255));
    };

    const int r = 10; // TODO: use separate values for w and h?
    static stateE board[r][r]{/* Any_background... */};
    static stateE state_lbutton = I;
    const stateE state_rbutton = Any_background;
    static const auto description = [] {
        auto term = [](stateE s, const char* desc) {
            ImGui::Dummy(square_size());
            put_col(s);
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            ImGui::AlignTextToFramePadding(); // `Dummy` does not align automatically.
            imgui_Str(desc);
        };

        imgui_Str("Operations:\n"
                  "Left-click a cell to set the value.\n"
                  "Right-click to set back to any-background.\n"
                  "Scroll in the board to change the value for left-click.\n");
        term(O, ": Supposed to remain 0.");
        term(I, ": Supposed to remain 1.");
        term(O_background, ": Background 0.");
        term(I_background, ": Background 1.");
        term(Any_background, ": Any background.");
        imgui_Str("By 'Adopt', you will get a rule-lock pair that can satisfy the constraints represented by the "
                  "arrangements. (For example, a pattern of white cells surrounded by any-background cells will keep "
                  "stable whatever its surroundings are. You can right-click this '(...)' to get an example.)");
    };

    ImGui::AlignTextToFramePadding();
    imgui_StrTooltip("(...)", description);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        for (auto& l : board) {
            for (auto& s : l) {
                s = Any_background;
            }
        }
        for (int y = 1; y <= 4; ++y) {
            for (int x = 1; x <= 4; ++x) {
                board[y][x] = ((x == 2 || x == 3) && (y == 2 || y == 3)) ? O : I;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        for (auto& l : board) {
            for (auto& s : l) {
                s = Any_background;
            }
        }
    }
    ImGui::SameLine();
    const bool empty = [] {
        for (const auto& l : board) {
            for (const auto& s : l) {
                if (s != Any_background) {
                    return false;
                }
            }
        }
        return true;
    }();
    if (empty) {
        ImGui::BeginDisabled();
    }
    const bool ret = ImGui::Button("Adopt");
    if (empty) {
        ImGui::EndDisabled();
        imgui_ItemTooltip("Empty.");
    }

    // Display-only; the value of `state_lbutton` is controlled by mouse-scrolling.
    ImGui::BeginDisabled();
    for (const stateE s : {O, I, O_background, I_background}) {
        if (s != O) {
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        }
        // No need for unique ID here (as the return value is not used).
        ImGui::RadioButton("##Radio", s == state_lbutton);
        // TODO: show message?
        // imgui_ItemTooltip("Scroll in the board to change the value for left-click.");
        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        ImGui::Dummy(square_size());
        put_col(s);
    }
    ImGui::EndDisabled();

    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    for (int y = 0; y < r; ++y) {
        for (int x = 0; x < r; ++x) {
            if (x != 0) {
                ImGui::SameLine();
            }
            const bool editable = y >= 1 && y < r - 1 && x >= 1 && x < r - 1;
            stateE& state = board[y][x];

            // No need for unique ID here (as IsItemHovered + IsMouseDown doesn't rely on ID).
            ImGui::InvisibleButton("##Invisible", square_size(),
                                   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            if (editable && ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    state = state_rbutton;
                } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    state = state_lbutton;
                }
            }
            put_col(state, !editable /*-> disabled*/);
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
        aniso::moldT mold{};
        for (int y = 1; y < r - 1; ++y) {
            for (int x = 1; x < r - 1; ++x) {
                if (board[y][x] == O || board[y][x] == I) {
                    // For example:
                    //  O   O_b  I                  001       001
                    // [Any] O   O  will result in [0]00 and [1]00 being set.
                    //  I_b  I  I_b                 111       111
                    aniso::for_each_code([&](aniso::codeT code) {
                        auto imbue = [](bool& b, stateE state) {
                            if (state == O || state == O_background) {
                                b = 0;
                            } else if (state == I || state == I_background) {
                                b = 1;
                            }
                        };

                        aniso::situT situ = aniso::decode(code);

                        imbue(situ.q, board[y - 1][x - 1]);
                        imbue(situ.w, board[y - 1][x]);
                        imbue(situ.e, board[y - 1][x + 1]);
                        imbue(situ.a, board[y][x - 1]);
                        imbue(situ.s, board[y][x]);
                        imbue(situ.d, board[y][x + 1]);
                        imbue(situ.z, board[y + 1][x - 1]);
                        imbue(situ.x, board[y + 1][x]);
                        imbue(situ.c, board[y + 1][x + 1]);

                        mold.rule[aniso::encode(situ)] = board[y][x] == O ? 0 : 1;
                        mold.lock[aniso::encode(situ)] = true;
                    });
                }
            }
        }
        out = mold;
    }
}
