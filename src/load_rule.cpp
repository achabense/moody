#include <filesystem>
#include <fstream>
#include <sstream>

#include "common.hpp"

// TODO: whether to support write access (file-editing)?
// Or at least support saving rules into file (without relying on the clipboard)?

using pathT = std::filesystem::path;

// (wontfix) After wasting so much time, I'd rather afford the extra copy than bothering with "more efficient"
// implementations any more.

static std::string cpp17_u8string(const pathT& path) {
    const auto u8string = path.u8string();
    return std::string(u8string.begin(), u8string.end());
}

// As to why not using `filesystem::u8path`:
// There is no standard way to shut the compiler up for a [[deprecated]] warning.
// As to making an `std::u8string` first, see:
// https://stackoverflow.com/questions/57603013/how-to-safely-convert-const-char-to-const-char8-t-in-c20
// In short, in C++20 it's impossible to get `char8_t*` from `char*` without copy and in a well-defined way.
static pathT cpp17_u8path(const std::string_view path) { //
    return pathT(std::u8string(path.begin(), path.end()));
}

static void display_path(const pathT& p) {
    const std::string str = cpp17_u8string(p);
    imgui_StrCopyable(str, imgui_Str);
    // (Referring to ImGui::IsRectVisible() and ImGui::GetItemRectMin().)
    const bool fully_visible = GImGui->CurrentWindow->ClipRect.Contains(GImGui->LastItemData.Rect);
    if (!fully_visible) {
        imgui_ItemTooltip(str);
    }
}

static std::vector<std::pair<pathT, std::string>> special_paths;
bool file_nav_add_special_path(const char* u8path, const char* title) {
    std::error_code ec{};
    pathT p = std::filesystem::canonical(cpp17_u8path(u8path), ec);
    if (!ec) {
        special_paths.emplace_back(std::move(p), title);
        return true;
    } else {
        messenger::add_msg("Cannot add path:\n{}", u8path);
        return false;
    }
}

// TODO: better layout...
// TODO: record recently-opened folders...
class file_nav {
    using entryT = std::filesystem::directory_entry;

    char buf_path[200]{};
    char buf_filter[20]{};

    pathT m_current{}; // Canonical path; empty() <-> invalid.
    std::vector<entryT> m_dirs{}, m_files{};

    static void collect(const pathT& path, std::vector<entryT>& dirs, std::vector<entryT>& files) {
        dirs.clear();
        files.clear();
        for (const auto& entry :
             std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied)) {
            const auto status = entry.status();
            if (is_directory(status)) {
                dirs.emplace_back(entry);
            }
            if (is_regular_file(status)) {
                files.emplace_back(entry);
            }
        }
    }

    // (`path` may come from `entryT.path()` in `m_dirs`.)
    bool set_current(const pathT& path) {
        try {
            pathT p = std::filesystem::canonical(path);
            std::vector<entryT> p_dirs, p_files;
            collect(p, p_dirs, p_files);

            m_current.swap(p);
            m_dirs.swap(p_dirs);
            m_files.swap(p_files);
            return true;
        } catch (const std::exception& /* not used; the encoding is a mystery */) {
            messenger::add_msg("Cannot open folder:\n{}", cpp17_u8string(path));
        }
        return false;
    }

public:
    void refresh_if_valid() {
        if (!m_current.empty()) {
            try {
                collect(m_current, m_dirs, m_files);
            } catch (const std::exception& /* not used; the encoding is a mystery */) {
                messenger::add_msg("Cannot refresh folder:\n{}", cpp17_u8string(m_current));
                m_current.clear();
                m_dirs.clear();
                m_files.clear();
            }
        }
    }

    file_nav(const pathT& path = std::filesystem::current_path()) { set_current(path); }

    void select_file(const pathT* current_file /* Optional */, std::optional<pathT>& target) {
        ImGui::SetNextItemWidth(std::min(ImGui::CalcItemWidth(), (float)item_width));
        ImGui::InputText("Filter", buf_filter, std::size(buf_filter));
        ImGui::Separator();
        if (auto child = imgui_ChildWindow("Files")) {
            bool has = false;
            for (const entryT& entry : m_files) {
                const std::string str = cpp17_u8string(entry.path().filename());
                if (str.find(buf_filter) != str.npos) {
                    has = true;
                    const bool selected = current_file && *current_file == entry.path();
                    if (ImGui::Selectable(str.c_str(), selected)) {
                        target = entry.path();
                    }
                    if (selected && ImGui::IsWindowAppearing()) {
                        ImGui::SetScrollHereY();
                    }
                }
            }
            if (!has) {
                imgui_StrDisabled("None");
            }
        }
    }

    // Return one of `entryT.path()` in `m_files`.
    std::optional<pathT> display() {
        std::optional<pathT> target = std::nullopt;

        if (!m_current.empty()) {
            display_path(m_current);
        } else {
            assert(m_dirs.empty() && m_files.empty());
            imgui_StrDisabled("N/A");
        }

        ImGui::Separator();

        if (ImGui::BeginTable("##Table", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                ImGui::SetNextItemWidth(std::min(ImGui::CalcItemWidth(), (float)item_width));
                ImGui::InputTextWithHint("##Path", "Folder or file path", buf_path, std::size(buf_path));
                ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                if (ImGui::Button("Open") && buf_path[0] != '\0') {
                    std::error_code ec{};
                    const pathT p = m_current / cpp17_u8path(buf_path);
                    if (std::filesystem::is_directory(p, ec)) {
                        set_current(p);
                    } else if (std::filesystem::is_regular_file(p, ec) && set_current(p.parent_path())) {
                        // (wontfix) Inefficient, but there are a lot of uncertainties about `entryT.path()`.
                        // (Are there better ways to find such an entry? Is `entryT.path()` canonical?
                        // Does `target = canonical(p)` or `target = entryT(p).path()` work?)
                        for (const entryT& entry : m_files) {
                            if (std::filesystem::equivalent(entry.path(), p, ec)) {
                                target = entry.path();
                                break;
                            }
                        }
                    }

                    buf_path[0] = '\0';
                }
                ImGui::Separator();
                // (Using `ImGuiSelectableFlags_NoPadWithHalfSpacing` for the same visual effect as
                // those in _ChildWindow("Folders").)
                for (const auto& [path, title] : special_paths) {
                    if (ImGui::Selectable(title.c_str(), false, ImGuiSelectableFlags_NoPadWithHalfSpacing)) {
                        set_current(path);
                    }
                }
                if (ImGui::Selectable("..", false, ImGuiSelectableFlags_NoPadWithHalfSpacing)) {
                    set_current(m_current.parent_path());
                }
                ImGui::Separator();
                if (auto child = imgui_ChildWindow("Folders")) {
                    if (m_dirs.empty()) {
                        imgui_StrDisabled("None");
                    }
                    const entryT* sel = nullptr;
                    for (const entryT& entry : m_dirs) {
                        // TODO: cache str?
                        const std::string str = cpp17_u8string(entry.path().filename());
                        if (ImGui::Selectable(str.c_str())) {
                            sel = &entry;
                        }
                    }
                    if (sel) {
                        set_current(sel->path());
                    }
                }
            }
            ImGui::TableNextColumn();
            select_file(nullptr, target);
            ImGui::EndTable();
        }

        return target;
    }
};

// It is easy to locate all rules in the text via `extract_MAP_str`.
// However there are no easy ways to locate or highlight (only) the rule across the lines.
// See: https://github.com/ocornut/imgui/issues/2313
// So, currently `textT` is line-based, and only recognizes the first rule for each line, and will
// highlight the whole line if the line contains a rule.
class textT {
    struct lineT {
        std::string text;
        std::optional<int> id = std::nullopt;
    };

    std::vector<lineT> m_lines{};
    std::vector<legacy::extrT::valT> m_rules{};
    std::optional<int> m_pos = std::nullopt; // `display` returned m_rules[*m_pos] last time.

    struct selT {
        int beg = 0, end = 0;
        bool contains(int l) const {
            if (beg < end) {
                return beg <= l && l <= end;
            } else {
                return end <= l && l <= beg;
            }
        }
        std::pair<int, int> minmax() const { return std::minmax(beg, end); }
    };
    std::optional<selT> m_sel = std::nullopt;

public:
    textT() {}
    textT(std::string_view str) { append(str); }

    bool has_rule() const { return !m_rules.empty(); }

    void clear() {
        m_lines.clear();
        m_rules.clear();
        m_pos.reset();
        m_sel.reset();
    }

    // `str` is assumed to be utf8-encoded.
    // (If not, the rules are still likely extractable.)
    void append(std::string_view str) {
        // (wontfix) It's not worthwhile to bother removing this copy.
        std::istringstream is{std::string(str)};

        std::string text;
        while (std::getline(is, text)) {
            const auto val = legacy::extract_MAP_str(text).val;
            lineT& line = m_lines.emplace_back(std::move(text));
            if (val.has_value()) {
                m_rules.push_back(*val);
                line.id = m_rules.size() - 1;
            }
        }
    }

    // (Workaround: using `rewind` tag to reset the scroll after opening new files.)
    void display(std::optional<legacy::extrT::valT>& out, bool rewind = false) {
        if (m_sel && ImGui::IsMouseReleased(ImGuiMouseButton_Right) /* From anywhere */) {
            std::string str;
            const auto [min, max] = m_sel->minmax();
            for (int i = min; i <= max; ++i) {
                if (i != min) {
                    str += '\n';
                }
                str += m_lines[i].text;
            }

            // The problem (see the "Workaround" part below) can happen as long as wrap-pos is enabled.
            // For example, push-wrap-pos -> ImGui::Text will result in the same issue.
            ImGui::SetClipboardText(str.c_str());
            m_sel.reset();
#if 0
            // TODO: whether to silently call `ImGui::SetClipboardText`?
            messenger::add_msg(str);
            m_sel.reset();
            // (wontfix) The message popup will appear at next frame, so it's still possible that an extra click
            // is made (and begin selection) at this frame. As a result, when entering the popup the right-button
            // may still be held down, and if released it will bring anther string to the popup.
#endif
        }

        // TODO: whether to share the same config across the windows?
        static bool preview_mode = true;
        static preview_rule::configT config(preview_rule::configT::_220_160);

        bool ret = false;
        const int total = m_rules.size();

        if (total != 0) {
            ImGui::Checkbox("Preview mode", &preview_mode);
            ImGui::SameLine();
            config.set("Settings");

            std::optional<int> n_pos = std::nullopt;
            if (ImGui::Button("Focus")) {
                n_pos = m_pos.value_or(0);
            }
            ImGui::SameLine();
            sequence::seq(
                "<|", "Prev", "Next", "|>",                                   //
                [&] { n_pos = 0; },                                           //
                [&] { n_pos = std::max(0, m_pos.value_or(-1) - 1); },         //
                [&] { n_pos = std::min(total - 1, m_pos.value_or(-1) + 1); }, //
                [&] { n_pos = total - 1; });
            ImGui::SameLine();
            if (m_pos.has_value()) {
                ImGui::Text("Total:%d At:%d", total, *m_pos + 1);
            } else {
                ImGui::Text("Total:%d At:N/A", total);
            }

            if (!m_sel && n_pos) {
                assert(*n_pos >= 0 && *n_pos < total);
                m_pos = *n_pos;
                ret = true;
            }
        } else {
            ImGui::Text("(No rules)");
        }

        const bool locate = ret;
        ImGui::Separator();

        if (rewind) {
            ImGui::SetNextWindowScroll({0, 0});
        }
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 24, 24, 255));
        if (auto child = imgui_ChildWindow("Child")) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int l = 0; const auto& [text, id] : m_lines) {
                const int this_l = l++;
                ImGui::TextDisabled("%2d ", this_l + 1);
                ImGui::SameLine();
                if (preview_mode && id.has_value()) {
                    ImGui::BeginGroup();
                }
                {
                    // (Workaround: `ImGui::TextWrapped` cannot smartly render long single-lines.)
                    // (Related: https://github.com/ocornut/imgui/issues/5720)
                    if (text.size() < 2000) {
                        imgui_StrWrapped(text, item_width);
                    } else {
                        ImGui::BeginGroup();
                        const char* begin = text.data();
                        const char* const end = text.data() + text.size();
                        while (end - begin > 1000) {
                            const char* seg_end = begin + 1000; // seg_end < end
                            // Find a meaningful (utf8) code-point end nearby.
                            for (int i = 0; i < 4; ++i) {
                                if ((*seg_end & 0b11'000000) != 0b10'000000) {
                                    break;
                                }
                                --seg_end;
                            }
                            imgui_StrWrapped(std::string_view(begin, seg_end), item_width);
                            begin = seg_end;
                        }
                        if (begin != end) {
                            imgui_StrWrapped(std::string_view(begin, end), item_width);
                        }
                        ImGui::EndGroup();
                    }
                }

                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    m_sel = {this_l, this_l};
                } else if (m_sel && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    m_sel->end = this_l;
                }
                if (m_sel && m_sel->contains(this_l)) {
                    imgui_ItemRectFilled(IM_COL32(255, 255, 255, 90));
                }

                if (id.has_value()) {
                    const bool has_lock = m_rules[*id].lock.has_value();

                    if (*id == m_pos) {
                        imgui_ItemRectFilled(IM_COL32(has_lock ? 196 : 0, 255, 0, 60));
                        if (locate && !ImGui::IsItemVisible()) {
                            ImGui::SetScrollHereY();
                        }
                    }
                    if (!m_sel && ImGui::IsItemHovered()) {
                        imgui_ItemRectFilled(IM_COL32(has_lock ? 196 : 0, 255, 0, 30));
                        if (ImGui::IsItemClicked()) {
                            m_pos = *id;
                            ret = true;
                        }
                    }

                    if (preview_mode) {
                        imgui_StrDisabled("-: ");
                        ImGui::SameLine();
                        if (*id != 0 && m_rules[*id].rule == m_rules[*id - 1].rule) {
                            imgui_StrDisabled("The same as the last rule.");
                        } else {
                            preview_rule::preview(*id, config, m_rules[*id].rule, true);
                        }
                        ImGui::EndGroup();
                    }
                }
            }
            ImGui::PopStyleVar();
        }
        ImGui::PopStyleColor();

        if (ret) {
            assert(m_pos && *m_pos >= 0 && *m_pos < total);
            // (Relying on `sequence::seq` to be in the same level in the id-stack.)
            sequence::bind_to(ImGui::GetID("Next"));
            out = m_rules[*m_pos];
        }
    }
};

static const int max_size = 1024 * 256;

// For error message.
static std::string too_long(uintmax_t size, int max_size) {
    const bool use_mb = size >= 1024 * 1024;
    return std::format("{:.2f}{} > {:.2f}KB", size / (use_mb ? (1024 * 1024.0) : 1024.0), use_mb ? "MB" : "KB",
                       max_size / 1024.0);
}

// TODO: support opening multiple files?
// TODO: add a mode to avoid opening files without rules?
static void load_rule_from_file(std::optional<legacy::extrT::valT>& out) {
    static file_nav nav;

    struct fileT {
        pathT path;
        textT text;
    };
    static bool rewind = false;
    static std::optional<fileT> file;

    std::optional<pathT> sel = std::nullopt;
    if (!file) {
        if (ImGui::SmallButton("Refresh")) {
            nav.refresh_if_valid();
        }

        sel = nav.display();
    } else {
        const bool close = ImGui::SmallButton("Close");
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload")) {
            sel = file->path;
        }
        ImGui::SameLine();
        ImGui::SmallButton("...");
        // `SetNextWindowSize` will only affect the `select_file` window, even if it is not opened.
        ImGui::SetNextWindowSize({0, 200}, ImGuiCond_Always);
        if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
            nav.select_file(&file->path, sel);
            ImGui::EndPopup();
        }
        display_path(file->path);

        ImGui::Separator();
        file->text.display(out, std::exchange(rewind, false));
        if (close) {
            file.reset();
        }
    }

    if (sel) {
        try {
            fileT temp{.path = std::move(*sel)};
            temp.text.append([](const pathT& path) -> std::string {
                std::error_code ec{};
                const auto size = std::filesystem::file_size(path, ec);
                if (!ec && size <= max_size) {
                    std::ifstream file(path, std::ios::in | std::ios::binary);
                    if (file) {
                        std::string data(size, '\0');
                        file.read(data.data(), size);
                        if (file && file.gcount() == size) {
                            return data;
                        }
                    }
                }

                if (!ec && size > max_size) {
                    messenger::add_msg("File too large: {}\n{}", too_long(size, max_size), cpp17_u8string(path));
                } else {
                    messenger::add_msg("Failed to load file:\n{}", cpp17_u8string(path));
                }
                throw 0; // 0 ->
            }(temp.path));
            file = std::move(temp);
            rewind = true;
        } catch (...) {
            ; // <- 0
        }
    }
}

static void load_rule_from_clipboard(std::optional<legacy::extrT::valT>& out) {
    static textT text;
    static bool rewind = false;
    if (ImGui::SmallButton("Read clipboard")) {
        if (const char* str = ImGui::GetClipboardText()) {
            std::string_view str_view(str);
            if (str_view.size() > max_size) {
                messenger::add_msg("Text too long: {}", too_long(str_view.size(), max_size));
            } else {
                text.clear();
                text.append(str_view);
                rewind = true;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        text.clear();
    }

    ImGui::Separator();
    text.display(out, std::exchange(rewind, false));
}

// Must keep in sync with "docs.cpp".
struct docT {
    const char* title;
    const char* text;
};

extern const docT docs[];
extern const int doc_size;

static void load_rule_from_memory(std::optional<legacy::extrT::valT>& out) {
    static textT text;
    static bool rewind = false;
    static std::optional<int> doc_id = std::nullopt;
    static auto select = []() {
        for (int i = 0; i < doc_size; ++i) {
            if (ImGui::Selectable(docs[i].title, doc_id == i, ImGuiSelectableFlags_DontClosePopups) && doc_id != i) {
                text.clear();
                text.append(docs[i].text);
                rewind = true;
                doc_id = i;
            }
        }
    };

    if (!doc_id) {
        select();
    } else {
        const bool close = ImGui::SmallButton("Close");
        ImGui::SameLine();
        ImGui::SmallButton("...");
        if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
            select();
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        imgui_Str(docs[*doc_id].title);

        ImGui::Separator();
        text.display(out, std::exchange(rewind, false));
        if (close) {
            text.clear();
            doc_id.reset();
        }
    }
}

// Preserving the `load_rule_from_xx` functions to allow for integration.
std::optional<legacy::extrT::valT> load_file() {
    std::optional<legacy::extrT::valT> out = std::nullopt;
    load_rule_from_file(out);
    return out;
}

std::optional<legacy::extrT::valT> load_clipboard() {
    std::optional<legacy::extrT::valT> out = std::nullopt;
    load_rule_from_clipboard(out);
    return out;
}

std::optional<legacy::extrT::valT> load_doc() {
    std::optional<legacy::extrT::valT> out = std::nullopt;
    load_rule_from_memory(out);
    return out;
}
