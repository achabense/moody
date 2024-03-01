#include <filesystem>
#include <fstream>
#include <sstream>

#include "common.hpp"

// TODO: whether to consider write access (file-editing etc)?
// TODO: support saving into file? (without relying on the clipboard)

#define ENABLE_MULTILINE_COPYING

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

static std::vector<std::pair<pathT, std::string>> special_paths;
bool file_nav_add_special_path(const char* u8path, const char* title) {
    std::error_code ec{};
    pathT p = std::filesystem::canonical(cpp17_u8path(u8path), ec);
    if (!ec) {
        special_paths.emplace_back(std::move(p), title);
        return true;
    }
    return false;
}

class file_nav {
    using entryT = std::filesystem::directory_entry;

    char buf_path[200]{};
    char buf_filter[20]{};

    bool m_valid = false; // The last call to `collect(current, ...)` is successful.
    pathT m_current;      // Canonical path.
    std::vector<entryT> m_dirs, m_files;

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
    void set_current(const pathT& path) {
        try {
            pathT p = std::filesystem::canonical(path);
            std::vector<entryT> p_dirs, p_files;
            collect(p, p_dirs, p_files);

            m_valid = true;
            m_current.swap(p);
            m_dirs.swap(p_dirs);
            m_files.swap(p_files);
        } catch (const std::exception& /* not used; the encoding is a mystery */) {
            messenger::add_msg("Cannot open folder:\n{}", cpp17_u8string(path));
        }
    }

public:
    void refresh_if_valid() {
        if (m_valid) {
            try {
                collect(m_current, m_dirs, m_files);
            } catch (const std::exception& /* not used; the encoding is a mystery */) {
                messenger::add_msg("Cannot refresh folder:\n{}", cpp17_u8string(m_current));
                m_valid = false;
                m_dirs.clear();
                m_files.clear();
            }
        }
    }

    file_nav(const pathT& path = std::filesystem::current_path()) {
        m_valid = false;
        set_current(path);
    }

    [[nodiscard]] std::optional<pathT> display() {
        std::optional<pathT> target = std::nullopt;

        if (m_valid) {
            imgui_StrCopyable(cpp17_u8string(m_current), imgui_Str);
        } else {
            assert(m_dirs.empty() && m_files.empty());
            imgui_StrDisabled("(Invalid) " + cpp17_u8string(m_current));
        }

        ImGui::Separator();

        if (ImGui::BeginTable("##Table", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                // TODO: better hint...
                if (ImGui::InputTextWithHint("Path", "-> enter", buf_path, std::size(buf_path),
                                             ImGuiInputTextFlags_EnterReturnsTrue)) {
                    // TODO: is the usage of / correct?
                    // TODO: is this still ok when !valid?
                    set_current(m_current / cpp17_u8path(buf_path));
                    buf_path[0] = '\0';
                }
                for (const auto& [path, title] : special_paths) {
                    if (ImGui::MenuItem(title.c_str())) {
                        set_current(path);
                    }
                }
                if (ImGui::MenuItem("..")) {
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
            {
                ImGui::InputText("Filter", buf_filter, std::size(buf_filter));
                ImGui::Separator();
                if (auto child = imgui_ChildWindow("Files")) {
                    bool has = false;
                    for (const entryT& entry : m_files) {
                        const std::string str = cpp17_u8string(entry.path().filename());
                        if (str.find(buf_filter) != str.npos) {
                            has = true;
                            if (ImGui::Selectable(str.c_str())) {
                                target = entry.path();
                            }
                        }
                    }
                    if (!has) {
                        imgui_StrDisabled("None");
                    }
                }
            }
            ImGui::EndTable();
        }

        return target;
    }
};

static std::optional<std::string> load_binary(const pathT& path, int max_size) {
    std::error_code ec{};
    const auto size = std::filesystem::file_size(path, ec);
    if (size != -1 && size < max_size) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (file) {
            std::string data(size, '\0');
            file.read(data.data(), size);
            if (file && file.gcount() == size) {
                return data;
            }
        }
    }

    if (size > max_size) {
        messenger::add_msg("File too large: {} > {} (bytes)\n{}", size, max_size, cpp17_u8string(path));
    } else {
        messenger::add_msg("Cannot load file:\n{}", cpp17_u8string(path));
    }
    return {};
}

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
    int m_pos = 0; // Current pos (m_rules[m_pos]), valid if !m_rules.empty().

    bool should_rewind = true;

#ifdef ENABLE_MULTILINE_COPYING
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
#endif // ENABLE_MULTILINE_COPYING

public:
    textT() {}
    textT(std::string_view str) { append(str); }

    bool has_rule() const { return !m_rules.empty(); }

    void clear() {
        m_lines.clear();
        m_rules.clear();
        m_pos = 0;
        should_rewind = true;

#ifdef ENABLE_MULTILINE_COPYING
        m_sel.reset();
#endif
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

    void display(std::optional<legacy::extrT::valT>& out) {
        bool ret = false;
        const int total = m_rules.size();

        if (total != 0) {
            if (ImGui::Button("Focus")) {
                ret = true;
            }
            ImGui::SameLine();
            iter_group(
                "<|", "prev", "next", "|>",                                  //
                [&] { ret = true, m_pos = 0; },                              //
                [&] { ret = true, m_pos = std::max(0, m_pos - 1); },         //
                [&] { ret = true, m_pos = std::min(total - 1, m_pos + 1); }, //
                [&] { ret = true, m_pos = total - 1; });
            ImGui::SameLine();
            ImGui::Text("Total:%d At:%d", total, m_pos + 1);

            // TODO: (temp) added back as this is very convenient; whether to require the window to be focused?
            // (required if there are going to be multiple opened files in the gui)...
            if (!ret) {
                if (imgui_KeyPressed(ImGuiKey_UpArrow, true)) {
                    ret = true, m_pos = std::max(0, m_pos - 1);
                } else if (imgui_KeyPressed(ImGuiKey_DownArrow, true)) {
                    ret = true, m_pos = std::min(total - 1, m_pos + 1);
                }
            }
        } else {
            ImGui::Text("No rules");
        }

        ImGui::Separator();

        const bool locate = ret;

        if (std::exchange(should_rewind, false)) {
            ImGui::SetNextWindowScroll({0, 0});
            assert(m_pos == 0);
            if (total != 0) {
                ret = true; // TODO: whether to set ret in this case?
            }
        }

#ifdef ENABLE_MULTILINE_COPYING
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && m_sel) {
            std::string str;
            const auto [min, max] = m_sel->minmax();
            for (int i = min; i <= max; ++i) {
                if (i != min) {
                    str += '\n';
                }
                str += m_lines[i].text;
            }
            messenger::add_msg("{}", str);
            m_sel.reset();
        }
#endif // ENABLE_MULTILINE_COPYING

        if (auto child = imgui_ChildWindow("Child")) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int l = 1; const auto& [text, id] : m_lines) {
                ImGui::TextDisabled("%2d ", l++);
                ImGui::SameLine();
                imgui_StrWrapped(text);

#ifdef ENABLE_MULTILINE_COPYING
                const int this_l = l - 2;
                if (ImGui::IsItemHovered()) {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        m_sel = {this_l, this_l};
                    } else if (m_sel && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        m_sel->end = this_l;
                    }
                }
                if (m_sel && m_sel->contains(this_l)) {
                    imgui_ItemRectFilled(IM_COL32(255, 255, 255, 90));
                    continue;
                }
#endif // ENABLE_MULTILINE_COPYING

                if (id.has_value()) {
                    const bool is_mold = m_rules[*id].lock.has_value();

                    if (id == m_pos) {
                        imgui_ItemRectFilled(IM_COL32(is_mold ? 196 : 0, 255, 0, 60));
                        if (!ImGui::IsItemVisible() && locate) {
                            ImGui::SetScrollHereY();
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        imgui_ItemRectFilled(IM_COL32(is_mold ? 196 : 0, 255, 0, 30));
                        if (ImGui::IsItemClicked()) {
                            m_pos = *id;
                            ret = true;
                        }
                    }
                }
            }
            ImGui::PopStyleVar();
        }

        if (ret) {
            assert(m_pos >= 0 && m_pos < total);
            out = m_rules[m_pos];
        }
    }
};

// TODO: show the last opened file in file-nav?
// TODO: support opening multiple files?
static void load_rule_from_file(std::optional<legacy::extrT::valT>& out) {
    static file_nav nav;

    struct fileT {
        pathT path;
        textT text;
    };
    static std::optional<fileT> file;

    bool close = false, load = false;
    if (!file) {
        if (ImGui::SmallButton("Refresh")) {
            nav.refresh_if_valid();
        }

        if (auto sel = nav.display()) {
            file.emplace(*sel);
            load = true;
        }
    } else {
        close = ImGui::SmallButton("Close");
        ImGui::SameLine();
        load = ImGui::SmallButton("Reload");
        imgui_StrCopyable(cpp17_u8string(file->path), imgui_Str);

        file->text.display(out);
    }

    if (close) {
        assert(file);
        file.reset();
    } else if (load) {
        assert(file);
        file->text.clear();
        if (auto data = load_binary(file->path, 100'000)) {
            file->text.append(*data);
            if (!file->text.has_rule()) {
                file.reset();
                messenger::add_msg("Found no rules");
            }
        } else {
            file.reset();
            // load_binary has done messenger::add_msg.
        }
    }
}

static void load_rule_from_memory(std::optional<legacy::extrT::valT>& out) {
    static textT text("TODO");

    text.display(out);
}

std::optional<legacy::extrT::valT> load_rule() {
    std::optional<legacy::extrT::valT> out = std::nullopt;

    if (ImGui::BeginTabBar("Tabs")) {
        if (ImGui::BeginTabItem("Open file")) {
            load_rule_from_file(out);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Documents")) {
            load_rule_from_memory(out);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    return out;
}
