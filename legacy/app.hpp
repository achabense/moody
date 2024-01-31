#pragma once

// TODO: is this necessary?
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "app_imgui.hpp"
#include "app_sdl.hpp"

// TODO: app_sdl.hpp may need to be refactored away.
// For example, it introduces both rule.hpp and tile.hpp, which is not needed for edit_rule.

inline std::mt19937& global_mt19937() {
    static std::mt19937 rand(time(0));
    return rand;
}

// Never empty.
// TODO: (gui) whether to support random-access mode?
class rule_recorder {
    std::vector<legacy::compressT> m_record;
    int m_pos;

public:
    rule_recorder() {
        m_record.emplace_back(legacy::game_of_life());
        m_pos = 0;
    }

    int size() const { return m_record.size(); }

    // [0, size() - 1]
    int pos() const { return m_pos; }

    // TODO: look for better names...
    // TODO: reconsider what should be done when already exists in the whole vec...
    void take(const legacy::ruleT& rule) {
        legacy::compressT cmpr(rule);
        if (cmpr != m_record[m_pos]) {
            m_record.push_back(cmpr);
            m_pos = m_record.size() - 1;
        }
    }

    // void replace_current(const legacy::ruleT& rule) { //
    //     m_record[m_pos] = legacy::compressT(rule);
    // }

    legacy::ruleT current() const {
        assert(m_pos >= 0 && m_pos < size());
        return static_cast<legacy::ruleT>(m_record[m_pos]);
    }

    void set_pos(int pos) { //
        m_pos = std::clamp(pos, 0, size() - 1);
    }
    void set_next() { set_pos(m_pos + 1); }
    void set_prev() { set_pos(m_pos - 1); }
    void set_first() { set_pos(0); }
    void set_last() { set_pos(size() - 1); }

    // TODO: reconsider m_pos logic...
    // void append(const std::vector<legacy::compressT>& vec) { //
    //     m_record.insert(m_record.end(), vec.begin(), vec.end());
    // }

    void replace(std::vector<legacy::compressT>&& vec) {
        if (!vec.empty()) {
            m_record.swap(vec);
            m_pos = 0;
        }
        // else???
    }
};

// TODO: rename...
const int FixedItemWidth = 220;

// TODO: consider other approaches (native nav etc) if possible...
// TODO: e.g. toggle between buttons by left/right... / clear binding...
inline bool imgui_enterbutton(const char* label) {
    static ImGuiID bind_id = 0;
    bool ret = ImGui::Button(label);
    const ImGuiID button_id = ImGui::GetItemID();
    if (ret) {
        bind_id = button_id;
    }
    if (bind_id == button_id) {
        // TODO: should not be invokable when in disabled scope...
        // (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0 ~ not disabled, but this requires
        // the internal header... Are there public ways to do the same thing?
        if (imgui_keypressed(ImGuiKey_Enter, false)) {
            ret = true;
        }
        const ImU32 col = ret ? IM_COL32(128, 128, 128, 255) : IM_COL32_WHITE;
        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), col);
    }
    return ret;
}

// TODO: support rollbacking diff rules?
// TODO: support rollbacking locks?
// TODO: for editing opt, support in-lock and outof-lock mode?

// TODO: allow/disallow scrolling...
inline void iter_pair(const char* tag_first, const char* tag_prev, const char* tag_next, const char* tag_last,
                      auto act_first, auto act_prev, auto act_next, auto act_last) {
    if (ImGui::Button(tag_first)) {
        act_first();
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    // TODO: _Left, _Right to toggle?
    if (imgui_enterbutton(tag_prev)) {
        act_prev();
    }
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    if (imgui_enterbutton(tag_next)) {
        act_next();
    }
    ImGui::EndGroup();
    if (ImGui::IsItemHovered()) {
        if (imgui_scrollup()) {
            act_prev();
        } else if (imgui_scrolldown()) {
            act_next();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(tag_last)) {
        act_last();
    }
};

std::optional<std::pair<legacy::ruleT, legacy::lockT>> static_constraints();
std::optional<legacy::ruleT> edit_rule(const legacy::ruleT& target, legacy::lockT& locked, const code_image& icons);

void edit_tile(const legacy::ruleT& target, legacy::lockT& locked, tile_image& img);
