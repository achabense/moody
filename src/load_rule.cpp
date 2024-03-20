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
    void set_current(const pathT& path) {
        try {
            pathT p = std::filesystem::canonical(path);
            std::vector<entryT> p_dirs, p_files;
            collect(p, p_dirs, p_files);

            m_current.swap(p);
            m_dirs.swap(p_dirs);
            m_files.swap(p_files);
        } catch (const std::exception& /* not used; the encoding is a mystery */) {
            messenger::add_msg("Cannot open folder:\n{}", cpp17_u8string(path));
        }
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

    std::optional<pathT> display() {
        std::optional<pathT> target = std::nullopt;

        if (!m_current.empty()) {
            imgui_StrCopyable(cpp17_u8string(m_current), imgui_Str);
        } else {
            assert(m_dirs.empty() && m_files.empty());
            imgui_StrDisabled("N/A");
        }

        ImGui::Separator();

        if (ImGui::BeginTable("##Table", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                ImGui::InputTextWithHint("##Path", "Path", buf_path, std::size(buf_path));
                ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                if (ImGui::Button("Open") && buf_path[0] != '\0') {
                    const pathT p = m_current / cpp17_u8path(buf_path);
                    std::error_code ec{};
                    if (std::filesystem::is_regular_file(p, ec)) {
                        target = p;
                    } else if (std::filesystem::is_directory(p, ec)) {
                        set_current(p);
                    } else {
                        messenger::add_msg("Invalid path:\n{}", buf_path);
                    }
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
            // !!TODO: reconsider whether to silently call ImGui::SetClipboardText...
            messenger::add_msg(str);
            m_sel.reset();
        }

        bool ret = false;
        const int total = m_rules.size();

        if (total != 0) {
            std::optional<int> n_pos = std::nullopt;
            if (ImGui::Button("Focus")) {
                n_pos = m_pos.value_or(0);
            }
            ImGui::SameLine();
            sequence::seq(
                "<|", "prev", "next", "|>",                                   //
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
        if (auto child = imgui_ChildWindow("Child")) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int l = 1; const auto& [text, id] : m_lines) {
                ImGui::TextDisabled("%2d ", l++);
                ImGui::SameLine();
                imgui_StrWrapped(text);

                const int this_l = l - 2;
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
                }
            }
            ImGui::PopStyleVar();
        }

        if (ret) {
            assert(m_pos && *m_pos >= 0 && *m_pos < total);
            // (Relying on `sequence::seq` to be in the same level in the id-stack.)
            sequence::bind_to(ImGui::GetID("next"));
            out = m_rules[*m_pos];
        }
    }
};

static std::string load_binary(const pathT& path, int max_size) {
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

    throw std::runtime_error(
        size > max_size ? std::format("File too large: {} > {} (bytes)\n{}", size, max_size, cpp17_u8string(path))
                        : std::format("Failed to load file:\n{}", cpp17_u8string(path)));
}

static const int max_length = 100'000;

// TODO: support opening multiple files?
// TODO: show recently opened files/folders?
static void load_rule_from_file(std::optional<legacy::extrT::valT>& out) {
    static file_nav nav;

    struct fileT {
        pathT path;
        textT text;
    };
    static bool rewind = false;
    static std::optional<fileT> file;

    bool load = false;
    if (!file) {
        if (ImGui::SmallButton("Refresh")) {
            nav.refresh_if_valid();
        }

        if (auto sel = nav.display()) {
            file.emplace(*sel);
            rewind = true;
            load = true;
        }
    } else {
        const bool close = ImGui::SmallButton("Close");
        ImGui::SameLine();
        load = ImGui::SmallButton("Reload");
        imgui_StrCopyable(cpp17_u8string(file->path), imgui_Str);

        file->text.display(out, std::exchange(rewind, false));
        if (close) {
            file.reset();
        }
    }

    if (load) {
        assert(file);
        try {
            file->text.clear();
            file->text.append(load_binary(file->path, max_length));
        } catch (const std::exception& err) {
            file.reset();
            messenger::add_msg(err.what());
        }
    }
}

static void load_rule_from_clipboard(std::optional<legacy::extrT::valT>& out) {
    static textT text;
    if (ImGui::SmallButton("Read clipboard")) {
        if (const char* str = ImGui::GetClipboardText()) {
            std::string_view str_view(str);
            if (str_view.size() > max_length) {
                messenger::add_msg("Text too long: {} > {} (bytes)", str_view.size(), max_length);
            } else {
                text.clear();
                text.append(str_view);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        text.clear();
    }

    text.display(out);
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
    static std::optional<int> doc_id = std::nullopt;
    static bool rewind = false;

    if (!doc_id) {
        for (int i = 0; i < doc_size; ++i) {
            if (ImGui::Selectable(docs[i].title)) {
                text.clear();
                text.append(docs[i].text);
                rewind = true;
                doc_id = i;
            }
        }
    } else {
        // assert(text.has_rule());

        const bool close = ImGui::SmallButton("Close");
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

// The `load_rule_from_xx` functions are static to allow for integration.
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
