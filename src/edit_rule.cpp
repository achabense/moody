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

// `subsetT` (and `mapperT` pair) are highly customizable. However, for sanity there is no plan to
// support user-defined subsets in the gui part.
class subset_selector {
    aniso::subsetT current;

    struct termT {
        const char* const title;
        const aniso::subsetT* const set;
        const char* const description;
        bool selected = false;
        bool disabled = false; // current & set -> empty.
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
            t.disabled = !aniso::subsetT::common_rule(*t.set, current);
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
        terms_misc.emplace_back(
            "Hex", &ignore_hex,
            "Rules that emulate hexagonal neighborhood (by making the values independent of 'e/z'). "
            "See the last line for demonstration.");
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
        terms_native.emplace_back("-", &native_refl_asd, "Ditto, the reflection axis is `-`.");
        terms_native.emplace_back("\\", &native_refl_qsc, "Ditto, the reflection axis is `\\`.");
        terms_native.emplace_back("/", &native_refl_esz, "Ditto, the reflection axis is `/`.");
        terms_native.emplace_back("C2", &native_C2, "Rules that preserve C2 symmetry (2-fold rotational symmetry).");
        terms_native.emplace_back("C4", &native_C4,
                                  "C4 symmetry (4-fold rotational symmetry). This is a strict subset of C2.");

        terms_totalistic.emplace_back(
            "Tot", &native_tot_exc_s,
            "Outer-totalistic MAP rules. That is, the values are dependent on 's' and 'q+w+e+a+d+z+x+c'.\n\n"
            "(This is where the B/S notation applies.)");
        terms_totalistic.emplace_back(
            "Tot(+s)", &native_tot_inc_s,
            "Inner-totalistic MAP rules. That is, the values are only dependent on "
            "'q+w+e+a+s+d+z+x+c' (including 's'). This is a strict subset of outer-totalistic rules.");
        terms_totalistic.emplace_back("Hex", &hex_tot_exc_s, "Outer-totalistic hexagonal rules.");
        terms_totalistic.emplace_back("Hex(+s)", &hex_tot_inc_s, "Inner-totalistic hexagonal rules.");
        terms_totalistic.emplace_back("Von", &von_tot_exc_s, "Outer-totalistic von-Neumann rules.");
        terms_totalistic.emplace_back("Von(+s)", &von_tot_inc_s, "Inner-totalistic von-Neumann rules.");

        // q w -    q w
        // a s d ~ a s d
        // - x c    x c"
        terms_hex.emplace_back("All", &hex_isotropic,
                               "Rules that emulate isotropic hexagonal rules.\n\n"
                               "(Remember to unselect native-symmetry terms when working with this line.)");
        terms_hex.emplace_back(
            "a-d", &hex_refl_asd,
            "Rules that emulate reflection symmetry in the hexagonal tiling, taking the axis from 'a to d'.");
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

    const aniso::subsetT& select_subset(const sync_point& target) {
        assert_implies(!target.enable_lock, target.current.lock == aniso::moldT::lockT{});
        const aniso::moldT& mold = target.current;

        enum ringE { Contained, Compatible, Incompatible };
        enum centerE { Selected, Including, Disabled, None }; // TODO: add "equals" relation?
        auto put_term = [size = square_size()](ringE ring, centerE center, const char* title /* Optional */,
                                               bool interactive) -> bool {
            ImGui::Dummy(size);
            const bool hit = interactive && center != Disabled && ImGui::IsItemClicked(ImGuiMouseButton_Left);

            const ImU32 cent_col = center == Selected    ? IM_COL32(65, 150, 255, 255) // Roughly _ButtonHovered
                                   : center == Including ? IM_COL32(25, 60, 100, 255)  // Roughly _Button
                                                         : IM_COL32_BLACK_TRANS;
            const ImU32 ring_col = ring == Contained    ? IM_COL32(0, 255, 0, 255)   // Light green
                                   : ring == Compatible ? IM_COL32(0, 100, 0, 255)   // Dull green
                                                        : IM_COL32(200, 45, 0, 255); // Red
            ImU32 title_col = IM_COL32_WHITE;
            if (center == Disabled) {
                title_col = IM_COL32(150, 150, 150, 255);
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
                imgui_ItemRectFilled(IM_COL32(255, 255, 255, 45));
            }

            return hit;
        };
        auto check = [&](termT& term, bool show_title = false) -> void {
            if (put_term(term.set->contains(mold.rule)        ? Contained
                         : aniso::compatible(*term.set, mold) ? Compatible
                                                              : Incompatible,
                         term.selected                 ? Selected
                         : term.set->includes(current) ? Including
                         : term.disabled               ? Disabled
                                                       : None,
                         show_title ? term.title : nullptr, true)) {
                assert(!term.disabled);
                term.selected = !term.selected;
                update_current();
            }

            imgui_ItemTooltip(term.description);
        };

        {
            ImGui::AlignTextToFramePadding();
            imgui_StrTooltip("(...)", [&] {
                auto explain = [&](ringE ring, centerE center, const char* desc) {
                    put_term(ring, center, nullptr, false);
                    ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                    ImGui::AlignTextToFramePadding(); // `Dummy` does not align automatically.
                    imgui_Str(": ");
                    ImGui::SameLine(0, 0);
                    imgui_Str(desc);
                };

                imgui_Str(
                    "The following terms represent subsets of the MAP rules. You can select these terms freely - "
                    "the program will calculate the intersection of the selected subsets (with the whole MAP set), and "
                    "help you explore rules in it.\n"
                    "This set is later called \"working set\". For example, \"the rule belongs to the working set\" "
                    "has the same meaning as \"the rule belongs to every selected subset\". If nothing is selected, "
                    "the working set will be the whole MAP set.");
                ImGui::Separator();
                imgui_Str("The ring color reflects the relation between the subset and the current rule-lock pair:");
                explain(Contained, None, "The rule belongs to this subset.");
                if (!target.enable_lock) {
                    explain(Compatible, None, "The rule does not belong to this subset.");
                } else {
                    explain(
                        Compatible, None,
                        "The rule does not belong to this subset, but there exist rules in the subset that meet the "
                        "constraints (locked values) posed by rule-lock pair.\n"
                        "(Notice that the [intersection] of such subsets may still contain no rules that satisfy the "
                        "constraints. )");
                    explain(
                        Incompatible, None,
                        "The rule does not belong to this subset, and the constraints cannot be satisfied by any rule "
                        "in this subset.");
                }

                ImGui::Separator();
                imgui_Str("The center color reflects the selection details:");
                explain(Compatible, None, "Not selected.");
                explain(Compatible, Selected, "Selected.");
                explain(Compatible, Including,
                        "Not selected, but the working set already belongs to this subset, so it will behave "
                        "as if this is selected too.");
                explain(Compatible, Disabled, "Not selectable, otherwise the working set will be empty.");
            });
            ImGui::SameLine();
            imgui_Str("Subsets ~");
            ImGui::SameLine();
            put_term(current.contains(mold.rule)        ? Contained
                     : aniso::compatible(current, mold) ? Compatible
                                                        : Incompatible,
                     None, nullptr, false);
            imgui_ItemTooltip("The working set. (See '(...)' for explanation.)");

            // TODO: `static` for convenience. This must be refactored when there are to be multiple instances.
            static bool hide_details = false;
            if (!hide_details) {
                ImGui::SameLine();
                if (ImGui::Button("Clear")) {
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
                imgui_ItemTooltip("Select every subset that the current rule belongs to.");
            }
            ImGui::SameLine();
            ImGui::Checkbox("Hide details", &hide_details);
            if (hide_details) {
                return current;
            }
        }

        ImGui::Separator();
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

        assert(!current.empty());
        return current;
    }
};

class batch_adapter {
    static const int page_size = 6, perline = 3;
    using pageT = std::array<aniso::ruleT, page_size>;
    std::vector<pageT> pages;
    int page_no = 0;

    previewer::configT config{previewer::configT::_220_160};

public:
    void display(std::function<aniso::ruleT()> gen, sync_point& out, bool bind) {
        auto make_page = [&] {
            for (aniso::ruleT& rule : pages.emplace_back()) {
                rule = gen();
            }
            page_no = pages.size() - 1;
        };

        auto set_page = [&](int p, bool force = false) {
            if (p < 0) {
                return;
            } else if (p < pages.size()) {
                page_no = p;
            } else if (force) {
                make_page();
            }
        };

        if (bind) {
            sequence::bind_to(ImGui::GetID(">>>"));
        }
        if (ImGui::Button("Generate")) {
            make_page();
            sequence::bind_to(ImGui::GetID(">>>"));
        }
        quick_info("v '>>>' has the same effect when you are at the last page.");
        ImGui::SameLine();
        sequence::seq(
            "<|", "<<", ">>>", "|>", //
            [&] { set_page(0); }, [&] { set_page(page_no - 1); }, [&] { set_page(page_no + 1, true); },
            [&] { set_page((int)pages.size() - 1); });
        ImGui::SameLine();
        if (!pages.empty()) {
            ImGui::Text("Total:%d At:%d", (int)pages.size(), page_no + 1);
        } else {
            ImGui::Text("Total:%d At:N/A", (int)pages.size());
        }
        quick_info("^ Right-click to clear.");
        // (Using the same style as in `frame_main`.)
        bool clear = false; // Put off for more stable visual.
        if (ImGui::BeginPopupContextItem("", ImGuiPopupFlags_MouseButtonRight)) {
            clear = ImGui::Selectable("Clear");
            ImGui::EndPopup();
        }

        if (!pages.empty()) {
            ImGui::SameLine(0, 16);
            ImGui::BeginDisabled();
            bool b = true;
            ImGui::Checkbox("Preview mode", &b);
            ImGui::EndDisabled();
            ImGui::SameLine();
            config.set("Settings", "Restart");
            // Guarantee the child is at least as wide as the previous line.
            ImGui::SetNextWindowSizeConstraints(
                ImVec2(ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x - ImGui::GetStyle().WindowPadding.x, 0),
                ImVec2(FLT_MAX, FLT_MAX));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(32, 32, 32, 255));
            if (auto child = imgui_ChildWindow("Page", {}, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY)) {
                for (int j = 0; const aniso::ruleT& rule : pages[page_no]) {
                    if (j % perline != 0) {
                        ImGui::SameLine(0, 16); // (The same value as in `edit_rule`.)
                    } else {
                        ImGui::Separator();
                    }
                    ++j;
                    ImGui::BeginGroup();
                    ImGui::PushID(j);
                    if (ImGui::Button(">> Cur")) {
                        out.set_rule(rule);
                    }
                    ImGui::PopID();
                    previewer::preview(j, config, rule);
                    ImGui::EndGroup();
                }
            }
            ImGui::PopStyleColor();
        }

        if (clear && !pages.empty()) {
            pages = std::vector<pageT>{};
            page_no = 0;
        }
    }
};

void edit_rule(sync_point& sync, bool& bind_undo) {
    // (A "reason" parameter would be convenient, but it works poorly with clang-format...)
    auto guarded_block = [](const bool enable, const auto& fn /*, const char* reason = nullptr*/) {
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
    auto itemtooltip_with_previewer = [](const auto& desc) {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(24, 24, 24, 240));
        imgui_ItemTooltip(desc);
        ImGui::PopStyleColor();
    };

    static subset_selector selector;
    const aniso::subsetT& subset = selector.select_subset(sync);
    assert(!subset.empty());

    const aniso::moldT& mold = sync.current;
    const bool compatible = aniso::compatible(subset, mold);
    const bool contained = subset.contains(mold.rule);
    assert_implies(contained, compatible);

    ImGui::Separator();

    // Select mask.
    char chr_0 = '0', chr_1 = '1';
    const aniso::maskT& mask = [&]() -> const aniso::maskT& {
        const char* const about_mask =
            "A mask is an arbitrary rule (in the working set) to perform XOR masking for other rules.\n\n"
            "Some rules are special enough (for example, 'Zero/Identity'), so that the values masked by them have "
            "natural interpretations.\n"
            "If a rule belongs to the working set, its distance to the masking rule can be defined as the number "
            "of groups where they have different values.\n\n"
            "(The exact workings are more complex than explained here. For details see the \"Subset, mask ...\" "
            "section in \"Documents\".)";

        static aniso::maskT mask_custom{{}};

        enum maskE { Zero, Identity, Native, Custom }; // TODO: better name for 'Native'?
        struct termT {
            const char* label;
            const char* desc;
            char chr_0, chr_1;
        };
        static const termT mask_terms[]{
            {"Zero",
             "The all-zero rule.\n"
             "The masked values show actual values (different:'1' ~ 1, same:'0' ~ 0), and the distance "
             "to this rule shows how many groups return 1.",
             '0', '1'},

            {"Identity",
             "The rule that preserves the value of the center cell in all situations.\n"
             "The masked values show whether the cell will \"flip\" (different:'f' ~ flip, same:'.' ~ won't flip), "
             "and the distance to this rule reflects how \"volatile\" a rule is.",
             '.', 'f'},

            {"Native",
             "A rule calculated by the program that belongs to the working set. Depending on what subsets "
             "are selected, it may be the same as zero-rule, or identity-rule, or just an ordinary rule in the set.\n"
             "It's recommended you try this only when there are no other rules known to belong to the working "
             "set (neither 'Zero' nor 'Identity' works, and there are no existing rules to serve as custom mask).",
             'o', 'i'},

            {"Custom",
             "Custom rule; you can click '<< Cur' to set this to the current rule.\n"
             "Different:'i', same:'o'. This is a useful tool to help find interesting rules based on existing ones. "
             "For example, the smaller the distance is, the more likely that the rule behaves similar to the masking "
             "rule.",
             'o', 'i'}};

        static maskE mask_tag = Zero;
        const aniso::maskT* const mask_ptrs[]{&aniso::mask_zero, &aniso::mask_identity, &subset.get_mask(),
                                              &mask_custom};

        if (!subset.contains(*mask_ptrs[mask_tag])) {
            mask_tag = Native;
        }

        ImGui::AlignTextToFramePadding();
        imgui_StrTooltip("(...)", about_mask);
        ImGui::SameLine();
        imgui_Str("Mask ~");
        for (const maskE m : {Zero, Identity, Native, Custom}) {
            const bool m_avail = subset.contains(*mask_ptrs[m]);

            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            guarded_block(m_avail, [&] {
                if (ImGui::RadioButton(mask_terms[m].label, mask_tag == m)) {
                    mask_tag = m;
                }
            });

            itemtooltip_with_previewer([&] {
                if (!m_avail) {
                    imgui_Str("This rule does not belong to the working set.");
                    ImGui::Separator();
                }

                imgui_Str(mask_terms[m].desc);
                ImGui::Separator();
                previewer::preview(-1, previewer::configT::_220_160, *mask_ptrs[m], false);
                ImGui::SameLine();
                imgui_Str(aniso::to_MAP_str(*mask_ptrs[m]));
            });
        }

        ImGui::SameLine();
        guarded_block(contained, [&] {
            if (ImGui::Button("<< Cur")) {
                mask_custom = {mold.rule};
                mask_tag = Custom;
            }
        });
        if (!contained) {
            imgui_ItemTooltip("The current rule does not belong to the working set.");
        }

        chr_0 = mask_terms[mask_tag].chr_0;
        chr_1 = mask_terms[mask_tag].chr_1;
        assert(subset.contains(*mask_ptrs[mask_tag]));
        return *mask_ptrs[mask_tag];
    }();

    ImGui::Separator();

    sync.display_if_enable_lock([&](bool visible) {
        if (!visible) {
            return;
        }
        if (ImGui::Button("Clear##current")) {
            sync.set_lock({});
        }
        ImGui::SameLine();
        guarded_block(contained, [&] {
            if (ImGui::Button("Enhance##current")) {
                sync.set_lock(aniso::enhance_lock(subset, mold));
            }
        });
        imgui_ItemTooltip([&] {
            if (!contained) {
                imgui_Str("The current rule does not belong to the working set.");
                ImGui::Separator();
            }
            imgui_Str("\"Saturate\" the locked groups in the working set.\n\n"
                      "For example, suppose the working set is the isotropic set, and the constraints represent the "
                      "capture of a glider in one direction, then this can enhance the constraints to gliders in all "
                      "directions.");
        });
        ImGui::SameLine();
        int count = 0;
        aniso::for_each_code([&](aniso::codeT code) { count += sync.current.lock[code]; });
        ImGui::Text("Locked: %d/512", count);
    });

    const aniso::partitionT& par = subset.get_par();
    const auto scanlist = aniso::scanT::scanlist(par, mask, mold);

    static bool preview_mode = false;
    static previewer::configT config{previewer::configT::_220_160};
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
        const bool has_lock = c_free != c_group;

        assert(contained == (c_x == 0));
        assert(compatible == (c_locked_x == 0));

        guarded_block(compatible, [&] {
            // `dist`: The "distance" to the masking rule the randomization want to achieve.
            // (which does not make sense when !compatible)
            static double rate = 0.5;
            int free_dist = round(rate * c_free);

            static bool exact_mode = false;
            if (ImGui::Button(exact_mode ? "Exactly###Mode" : "Around ###Mode")) {
                exact_mode = !exact_mode;
            }
            quick_info("< Around / Exactly.");
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            ImGui::SetNextItemWidth(item_width);
            if (imgui_StepSliderInt("##Quantity", &free_dist, 0, c_free,
                                    compatible ? (has_lock ? "(Free) %d" : "%d") : "N/A") &&
                c_free != 0) {
                rate = double(free_dist) / c_free;
                assert(round(rate * c_free) == free_dist);
            }
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            static bool show_rand = false;
            const bool clicked = ImGui::Button("Randomize");
            if (clicked) {
                assert(compatible); // Otherwise, the button should be disabled.
                show_rand = true;
                ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
                if (ImGui::IsMousePosValid()) {
                    const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
                    ImGui::SetNextWindowPos(ImVec2(mouse_pos.x + 2, mouse_pos.y + 2), ImGuiCond_Always);
                }
            }
            // TODO: ideally, !compatible should only disable generation ('>>>' at last page.).
            if (show_rand && compatible) {
                if (auto window = imgui_Window("Randomize", &show_rand,
                                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
                    static batch_adapter batch;
                    batch.display(
                        [&]() -> aniso::ruleT {
                            if (exact_mode) {
                                return aniso::randomize_c(subset, mask, mold, global_mt19937(), free_dist);
                            } else {
                                return aniso::randomize_p(subset, mask, mold, global_mt19937(), rate);
                            }
                        },
                        sync, clicked);
                }
            }
        });
        if (!compatible) {
            imgui_ItemTooltip("Incompatible.");
        }
        ImGui::SameLine();
        imgui_StrTooltip("(?)", "Generate random rules with intended distance to the mask. A window with "
                                "'Generate' will appear after you click 'Randomize'.\n\n"
                                "For example, if you are using the 'Zero' mask and distance = 51, 'Generate' "
                                "will generate rules with 51 groups having '1'.\n"
                                "Also, suppose the current rule belongs to the working set, you can set it to "
                                "the custom mask, and 'Generate' with low distance to generate rules that "
                                "are \"close\" to it.");

        guarded_block(compatible, [&] {
            sequence::seq(
                "<00..", "Prev", "Next", "11..>", //
                [&] { sync.set_rule(aniso::seq_mixed::first(subset, mask, mold)); },
                [&] { sync.set_rule(aniso::seq_mixed::prev(subset, mask, mold)); },
                [&] { sync.set_rule(aniso::seq_mixed::next(subset, mask, mold)); },
                [&] { sync.set_rule(aniso::seq_mixed::last(subset, mask, mold)); },
                contained ? nullptr : "The current rule does not belong to the working set.");
        });
        if (!compatible) {
            imgui_ItemTooltip("Incompatible.");
        }
        ImGui::SameLine();
        imgui_StrTooltip("(?)",
                         "Iterate through the whole working set, by firstly iterating through all rules that have "
                         "distance = 1 to the masking rule, then 2, 3, ..., until max distance.\n\n"
                         "For example, suppose the current rule belongs to the working set. To iterate through all "
                         "rules that have distance = 1 to the current rule, you can:\n"
                         "1. '<< Cur' to set the current rule as the custom mask.\n"
                         "2. 'Next' to iterate. The left/right arrow key will be bound to 'Prev/Next' after you "
                         "click the button.");

        // TODO: move this to the right plane (before 'Restart')?
        ImGui::SameLine();
        guarded_block(true /* Unconditional */, [&] {
            if (ImGui::Button("Rev")) {
                sync.set_mold(aniso::trans_reverse(mold));
            }
        });
        itemtooltip_with_previewer([&] {
            imgui_Str("Get the 0/1 reversal dual of the current rule.");
            ImGui::Separator();
            const aniso::ruleT rev = aniso::trans_reverse(mold).rule;
            if (rev != mold.rule) {
                imgui_Str("Preview:");
                ImGui::SameLine();
                previewer::preview(-1, previewer::configT::_220_160, rev, false);
            } else {
                imgui_Str("(The result will be the same as the current rule, as it is self-complementary.)");
            }
        });
#if 0
        // TODO: whether to expose this? This is conceptually well-defined, but very hard to explain and
        // the effect may not be as expected.
        // TODO: ... sometimes this is really useful...
        // combine with batch preview -> approximated by different sets?
        ImGui::SameLine();
        guarded_block(compatible, [&] {
            if (ImGui::Button("Approximate")) {
                sync.set_rule(aniso::approximate(subset, mold));
            }
        });
        itemtooltip_with_previewer([&] {
            // TODO: refine message; and approximation is not very useful in practice...
            imgui_Str("When the current rule does not belong to the working set (but the "
                      "constraints can be met), you can try this to get the closest rule in the set.");
            ImGui::Separator();
            if (!contained) {
                imgui_Str("Preview:");
                ImGui::SameLine();
                previewer::preview(-1, previewer::configT::_220_160, aniso::approximate(subset, mold), false);
            } else {
                imgui_Str("(The current rule already belongs to the working set.)");
            }
        });
#endif
        ImGui::SameLine();
        ImGui::Checkbox("Preview mode", &preview_mode);
        quick_info("^ Try this!");
        if (preview_mode) {
            ImGui::SameLine();
            config.set("Settings", "Restart");
        }

        if (contained) {
            std::string str = std::format("Groups:{} ({}:{} {}:{})", c_group, chr_1, c_1, chr_0, c_0);
            if (c_free != c_group) {
                const int c_free_1 = c_1 - c_locked_1, c_free_0 = c_0 - c_locked_0;
                str += std::format(" Free:{} ({}:{} {}:{}) Locked:{} ({}:{} {}:{})", c_free, chr_1, c_free_1, chr_0,
                                   c_free_0, c_locked_1 + c_locked_0, chr_1, c_locked_1, chr_0, c_locked_0);
            }
            imgui_Str(str);
        } else if (compatible) {
            ImGui::Text("Groups:%d !contained", c_group);
            ImGui::SameLine();
            imgui_StrTooltip(
                "(?)",
                "The current rule does not belong to the working set. (Check the dull-blue groups for details.)\n\n"
                "(To get a rule in the working set you can try any of '<00..', '11..>', or 'Randomize'.)");
        } else {
            ImGui::Text("Groups:%d !compatible", c_group);
            ImGui::SameLine();
            imgui_StrTooltip("(?)", "There don't exist rules in the working set that have the same locked "
                                    "values as the current rule. (Check the red groups for details.)");
        }
    }

    if (preview_mode) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(32, 32, 32, 255));
    }
    if (auto child = imgui_ChildWindow("Groups")) {
        // set_scroll_by_up_down(ImGui::GetFrameHeight() * (preview_mode ? 2 : 1));
        const char labels_normal[2][3]{{'-', chr_0, '\0'}, {'-', chr_1, '\0'}};
        const char labels_preview[2][9]{{'-', chr_0, ' ', '-', '>', ' ', chr_1, ':', '\0'},
                                        {'-', chr_1, ' ', '-', '>', ' ', chr_0, ':', '\0'}};
        const aniso::ruleT_masked masked = mask ^ mold.rule;

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
            int perline = floor((ImGui::GetContentRegionAvail().x + spacing) / (group_size + spacing));
            return std::max(perline, 1);
        }();

        int n = 0;
        assert(par.k() <= 512);
        std::array<bool, 512> shown{};
        for (auto select : {+[](const aniso::scanT& c) { return !c.all_0() && !c.all_1(); }, //
                            +[](const aniso::scanT& c) { return !c.any_locked(); },          //
                            +[](const aniso::scanT& c) { return c.any_locked(); }}) {
            par.for_each_group([&](int j, const aniso::groupT& group) {
                if (shown[j] || !select(scanlist[j])) {
                    return;
                }
                shown[j] = true;

                const bool has_lock = scanlist[j].any_locked();
                const bool pure = scanlist[j].all_0() || scanlist[j].all_1();
                if (n % perline != 0) {
                    ImGui::SameLine(0, spacing);
                } else {
                    ImGui::Separator();
                }
                ++n;

                const bool incomptible = scanlist[j].locked_0 != 0 && scanlist[j].locked_1 != 0;
                const aniso::codeT head = group[0];

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

                if (preview_mode) {
                    ImGui::BeginGroup();
                }
                ImGui::PushStyleColor(ImGuiCol_Button, button_color[0]);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_color[1]);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_color[2]);
                if (code_button(head, zoom)) {
                    aniso::ruleT rule = mold.rule;
                    for (aniso::codeT code : group) {
                        rule[code] = !rule[code];
                    }
                    bind_undo = true;
                    sync.set_rule(rule);
                } else if (sync.enable_lock && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    aniso::moldT::lockT lock = mold.lock;
                    for (aniso::codeT code : group) {
                        lock[code] = !has_lock;
                    }
                    // bind_undo = true;
                    sync.set_lock(lock);
                }
                ImGui::PopStyleColor(3);

                const bool show_group = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);

                ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                align_text(ImGui::GetItemRectSize().y);
                imgui_Str(preview_mode ? labels_preview[masked[head]] : labels_normal[masked[head]]);
                if (has_lock) {
                    const ImU32 col = scanlist[j].all_locked() ? IM_COL32_WHITE : IM_COL32(128, 128, 128, 255);
                    imgui_ItemRect(col, ImVec2(-2, -2));
                }

                if (preview_mode) {
                    previewer::preview(j, config, [&] {
                        aniso::ruleT rule = mold.rule;
                        for (aniso::codeT code : group) {
                            rule[code] = !rule[code];
                        }
                        return rule;
                    });
                    ImGui::EndGroup();
                }

                if (show_group && ImGui::BeginTooltip()) {
                    imgui_Str(sync.enable_lock ? "Left-click to flip the values.\nRight-click to toggle the lock."
                                               : "Left-click to flip the values.");
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
        }
        assert(n == par.k());

        ImGui::PopStyleVar(1);
    }
    if (preview_mode) {
        ImGui::PopStyleColor();
    }
}

// TODO: move to "apply_rule.cpp"? (as this is a special type of capture...)
void static_constraints(sync_point& out) {
    enum stateE { Any_background, O, I, O_background, I_background };
    const int r = 10;
    static stateE board[r][r]{/* Any_background... */};
    static stateE state_lbutton = I;
    const stateE state_rbutton = Any_background;
    static const ImU32 cols[5]{IM_COL32(100, 100, 100, 255), //
                               IM_COL32(0, 0, 0, 255),       //
                               IM_COL32(255, 255, 255, 255), //
                               IM_COL32(80, 0, 80, 255),     //
                               IM_COL32(200, 0, 200, 255)};
    static const auto description = [] {
        auto term = [](stateE s, const char* desc) {
            ImGui::Dummy(square_size());
            imgui_ItemRectFilled(cols[s]);
            imgui_ItemRect(IM_COL32(200, 200, 200, 255));
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            ImGui::AlignTextToFramePadding(); // `Dummy` does not align automatically.
            imgui_Str(desc);
        };

        imgui_Str("(You can right-click this '(...)' to get an example.)");
        ImGui::Separator();
        imgui_Str("Operations:\n"
                  "Left-click the cell to set the value.\n"
                  "Right-click to set back to any-background.\n"
                  "Scroll in the board to change the value for left-click.\n");
        term(O, ": Supposed to remain 0.");
        term(I, ": Supposed to remain 1.");
        term(O_background, ": Background 0.");
        term(I_background, ": Background 1.");
        term(Any_background, ": Any background.");
        imgui_Str("By 'Adopt', you will get a rule-lock pair that can satisfy the constraints represented by the "
                  "arrangements. (For example, a pattern of white cells surrounded by any-background cells will keep "
                  "stable whatever its surroundings are.)");
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
        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        ImGui::Dummy(square_size());
        imgui_ItemRectFilled(cols[s]);
        imgui_ItemRect(IM_COL32(200, 200, 200, 255));
    }
    ImGui::EndDisabled();

    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    for (int y = 0; y < r; ++y) {
        for (int x = 0; x < r; ++x) {
            if (x != 0) {
                ImGui::SameLine();
            }

            // No need for unique ID here (as IsItemHovered + IsMouseDown doesn't rely on ID).
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
        out.set_mold(mold);
    }
}
