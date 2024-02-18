// TODO: not used in this source file...
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <filesystem>
#include <fstream>
#include <sstream>

#include "common.hpp"

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

// TODO: recheck...
// TODO: able to create/append/open file?
class file_nav {
    using entryT = std::filesystem::directory_entry;

    char buf_path[200]{};
    char buf_filter[20]{".txt"};

    // TODO: is canonical path necessary?
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

public:
    file_nav(const pathT& path = std::filesystem::current_path()) {
        m_valid = false;
        set_current(path);
    }

    // TODO: refine...
    inline static std::vector<std::pair<pathT, std::string>> additionals;
    static bool add_special_path(const char* u8path, const char* title) { //
        std::error_code ec{};
        pathT p = std::filesystem::canonical(cpp17_u8path(u8path), ec);
        if (!ec) {
            additionals.emplace_back(std::move(p), title);
            return true;
        }
        return false;
    }

    [[nodiscard]] std::optional<pathT> display() {
        std::optional<pathT> target = std::nullopt;

        if (ImGui::SmallButton("Refresh")) {
            refresh_if_valid();
        }

        if (m_valid) {
            imgui_str(cpp17_u8string(m_current));
        } else {
            assert(m_dirs.empty() && m_files.empty());
            imgui_strdisabled("(Invalid) " + cpp17_u8string(m_current));
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
                for (const auto& [path, title] : additionals) {
                    if (ImGui::MenuItem(title.c_str())) {
                        set_current(path);
                    }
                }
                if (ImGui::MenuItem("..")) {
                    set_current(m_current.parent_path());
                }
                ImGui::Separator();
                if (auto child = imgui_childwindow("Folders")) {
                    if (m_dirs.empty()) {
                        imgui_strdisabled("None");
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
                        // TODO: does directory_entry.path always return absolute path?
                        assert(sel->path().is_absolute());
                        set_current(sel->path());
                    }
                }
            }
            ImGui::TableNextColumn();
            {
                ImGui::InputText("Filter", buf_filter, std::size(buf_filter));
                ImGui::Separator();
                if (auto child = imgui_childwindow("Files")) {
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
                        imgui_strdisabled("None");
                    }
                }
            }
            ImGui::EndTable();
        }

        return target;
    }
};

bool file_nav_add_special_path(const char* u8path, const char* title) {
    return file_nav::add_special_path(u8path, title);
}

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

// TODO: should support clipboard paste in similar ways...
// TODO: support saving into file? (without relying on the clipboard)
// TODO: support in-memory loadings. (e.g. tutorial about typical ways to find interesting rules)

// TODO: refine...
struct fileT {
    struct lineT {
        std::string text;
        std::optional<int> id = std::nullopt;
    };

    pathT m_path;
    std::vector<lineT> m_lines;
    std::vector<legacy::extrT::valT> m_rules; // TODO: whether to do compression?
    int pointing_at = 0;                      // valid if !m_rules.empty().

    bool need_reset_scroll = true;

    // TODO: It is easy to locate all rules in a text span via `extract_MAP_str`.
    // However there are no easy ways to locate or highlight (only) the rule across the lines in the gui.
    // See: https://github.com/ocornut/imgui/issues/2313
    // So, currently the program only recognizes the first rule for each line, and highlights the whole line if
    // the line contains a rule.

    // The content of the stream is assumed to be utf8-encoded.
    // (If not, the rules are still likely extractable.)
    static void append(std::vector<lineT>& lines, std::vector<legacy::extrT::valT>& rules, std::istream& is) {
        std::string text;
        while (std::getline(is, text)) {
            const auto val = legacy::extract_MAP_str(text).val;
            lineT& line = lines.emplace_back(std::move(text));
            if (val.has_value()) {
                rules.push_back(*val);
                line.id = rules.size() - 1;
            }
        }
    }

    fileT(pathT path) : m_path(std::move(path)) { reload(); }

    void reload() {
        m_lines.clear();
        m_rules.clear();
        pointing_at = 0;
        need_reset_scroll = true;

        if (auto data = load_binary(m_path, 100'000)) {
            std::istringstream ss(std::move(*data));
            append(m_lines, m_rules, ss);
        }
    }

    std::optional<legacy::extrT::valT> display() {
        bool hit = false; // TODO: rename...
        const int total = m_rules.size();

        if (total != 0) {
            ImGui::BeginGroup();
            iter_pair(
                "<|", "prev", "next", "|>",                                              //
                [&] { hit = true, pointing_at = 0; },                                    //
                [&] { hit = true, pointing_at = std::max(0, pointing_at - 1); },         //
                [&] { hit = true, pointing_at = std::min(total - 1, pointing_at + 1); }, //
                [&] { hit = true, pointing_at = total - 1; }, false); // TODO: whether to allow scrolling?
            ImGui::SameLine();
            ImGui::Text("Total:%d At:%d", total, pointing_at + 1);
            ImGui::EndGroup();
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

        if (need_reset_scroll) {
            ImGui::SetNextWindowScroll({0, 0});
            assert(pointing_at == 0);
            if (total != 0) {
                hit = true; // TODO: whether to set hit in this case?
            }
            need_reset_scroll = false;
        }
        if (auto child = imgui_childwindow("Child")) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int l = 1; const auto& [text, id] : m_lines) {
                ImGui::TextDisabled("%2d ", l++);
                ImGui::SameLine();
                imgui_strwrapped(text);

                if (id.has_value()) {
                    const bool is_mold = m_rules[*id].lock.has_value();

                    if (id == pointing_at) {
                        const ImVec2 pos_min = ImGui::GetItemRectMin();
                        const ImVec2 pos_max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max,
                                                                  IM_COL32(is_mold ? 196 : 0, 255, 0, 60));
                        if (!ImGui::IsItemVisible() && focus) {
                            ImGui::SetScrollHereY();
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        const ImVec2 pos_min = ImGui::GetItemRectMin();
                        const ImVec2 pos_max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max,
                                                                  IM_COL32(is_mold ? 196 : 0, 255, 0, 30));
                        if (ImGui::IsItemClicked()) {
                            pointing_at = *id;
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

// TODO: show the last opened file in file-nav?
// TODO: whether to support opening multiple files?
std::optional<legacy::extrT::valT> load_rule() {
    static file_nav nav;
    static std::optional<fileT> file;

    std::optional<legacy::extrT::valT> out;

    bool close = false;
    bool reload = false;
    if (file) {
        // TODO: the path (& file_nav's) should be copyable...

        if (ImGui::SmallButton("Close")) {
            close = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload")) {
            reload = true;
        }
        imgui_str(cpp17_u8string(file->m_path));

        out = file->display();
    } else {
        if (auto sel = nav.display()) {
            file.emplace(*sel);
            if (file->m_rules.empty()) {
                file.reset();
                messenger::add_msg("Found no rules");
            }
        }
    }

    if (close) {
        file.reset();
    }
    // TODO: awkward... refactor...
    if (reload) {
        assert(file);
        file->reload();
        if (file->m_rules.empty()) {
            file.reset();
            messenger::add_msg("Found no rules");
        }
    }

    return out;
}
