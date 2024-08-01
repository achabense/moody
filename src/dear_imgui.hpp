#pragma once

#include <algorithm>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

// Follows `IM_COL32_XX`; note that `constexpr` cannot guarantee `fn(100, 255)` be
// calculated at compile time, especially in debug mode.
consteval ImU32 IM_COL32_GREY(ImU8 v, ImU8 alpha) { return IM_COL32(v, v, v, alpha); }

inline void imgui_ItemRect(ImU32 col, ImVec2 off_min = {0, 0}) {
    const ImVec2 pos_min = ImGui::GetItemRectMin();
    const ImVec2 pos_max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRect(pos_min + off_min, pos_max - off_min, col);
}

inline void imgui_ItemRectFilled(ImU32 col, ImVec2 off_min = {0, 0}) {
    const ImVec2 pos_min = ImGui::GetItemRectMin();
    const ImVec2 pos_max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRectFilled(pos_min + off_min, pos_max - off_min, col);
}

// (Referring to ImGui::IsRectVisible() and ImGui::GetItemRectMin().)
inline bool imgui_ItemFullyVisible() { //
    return GImGui->CurrentWindow->ClipRect.Contains(GImGui->LastItemData.Rect);
}

inline void imgui_ItemTooltip(const std::invocable<> auto& desc) {
    if (GImGui->CurrentWindow->SkipItems) {
        return;
    }
    if (ImGui::BeginItemTooltip()) {
        // The same as the one in `HelpMarker` in "imgui_demo.cpp".
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        desc();
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

inline void imgui_ItemTooltip(std::string_view desc) {
    imgui_ItemTooltip([desc] { ImGui::TextUnformatted(desc.data(), desc.data() + desc.size()); });
}

inline bool imgui_ItemClickable(ImGuiMouseButton_ mouse_button = ImGuiMouseButton_Right) {
    if (!ImGui::IsItemHovered()) {
        return false;
    }
    const bool clicked = ImGui::IsMouseClicked(mouse_button);
    const ImU32 col = clicked ? IM_COL32_WHITE : IM_COL32_GREY(128, 255);
    const ImVec2 pos_min = ImGui::GetItemRectMin();
    const ImVec2 pos_max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine({pos_min.x, pos_max.y - 1}, {pos_max.x, pos_max.y - 1}, col);
    return clicked;
}

// Unlike ImGui::Text(Wrapped/...), these functions take unformatted string as the argument.
inline void imgui_Str(std::string_view str) { //
    ImGui::TextUnformatted(str.data(), str.data() + str.size());
}

inline void imgui_StrWrapped(std::string_view str, float min_len) {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::max(min_len, ImGui::GetContentRegionAvail().x));
    imgui_Str(str);
    ImGui::PopTextWrapPos();
}

inline void imgui_StrColored(const ImVec4& col, std::string_view str) {
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    imgui_Str(str);
    ImGui::PopStyleColor();
}

inline void imgui_StrDisabled(std::string_view str) {
    imgui_StrColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), str);
}

// TODO: the name is outdated now...
// Using std::string as `SetClipboardText` requires C-style string.
inline void imgui_StrCopyable(const std::string& str, void (*str_func)(std::string_view),
                              void (*copy_func)(const std::string&),
                              ImGuiMouseButton_ mouse_button = ImGuiMouseButton_Right) {
    str_func(str);
    if (imgui_ItemClickable(mouse_button)) {
        copy_func(str);
    }
}

// Similar to `HelpMarker` in "imgui_demo.cpp".
inline void imgui_StrTooltip(std::string_view str, const std::invocable<> auto& desc) {
    imgui_StrDisabled(str);
    ::imgui_ItemTooltip(desc);
}

inline void imgui_StrTooltip(std::string_view str, std::string_view desc) { //
    imgui_StrTooltip(str, [desc] { imgui_Str(desc); });
}

struct [[nodiscard]] imgui_Window {
    imgui_Window(const imgui_Window&) = delete;
    imgui_Window& operator=(const imgui_Window&) = delete;

    const bool visible;
    explicit imgui_Window(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, p_open, flags)) {}
    ~imgui_Window() {
        ImGui::End(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

struct [[nodiscard]] imgui_ChildWindow {
    imgui_ChildWindow(const imgui_ChildWindow&) = delete;
    imgui_ChildWindow& operator=(const imgui_ChildWindow&) = delete;

    const bool visible;
    explicit imgui_ChildWindow(const char* name, const ImVec2& size = {}, ImGuiChildFlags child_flags = {},
                               ImGuiWindowFlags window_flags = {})
        : visible(ImGui::BeginChild(name, size, child_flags, window_flags)) {}
    ~imgui_ChildWindow() {
        ImGui::EndChild(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

// TODO: are there public ways to do this?
inline bool imgui_TestItemFlag(ImGuiItemFlags_ flag) { //
    return (GImGui->CurrentItemFlags & flag) != 0;
}

inline bool imgui_IsWindowHoverable(ImGuiHoveredFlags flags = 0) { //
    return ImGui::IsWindowContentHoverable(ImGui::GetCurrentWindow(), flags);
}

inline float imgui_ContentRegionMaxAbsX() { return ImGui::GetContentRegionMaxAbs().x; }

inline float imgui_ItemSpacingX() { return ImGui::GetStyle().ItemSpacing.x; }

inline float imgui_ItemInnerSpacingX() { return ImGui::GetStyle().ItemInnerSpacing.x; }

inline bool imgui_MouseScrolling() { return ImGui::GetIO().MouseWheel != 0; }

inline bool imgui_MouseScrollingDown() { return ImGui::GetIO().MouseWheel < 0; }

inline bool imgui_MouseScrollingUp() { return ImGui::GetIO().MouseWheel > 0; }

// (Referring to ImGui::InputScalar.)
inline bool imgui_StepSliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d") {
    if (GImGui->CurrentWindow->SkipItems) {
        return false;
    }

    int v2 = *v;

    const float r = ImGui::GetFrameHeight();
    const float s = imgui_ItemInnerSpacingX();
    ImGui::BeginGroup();
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - 2 * (r + s)));
    ImGui::SliderInt("", &v2, v_min, v_max, format, ImGuiSliderFlags_NoInput);
    ImGui::PushButtonRepeat(true);
    ImGui::SameLine(0, s);
    // (`InputScalar` makes .FramePadding.x = y for these buttons, not added here.)
    if (ImGui::Button("-", ImVec2(r, r))) {
        --v2;
    }
    ImGui::SameLine(0, s);
    if (ImGui::Button("+", ImVec2(r, r))) {
        ++v2;
    }
    ImGui::PopButtonRepeat();
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    if (label != label_end) {
        ImGui::SameLine(0, s);
        imgui_Str(std::string_view(label, label_end));
    }
    ImGui::PopID();
    ImGui::EndGroup();

    v2 = std::clamp(v2, v_min, v_max);
    const bool changed = *v != v2;
    *v = v2;
    return changed;
}
