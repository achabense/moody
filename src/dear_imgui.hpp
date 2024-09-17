#pragma once

#include <algorithm>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

// Follows `IM_COL32_XX`; note that `constexpr` cannot guarantee `fn(100, 255)` be
// calculated at compile time, especially in debug mode.
consteval ImU32 IM_COL32_GREY(ImU8 v, ImU8 alpha) { return IM_COL32(v, v, v, alpha); }

inline ImRect imgui_GetItemRect() { return GImGui->LastItemData.Rect; }

inline ImRect imgui_GetWindowRect() {
    const ImVec2 window_min = ImGui::GetWindowPos(), window_max = window_min + ImGui::GetWindowSize();
    return {window_min, window_max};
}

// These names are somewhat misleading after the introduction of `imgui_GetItemRect`...
inline void imgui_ItemRect(ImU32 col, ImVec2 off_min = {0, 0}) {
    const auto [pos_min, pos_max] = GImGui->LastItemData.Rect;
    ImGui::GetWindowDrawList()->AddRect(pos_min + off_min, pos_max - off_min, col);
}

inline void imgui_ItemRectFilled(ImU32 col, ImVec2 off_min = {0, 0}) {
    const auto [pos_min, pos_max] = GImGui->LastItemData.Rect;
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

template <class T>
inline bool imgui_RadioButton(const char* label, T* v, std::type_identity_t<T> c) {
    assert(v != nullptr);
    if (ImGui::RadioButton(label, bool(*v == c))) {
        *v = c;
        return true;
    }
    return false;
}

// TODO: are there public ways to do this?
inline bool imgui_TestItemFlag(ImGuiItemFlags_ flag) { //
    return (GImGui->CurrentItemFlags & flag) != 0;
}

inline bool imgui_IsWindowHoverable(ImGuiHoveredFlags flags = 0) { //
    return ImGui::IsWindowContentHoverable(ImGui::GetCurrentWindowRead(), flags);
}

inline float imgui_ContentRegionMaxAbsX() { return ImGui::GetContentRegionMaxAbs().x; }

inline float imgui_ItemSpacingX() { return ImGui::GetStyle().ItemSpacing.x; }

inline float imgui_ItemInnerSpacingX() { return ImGui::GetStyle().ItemInnerSpacing.x; }

inline bool imgui_MouseScrolling() { return ImGui::GetIO().MouseWheel != 0; }

inline bool imgui_MouseScrollingDown() { return ImGui::GetIO().MouseWheel < 0; }

inline bool imgui_MouseScrollingUp() { return ImGui::GetIO().MouseWheel > 0; }
