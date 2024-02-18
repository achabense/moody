#pragma once

#include <algorithm>
#include <string>

#include "imgui.h"
#include "imgui_internal.h" // TODO: record dependency...

// Unlike ImGui::TextWrapped, doesn't take fmt str...
// TODO: the name is awful...
inline void imgui_str(std::string_view str) { //
    ImGui::TextUnformatted(str.data(), str.data() + str.size());
}

inline void imgui_strwrapped(std::string_view str) {
    ImGui::PushTextWrapPos(0.0f);
    imgui_str(str);
    ImGui::PopTextWrapPos();
}

inline void imgui_strcolored(const ImVec4& col, std::string_view str) {
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    imgui_str(str);
    ImGui::PopStyleColor();
}

inline void imgui_strdisabled(std::string_view str) {
    imgui_strcolored(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], str);
}

// ~ referring to ImGui::InputScalar; recheck...
// TODO: better name; allow/disallow ++/-- by mouse scrolling?
inline bool imgui_int_slider(const char* label, int* v, int v_min, int v_max) {
    if (ImGui::GetCurrentWindow()->SkipItems) {
        return false;
    }

    int v2 = *v;

    const float r = ImGui::GetFrameHeight();
    const float s = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::BeginGroup();
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - 2 * (r + s)));
    ImGui::SliderInt("", &v2, v_min, v_max, "%d", ImGuiSliderFlags_NoInput);
    ImGui::PushButtonRepeat(true);
    ImGui::SameLine(0, s);
    // TODO: whether to push&pop framepadding?
    if (ImGui::Button("-", ImVec2(r, r))) {
        --v2;
    }
    ImGui::SameLine(0, s);
    if (ImGui::Button("+", ImVec2(r, r))) {
        ++v2;
    }
    ImGui::PopButtonRepeat();
    // TODO: FindRenderedTextEnd is a function in imgui_internal.h...
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    if (label != label_end) {
        ImGui::SameLine(0, s);
        imgui_str(std::string_view(label, label_end));
    }
    ImGui::PopID();
    ImGui::EndGroup();

    v2 = std::clamp(v2, v_min, v_max);
    const bool changed = *v != v2;
    *v = v2;
    return changed;
}

inline bool imgui_keypressed(ImGuiKey key, bool repeat) {
    return !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(key, repeat);
};

// TODO: other mouse functions...
// TODO: rename to _mouse_XXX?
inline bool imgui_scrolling() { return ImGui::GetIO().MouseWheel != 0; }

inline bool imgui_scrolldown() { return ImGui::GetIO().MouseWheel < 0; }

inline bool imgui_scrollup() { return ImGui::GetIO().MouseWheel > 0; }

struct [[nodiscard]] imgui_window {
    imgui_window(const imgui_window&) = delete;
    imgui_window& operator=(const imgui_window&) = delete;

    const bool visible;
    // TODO: refine interface and documentation
    explicit imgui_window(const char* name, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, nullptr, flags)) {}
    explicit imgui_window(const char* name, bool* p_open, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, p_open, flags)) {}
    ~imgui_window() {
        ImGui::End(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

struct [[nodiscard]] imgui_childwindow {
    imgui_childwindow(const imgui_childwindow&) = delete;
    imgui_childwindow& operator=(const imgui_childwindow&) = delete;

    const bool visible;
    explicit imgui_childwindow(const char* name, const ImVec2& size = {}, ImGuiChildFlags child_flags = {},
                               ImGuiWindowFlags window_flags = {})
        : visible(ImGui::BeginChild(name, size, child_flags, window_flags)) {}
    ~imgui_childwindow() {
        ImGui::EndChild(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

class [[nodiscard]] imgui_itemtooltip {
    bool begun = false;

public:
    imgui_itemtooltip(const imgui_itemtooltip&) = delete;
    imgui_itemtooltip& operator=(const imgui_itemtooltip&) = delete;

    explicit imgui_itemtooltip(bool& toggle) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
            // TODO: still have slight control conflicts...
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
                toggle = !toggle;
            }
            if (toggle) {
                // TODO: wrap according to actual tooltip contents...
                // imgui_str("Double right click to turn on/off the tooltip");
                ImGui::BeginTooltip();
                begun = true;
            }
        }
    }
    ~imgui_itemtooltip() {
        if (begun) {
            ImGui::EndTooltip();
        }
    }

    explicit operator bool() const { return begun; }
};
