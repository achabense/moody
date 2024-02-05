// TODO: not used in this source file...
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "app.hpp"

// TODO: should support clipboard paste in similar ways...
// TODO: support in-memory loadings. (e.g. tutorial about typical ways to find interesting rules)

// TODO: refine...
struct fileT {
    struct lineT {
        bool has_rule;
        int id; // valid if has_rule.
        std::string text;
    };

    std::filesystem::path m_path;
    std::vector<lineT> m_lines;
    std::vector<legacy::compressT> m_rules;
    int pointing_at = 0; // valid if !m_rules.empty().

    // TODO: whether to throw if not found?
    fileT(std::filesystem::path path) : m_path(std::move(path)) {
        std::ifstream ifs(m_path);
        std::string line;

        int id = 0;
        // TODO: MSVC shows a warning here... can getline safely deal with moved-from string?
        while (std::getline(ifs, line)) {
            auto extr = legacy::extract_rules(line);
            bool has_rule = !extr.empty();
            m_lines.emplace_back(has_rule, has_rule ? id++ : 0, std::move(line));
            if (has_rule) {
                m_rules.push_back(extr[0]);
                // TODO: (how&) whether to support more fine-grained (in-line) rule-location?
                // (only the first rule in each line is being recognized... (so `extract_rules` is wasteful)
            }
        }
    }

    // TODO: how to combine with file-nav (into a single window)?
    std::optional<legacy::ruleT> display(const legacy::ruleT& test_sync) {
        static const bool wrap = true; // TODO: (temporarily const)

        std::optional<legacy::ruleT> out;
        bool hit = false; // TODO: also sync on window appearing?
        const int total = m_rules.size();

        if (total != 0) {
            const bool in_sync = test_sync == m_rules[pointing_at];

            iter_pair(
                "<|", "prev", "next", "|>",                                              //
                [&] { hit = true, pointing_at = 0; },                                    //
                [&] { hit = true, pointing_at = std::max(0, pointing_at - 1); },         //
                [&] { hit = true, pointing_at = std::min(total - 1, pointing_at + 1); }, //
                [&] { hit = true, pointing_at = total - 1; }, false, false);
            ImGui::SameLine();
            ImGui::Text("Total:%d At:%d %s", total, pointing_at + 1, !in_sync ? "(click to sync)" : "");
            if (!in_sync && ImGui::IsItemHovered()) {
                const ImVec2 pos_min = ImGui::GetItemRectMin();
                const ImVec2 pos_max = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 30));
            }

            if (ImGui::IsItemClicked()) {
                hit = true;
            }

            // TODO: (temp) without ImGuiFocusedFlags_ChildWindows, clicking the child window will invalidate this.
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                if (imgui_keypressed(ImGuiKey_UpArrow, true)) {
                    hit = true, pointing_at = std::max(0, pointing_at - 1);
                }
                if (imgui_keypressed(ImGuiKey_DownArrow, true)) {
                    hit = true, pointing_at = std::min(total - 1, pointing_at + 1);
                }
            }
        } else {
            ImGui::Text("Not found");
        }

        const bool focus = hit;

        ImGui::Separator();

        if (auto child = imgui_childwindow("Child", {}, 0,
                                           wrap ? ImGuiWindowFlags_None : ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            // TODO: refine line-no logic (line-no or id-no)?
            for (int l = 1; const auto& [has_rule, id, text] : m_lines) {
                ImGui::TextDisabled("%2d ", l++);
                ImGui::SameLine();
                if (wrap) {
                    imgui_strwrapped(text);
                } else {
                    imgui_str(text);
                }
                if (has_rule) {
                    if (id == pointing_at) {
                        const ImVec2 pos_min = ImGui::GetItemRectMin();
                        const ImVec2 pos_max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 60));
                        if (!ImGui::IsItemVisible() && focus) {
                            ImGui::SetScrollHereY();
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        const ImVec2 pos_min = ImGui::GetItemRectMin();
                        const ImVec2 pos_max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 30));
                        if (ImGui::IsItemClicked()) {
                            pointing_at = id;
                            hit = true;
                        }
                    }
                }
            }
            ImGui::PopStyleVar();
        }

        if (hit) {
            assert(pointing_at >= 0 && pointing_at < total);
            return m_rules[pointing_at];
        } else {
            return std::nullopt;
        }
    }
};

// TODO: reset scroll-y for new files...
// TODO: show the last opened file in file-nav?

std::optional<legacy::ruleT> load_rule(const legacy::ruleT& test_sync) {
    static file_nav nav;
    static std::optional<fileT> file;

    std::optional<legacy::ruleT> out;

    bool close = false;
    if (file) {
        // TODO: the path (& file_nav's) should be copyable...

        // TODO (temp) selectable looks good but is easy to click by mistake...
        if (ImGui::SmallButton("Close")) {
            close = true;
        }
        imgui_str(cpp17_u8string(file->m_path));

        out = file->display(test_sync);
    } else {
        if (auto sel = nav.display()) {
            file.emplace(*sel);
            if (file->m_rules.empty()) {
                file.reset();
                logger::add_msg(1000ms, "No rules"); // TODO: better msg...
            }
        }
    }

    if (close) {
        file.reset();
    }

    // TODO: whether to support opening multiple files?
    // TODO: better layout...

    return out;
}
