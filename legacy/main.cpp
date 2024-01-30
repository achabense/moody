#include "app.hpp"

// TODO: Right-click must either to open a submenu, or to toggle on/off the tooltip.
// TODO: Generalize typical behavior patterns to find new rules.

void show_target_rule(const legacy::ruleT& target, rule_recorder& recorder) {
    const std::string rule_str = to_MAP_str(target);

    ImGui::AlignTextToFramePadding();
    imgui_str("[Current rule]");
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        ImGui::SetClipboardText(rule_str.c_str());
        logger::log_temp(300ms, "Copied");
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste")) {
        // TODO: redesign... (especially, should not replace directly?)
        if (const char* text = ImGui::GetClipboardText()) {
            auto result = legacy::extract_rules(text);
            // TODO: copied from the main function...
            if (!result.empty()) {
                logger::log_temp(500ms, "found {} rules", result.size());
                recorder.replace(std::move(result));
            } else {
                logger::log_temp(300ms, "found nothing");
            }
        }
        // else...
    }

    // TODO: re-implement file-saving?
    imgui_str(rule_str);

    // TODO: better layout...
    ImGui::Separator();
    // TODO: +1 is clumsy
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1);
    ImGui::SameLine();
    iter_pair(
        "<|", "prev", "next", "|>", //
        [&] { recorder.set_first(); }, [&] { recorder.set_prev(); }, [&] { recorder.set_next(); },
        [&] { recorder.set_last(); });
}

// TODO: "paste" should have a similar widget...
class file_nav_with_recorder {
    file_nav m_nav;
    std::optional<std::filesystem::path> m_file;
    rule_recorder m_recorder;

public:
    void display(rule_recorder& out) {
        // TODO: refresh... / reset(clear)...
        // TODO: set as current_path... ./ copy_path...
        // TODO: notify that all the operations are [read-only...]
        bool hit = false;

        if (!m_file) {
            ImGui::BeginDisabled();
        }
        // TODO: using childwindow only for padding...
        if (auto child = imgui_childwindow("Recorder", {},
                                           ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY)) {
            if (m_file) {
                // TODO: or wrapped full path?
                imgui_str(cpp17_u8string(m_file->filename()));
            } else {
                imgui_str("...");
            }

            // TODO: +1 is clumsy
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Total:%d At:%d", m_recorder.size(), m_recorder.pos() + 1);
            ImGui::SameLine();

            iter_pair(
                "<|", "prev", "next", "|>", //
                [&] { m_recorder.set_first(), hit = true; }, [&] { m_recorder.set_prev(), hit = true; },
                [&] { m_recorder.set_next(), hit = true; }, [&] { m_recorder.set_last(), hit = true; });
        }
        if (!m_file) {
            ImGui::EndDisabled();
        }
        if (ImGui::IsItemClicked()) {
            assert(m_file);
            hit = true;
        }

        if (hit) {
            assert(m_file);
            out.take(m_recorder.current());
        }

        {
            // TODO: find better ways to show sync...
            const bool sync = m_file && (m_recorder.current() == out.current());
            const ImU32 col = sync ? IM_COL32_WHITE : ImGui::GetColorU32(ImGuiCol_Separator);
            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), col);
        }

        // TODO: better layout...
        if (auto sel = m_nav.display()) {
            auto result = legacy::extract_rules(load_binary(*sel, 1'000'000));
            if (!result.empty()) {
                logger::log_temp(500ms, "found {} rules", result.size());
                m_recorder.replace(std::move(result));
                out.take(m_recorder.current()); // Sync with `out`.
                m_file = std::move(sel);
            } else {
                logger::log_temp(500ms, "found nothing");
            }
        }
    }
};

// TODO: reconsider: where should "current-rule" be located...
struct runner_ctrl {
    legacy::ruleT rule;
    legacy::lockT locked;
    // TODO: temporal pos; the sync point ((ruleT,lockT) pair) shouldn't belong to runner_ctrl...

    // TODO: better name?
    static constexpr int pace_min = 1, pace_max = 20;
    int pace = 1;
    bool anti_flick = true; // TODO: add explanation
    int actual_pace() const {
        if (anti_flick && legacy::will_flick(rule) && pace % 2) {
            return pace + 1;
        }
        return pace;
    }

    // TODO: remove this feature?
    static constexpr int start_min = 0, start_max = 200;
    int start_from = 0;

    static constexpr int gap_min = 0, gap_max = 20;
    int gap_frame = 0;

    bool pause = false;
    bool pause2 = false; // TODO: explain...

    void run(torusT& runner, int extra = 0) const {
        if (runner.gen() < start_from) {
            runner.run(rule, start_from - runner.gen());
        } else {
            if (extra != 0) {
                runner.run(rule, extra);
                extra = 0;
            }
            if (!pause && !pause2) {
                if (ImGui::GetFrameCount() % (gap_frame + 1) == 0) {
                    runner.run(rule, actual_pace());
                }
            }
        }
    }
};

// TODO: are there portable ways to convert argv to a valid filesystem::path (without messing up encodings)?
int main(int argc, char** argv) {
    app_backend::init();
    {
        char* base_path = SDL_GetBasePath();

        if (!base_path) {
            printf("Error: %s", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        const std::string path = base_path;
        assert(path.ends_with('\\') || path.ends_with('/'));
        SDL_free(base_path);

        const auto strdup = [](const std::string& str) {
            char* buf = new char[str.size() + 1];
            strcpy(buf, str.c_str());
            return buf;
        };

        file_nav::add_special_path(std::filesystem::u8path(path), "Exe path");

        // Avoid "imgui.ini" (and maybe also "imgui_log.txt") sprinkling everywhere.
        // TODO: IniFilename and LogFilename should be unconditionally fixed (even if not using base-path)
        // (wontfix) These memory leaks are negligible.
        ImGui::GetIO().IniFilename = strdup(path + "imgui.ini");
        ImGui::GetIO().LogFilename = strdup(path + "imgui_log.txt");

        // TODO: remove when finished...
        file_nav::add_special_path(R"(C:\*redacted*\Desktop\rulelists_new)", "Temp");
    }

    // TODO: rephrase...
    // Currently the program doesn't attempt to deal with navigation mode.
    // The controls and program-defined widgets are not taking nav-mode compatiblity into consideration.
    // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // X
    // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // X
    assert(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard));
    assert(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad));

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // TODO: works but blurry, and how to apply in project?
    // ImGui::GetIO().Fonts->AddFontFromFileTTF(R"(C:\*redacted*\Desktop\Deng.ttf)", 13, nullptr,
    //                                          ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());

    rule_recorder recorder;

    // TODO: redesign...
    tileT_filler filler{.use_seed = true, .seed = 0, .density = 0.5};
    torusT runner({.width = 480, .height = 360});
    runner.restart(filler);

    runner_ctrl ctrl{.rule = recorder.current(),
                     .locked = {},
                     .pace = 1,
                     .anti_flick = true,
                     .start_from = 0,
                     .gap_frame = 0,
                     .pause = false};

    bool show_nav_window = true;
    file_nav_with_recorder nav;

    code_image icons;

    tile_image img;
    ImVec2 img_off = {0, 0}; // TODO: supposed to be of integer-precision...
    int img_zoom = 1;

    struct pasteT : public std::optional<legacy::tileT> {
        legacy::tileT::posT pos{0, 0}; // dbegin for copy... (TODO: this is confusing...)
    };
    pasteT paste;

    // TODO: rename; move elsewhere...
    // TODO: ctrl to move selected area?
    struct selectT {
        // []
        legacy::tileT::posT select_0{0, 0}, select_1{0, 0}; // cell index, not pixel.

        void clear() { select_0 = select_1 = {0, 0}; }
        void toggle_select_all(legacy::tileT::sizeT size) {
            // all-selected ? clear : select-all
            const auto [begin, end] = range();
            if (begin.x == 0 && begin.y == 0 && end.x == size.width && end.y == size.height) {
                clear();
            } else {
                select_0 = {0, 0};
                select_1 = {.x = size.width - 1, .y = size.height - 1};
            }
        }

        legacy::tileT::rangeT range() const {
            int x0 = select_0.x, x1 = select_1.x;
            int y0 = select_0.y, y1 = select_1.y;
            if (x0 > x1) {
                std::swap(x0, x1);
            }
            if (y0 > y1) {
                std::swap(y0, y1);
            }
            return {.begin{x0, y0}, .end{x1 + 1, y1 + 1}};
        }
    };
    selectT sel{};

    while (app_backend::begin_frame()) {
        // TODO: applying following logic; consider refining it.
        // (there should be a single sync point to represent current rule (and lock)...)
        // recorder is modified during display, but will synchronize with runner's before next frame.
        assert(ctrl.rule == recorder.current());

        if (show_nav_window) {
            if (auto window = imgui_window("File nav", &show_nav_window)) {
                nav.display(recorder);
            }
        }

        // TODO: let right+ctrl move the block?

        // TODO: set pattern as init state? what if size is already changed?
        // TODO: specify mouse-dragging behavior (especially, no-op must be an option)
        // TODO: range-selected randomization don't need fixed seed. However, there should be a way to specify density.
        // TODO: support drawing as a dragging behavior if easy.
        // TODO: copy vs copy to clipboard; paste vs paste from clipboard? (don't want to pollute clipboard with small
        // rle strings...
        // TODO: should be able to recognize "rule = " part in the rle string.

        // TODO: "periodical tile" feature is generally not too useful without boundless space, and torus is
        // enough for visual feedback.
        auto show_tile = [&] {
            // TODO: refine "resize" gui and logic...
            static char input_width[20]{}, input_height[20]{};
            const float s = ImGui::GetStyle().ItemInnerSpacing.x;
            {
                const auto filter = [](ImGuiInputTextCallbackData* data) -> int {
                    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
                };
                const float w = (ImGui::CalcItemWidth() - s) / 2;
                ImGui::SetNextItemWidth(w);
                ImGui::InputTextWithHint("##Width", "width", input_width, 20, ImGuiInputTextFlags_CallbackCharFilter,
                                         filter);
                ImGui::SameLine(0, s);
                ImGui::SetNextItemWidth(w);
                ImGui::InputTextWithHint("##Height", "height", input_height, 20, ImGuiInputTextFlags_CallbackCharFilter,
                                         filter);
                ImGui::SameLine(0, s);
            }
            const bool resize = ImGui::Button("Resize");

            // TODO: move elsewhere in the gui?
            ImGui::Text("Width:%d,Height:%d,Gen:%d,Density:%.4f", runner.tile().width(), runner.tile().height(),
                        runner.gen(), float(legacy::count(runner.tile())) / runner.tile().area());
            // TODO: canvas size, tile size, selected size, paste size...

            const bool corner = ImGui::Button("Corner"); // TODO: move elsewhere...
            ImGui::SameLine();
            const bool center = ImGui::Button("Center");
            ImGui::SameLine();
            imgui_str("Zoom");
            assert(img_zoom == 1 || img_zoom == 2 || img_zoom == 4 || img_zoom == 8);
            for (const int z : {1, 2, 4, 8}) {
                ImGui::SameLine(0, s);
                // TODO: avoid usage of format...
                if (ImGui::RadioButton(std::format("{}##Z{}", z, z).c_str(), img_zoom == z)) {
                    img_off = {8, 8}; // TODO: temporarily intentional...
                    img_zoom = z;
                }
            }

            ImGui::SameLine();
            const bool fit = ImGui::Button("Fit"); // TODO: size preview?
            // TODO: select all, unselect button...

            if (paste) {
                ImGui::SameLine(), imgui_str("|"), ImGui::SameLine();
                if (ImGui::Button("Drop paste")) {
                    paste.reset();
                }
            }

            const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
            const ImVec2 canvas_size = ImGui::GetContentRegionAvail();

            // TODO: this can be negative wtf... investigate...
            // TODO: (regression?) some key ctrls are skipped by this...
            if (canvas_size.x <= 0 || canvas_size.y <= 0) {
                // TODO: show something when fit/etc are hit?
                ctrl.pause2 = true;
                // ImGui::Text("canvas: x=%f y=%f", canvas_size.x,canvas_size.y);
                return;
            }
            // TODO: or like this?
            // const ImVec2 canvas_size = [] {
            //     // Values of GetContentRegionAvail() can be negative...
            //     ImVec2 size = ImGui::GetContentRegionAvail();
            //     return ImVec2(std::max(size.x, 50.0f), std::max(size.y, 50.0f));
            // }();

            // TODO: the constraint is arbitrary; are there more sensible ways to decide size constraint?
            const auto size_clamped = [](int width, int height) {
                return legacy::tileT::sizeT{.width = std::clamp(width, 64, 1200),
                                            .height = std::clamp(height, 64, 1200)};
            };
            if (fit) {
                img_off = {0, 0};

                const legacy::tileT::sizeT size =
                    size_clamped((int)canvas_size.x / img_zoom, (int)canvas_size.y / img_zoom);
                if (runner.tile().size() != size) {
                    runner.restart(filler, size);
                    // TODO: how to support background period then?
                    assert(runner.tile().size() == size); // ???
                }
            }
            if (resize) {
                // TODO: support using current screen/tilesize/zoom?
                int iwidth = 0, iheight = 0;
                const auto [ptr, ec] = std::from_chars(input_width, input_width + 20, iwidth);
                const auto [ptr2, ec2] = std::from_chars(input_height, input_height + 20, iheight);
                if (ec == std::errc{} && ec2 == std::errc{}) {
                    img_off = {0, 0};
                    img_zoom = 1; // <-- TODO: whether to reset zoom here?
                    const legacy::tileT::sizeT size = size_clamped(iwidth, iheight);
                    if (runner.tile().size() != size) {
                        runner.restart(filler, size);
                    }
                }
                // else ...
                input_width[0] = '\0';
                input_height[0] = '\0';
            }

            // Size is fixed now:
            const legacy::tileT::sizeT tile_size = runner.tile().size();
            const ImVec2 img_size(tile_size.width * img_zoom, tile_size.height * img_zoom);

            // Validity of paste is fixed now:
            // This can happen when e.g. paste -> resize...
            if (paste && (paste->width() > tile_size.width || paste->height() > tile_size.height)) {
                paste.reset();
            }

            if (corner) {
                img_off = {0, 0};
            }
            if (center) {
                img_off = canvas_size / 2 - img_size / 2;
                img_off.x = floor(img_off.x);
                img_off.y = floor(img_off.y); // TODO: is flooring correct?
            }

            const ImVec2 img_pos = canvas_pos + img_off;

            ImDrawList* const drawlist = ImGui::GetWindowDrawList();
            drawlist->PushClipRect(canvas_pos, canvas_pos + canvas_size);
            drawlist->AddRectFilled(canvas_pos, canvas_pos + canvas_size, IM_COL32(20, 20, 20, 255));

            if (!paste) {
                drawlist->AddImage(img.update(runner.tile()), img_pos, img_pos + img_size);
            } else {
                // TODO: displays poorly with miniwindow...

                assert(paste->width() <= tile_size.width && paste->height() <= tile_size.height);
                paste.pos.x = std::clamp(paste.pos.x, 0, tile_size.width - paste->width());
                paste.pos.y = std::clamp(paste.pos.y, 0, tile_size.height - paste->height());
                // TODO: rename...
                const legacy::tileT::rangeT range = {paste.pos, paste.pos + paste->size()};

                legacy::tileT temp(paste->size()); // TODO: wasteful...
                legacy::copy(temp, {0, 0}, runner.tile(), range);
                legacy::copy<legacy::copyE::Or>(runner.tile(), paste.pos, *paste);

                drawlist->AddImage(img.update(runner.tile()), img_pos, img_pos + img_size);

                legacy::copy(runner.tile(), paste.pos, temp);

                const ImVec2 paste_min = img_pos + ImVec2(range.begin.x, range.begin.y) * img_zoom;
                const ImVec2 paste_max = img_pos + ImVec2(range.end.x, range.end.y) * img_zoom;
                drawlist->AddRectFilled(paste_min, paste_max, IM_COL32(255, 0, 0, 60));
            }

            if (const auto range = sel.range(); range.width() > 1 || range.height() > 1) {
                const ImVec2 sel_min = img_pos + ImVec2(range.begin.x, range.begin.y) * img_zoom;
                const ImVec2 sel_max = img_pos + ImVec2(range.end.x, range.end.y) * img_zoom;
                drawlist->AddRectFilled(sel_min, sel_max, IM_COL32(0, 255, 0, 40));
                drawlist->AddRect(sel_min, sel_max, IM_COL32(0, 255, 0, 160));
            }
            drawlist->PopClipRect();

            ImGui::InvisibleButton("Canvas", canvas_size);
            const bool active = ImGui::IsItemActive();
            ctrl.pause2 = paste || active; // TODO: won't work if |= active... should ctrl.run clear pause2?
            if (ImGui::IsItemHovered()) {
                // TODO: reorganize...
                const ImGuiIO& io = ImGui::GetIO();

                assert(ImGui::IsMousePosValid());
                // It turned out that, this will work well even if outside of the image...
                const ImVec2 mouse_pos = io.MousePos;
                if (active) {
                    if (!io.KeyCtrl) {
                        img_off += io.MouseDelta;
                    } else if (img_zoom == 1) {
                        runner.shift(io.MouseDelta.x, io.MouseDelta.y);
                    }
                }

                // TODO: refine...
                // TODO: zoom window temporarily conflicts with range selection... (both use Rclick)
                if (!paste && img_zoom <= 2 && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                    const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);
                    if (celx >= -10 && celx < tile_size.width + 10 && cely >= -10 && cely < tile_size.height + 10) {
                        static bool toggle = true;
                        if (auto tooltip = imgui_itemtooltip(toggle)) {
                            // int minx = std::max(celx - 20, 0);
                            // int miny = std::max(cely - 20, 0);
                            // int maxx = std::min(celx + 20, tile_size.width);
                            // int maxy = std::min(cely + 20, tile_size.height);

                            // TODO: simplify...
                            assert(tile_size.width >= 40);
                            assert(tile_size.height >= 40);
                            int minx = celx - 20, miny = cely - 20;
                            int maxx = celx + 20, maxy = cely + 20;
                            if (minx < 0) {
                                minx = 0, maxx = 40;
                            }
                            if (miny < 0) {
                                miny = 0, maxy = 40;
                            }
                            if (maxx > tile_size.width) {
                                minx = tile_size.width - 40, maxx = tile_size.width;
                            }
                            if (maxy > tile_size.height) {
                                miny = tile_size.height - 40, maxy = tile_size.height;
                            }
                            assert(maxx - minx == 40 && maxy - miny == 40);

                            const int w = maxx - minx, h = maxy - miny;
                            ImGui::PushTextWrapPos(200);
                            imgui_str("Right click to turn on/off the tooltip");
                            ImGui::PopTextWrapPos();
                            ImGui::Image(img.texture(), ImVec2(40 * 4, 40 * 4),
                                         {(float)minx / tile_size.width, (float)miny / tile_size.height},
                                         {(float)maxx / tile_size.width, (float)maxy / tile_size.height});
                        }
                    }
                }

                // TODO: this shall belong to the runner.
                // TODO: precedence against left-clicking?
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    // ctrl.pause = true;
                    const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                    const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);

                    sel.select_0.x = std::clamp(celx, 0, tile_size.width - 1);
                    sel.select_0.y = std::clamp(cely, 0, tile_size.height - 1);
                }
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                    const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);

                    sel.select_1.x = std::clamp(celx, 0, tile_size.width - 1);
                    sel.select_1.y = std::clamp(cely, 0, tile_size.height - 1);
                }
                if (paste) {
                    const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                    const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);

                    // TODO: can width<paste.width here?
                    paste.pos.x = std::clamp(celx, 0, tile_size.width - paste->width());
                    paste.pos.y = std::clamp(cely, 0, tile_size.height - paste->height());

                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        legacy::copy<legacy::copyE::Or>(runner.tile(), paste.pos, *paste);
                        paste.reset();
                    }
                }

                // TODO (temp) moved here to avoid affecting other utils (which rely on img_pos)
                if (imgui_scrolling()) {
                    const ImVec2 cellidx = (mouse_pos - img_pos) / img_zoom;
                    if (imgui_scrolldown() && img_zoom != 1) {
                        img_zoom /= 2;
                    }
                    if (imgui_scrollup() && img_zoom != 8) {
                        img_zoom *= 2;
                    }
                    img_off = (mouse_pos - cellidx * img_zoom) - canvas_pos;
                    img_off.x = round(img_off.x);
                    img_off.y = round(img_off.y); // TODO: is rounding correct?
                }
            }

            // TODO: "shrink selection" utility?
            if (imgui_keypressed(ImGuiKey_A, false)) {
                sel.toggle_select_all(tile_size);
            }
            if (imgui_keypressed(ImGuiKey_V, false)) {
                if (const char* text = ImGui::GetClipboardText()) {
                    try {
                        // TODO: or ask whether to resize runner.tile?
                        paste.emplace(legacy::from_RLE_str(text, tile_size));
                    } catch (const std::exception& err) {
                        logger::log_temp(2500ms, "{}", err.what());
                    }
                }
            }
            if (const auto range = sel.range(); range.height() > 1 || range.width() > 1) {
                if (imgui_keypressed(ImGuiKey_C, false) || imgui_keypressed(ImGuiKey_X, false)) {
                    ImGui::SetClipboardText(legacy::to_RLE_str(ctrl.rule, runner.tile(), range).c_str());
                }
                if (imgui_keypressed(ImGuiKey_Backspace, false) || imgui_keypressed(ImGuiKey_X, false)) {
                    // TODO: 0/1.../ agar...
                    legacy::clear_inside(runner.tile(), range, 0);
                }
                if (imgui_keypressed(ImGuiKey_Equal, false)) {
                    // TODO: specify density etc... or share with tileT_filler?
                    legacy::random_fill(runner.tile(), global_mt19937(), 0.5, range);
                }
                // TODO: redesign keyboard ctrl...
                if (imgui_keypressed(ImGuiKey_0, false)) {
                    legacy::clear_outside(runner.tile(), range, 0);
                }

                // TODO: refine capturing...
                if (imgui_keypressed(ImGuiKey_P, false)) {
                    // TODO: support specifying padding area...
                    legacy::tileT cap(range.size()), cap2(range.size());
                    legacy::copy(cap, {0, 0}, runner.tile(), range);
                    legacy::lockT locked{};
                    auto rulx = [&](legacy::codeT code) {
                        locked[code] = true;
                        return ctrl.rule[code];
                    };
                    // TODO: how to decide generation?
                    for (int g = 0; g < 50; ++g) {
                        cap.gather(cap, cap, cap, cap, cap, cap, cap, cap);
                        cap.apply(rulx, cap2);
                        cap.swap(cap2);
                    }
                    ctrl.locked = locked;
                }
            }
        };

        // TODO: not robust
        // ~ runner.restart(...) shall not happen before rendering.
        bool restart = false;
        int extra = 0;

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        if (auto window = imgui_window("Tile", flags)) {
            ImGui::Checkbox("Nav window", &show_nav_window);
            ImGui::SameLine();
            // TODO: change color when is too fps is too low...
            ImGui::Text("   (%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());

            show_target_rule(ctrl.rule, recorder);
            ImGui::Separator();

            if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (auto child = imgui_childwindow("Rul")) {
                    if (auto out = stone_constraints()) {
                        auto& [rule, lock] = *out;
                        recorder.take(rule);
                        ctrl.locked = lock;
                    }
                    if (auto out = edit_rule(ctrl.rule, ctrl.locked, icons)) {
                        recorder.take(*out);
                    }

                    // TODO: This is used to pair with enter key and is somewhat broken...
                    // TODO: should enter set_next first?
                    if (imgui_keypressed(ImGuiKey_Apostrophe, false)) {
                        recorder.set_prev();
                    }
                }
                ImGui::TableNextColumn();
                // TODO: it seems this childwindow is not necessary?
                if (auto child = imgui_childwindow("Til")) {
                    ImGui::PushItemWidth(FixedItemWidth);
                    ImGui::BeginGroup();
                    {
                        ImGui::Checkbox("Pause", &ctrl.pause);
                        ImGui::SameLine();
                        ImGui::BeginDisabled();
                        ImGui::Checkbox("Pause2", &ctrl.pause2);
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        // ↑ TODO: better visual?
                        // ↓ TODO: imgui_repeatbutton?
                        ImGui::PushButtonRepeat(true);
                        // TODO: should allow keyboard control...

                        // TODO: visual feedback...
                        if (ImGui::Button("+1")) {
                            extra = 1;
                        }
                        ImGui::SameLine();
                        // TODO: is this usage of ### correct?
                        // (Correct, but usage of format might be a bad idea here...)
                        if (ImGui::Button(std::format("+p({})###+p", ctrl.actual_pace()).c_str())) {
                            extra = ctrl.actual_pace();
                        }
                        ImGui::PopButtonRepeat();
                        ImGui::SameLine();
                        if (ImGui::Button("Restart") || imgui_keypressed(ImGuiKey_R, false)) {
                            restart = true;
                        }

                        // TODO: Gap-frame shall be really timer-based...
                        ImGui::SliderInt("Gap Frame (0~20)", &ctrl.gap_frame, ctrl.gap_min, ctrl.gap_max, "%d",
                                         ImGuiSliderFlags_NoInput);
                        // TODO: move elsewhere...
                        imgui_int_slider("Start gen (0~200)", &ctrl.start_from, ctrl.start_min, ctrl.start_max);
                        imgui_int_slider("Pace (1~20)", &ctrl.pace, ctrl.pace_min, ctrl.pace_max);
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("(Actual pace: %d)", ctrl.actual_pace());
                        ImGui::SameLine();
                        ImGui::Checkbox("anti-flick", &ctrl.anti_flick);
                    }
                    ImGui::EndGroup();
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    {
                        // TODO: use radio instead?
                        if (ImGui::Checkbox("Use seed", &filler.use_seed)) {
                            // TODO: unconditional?
                            if (filler.use_seed) {
                                restart = true;
                            }
                        }
                        if (!filler.use_seed) {
                            ImGui::BeginDisabled();
                        }
                        if (int seed = filler.seed; imgui_int_slider("Seed (0~99)", &seed, 0, 99)) {
                            filler.seed = seed;
                            restart = true;
                        }
                        if (!filler.use_seed) {
                            ImGui::EndDisabled();
                        }

                        // TODO: integer(ratio) density?
                        if (ImGui::SliderFloat("Init density (0~1)", &filler.density, 0.0f, 1.0f, "%.3f",
                                               ImGuiSliderFlags_NoInput)) {
                            restart = true;
                        }
                    }
                    ImGui::EndGroup();

                    show_tile();
                    ImGui::PopItemWidth();

                    // TODO: enable/disable keyboard ctrl (enable by default)
                    // TODO: redesign keyboard ctrl...
                    if (imgui_keypressed(ImGuiKey_1, true)) {
                        ctrl.gap_frame = std::max(ctrl.gap_min, ctrl.gap_frame - 1);
                    }
                    if (imgui_keypressed(ImGuiKey_2, true)) {
                        ctrl.gap_frame = std::min(ctrl.gap_max, ctrl.gap_frame + 1);
                    }
                    if (imgui_keypressed(ImGuiKey_3, true)) {
                        ctrl.pace = std::max(ctrl.pace_min, ctrl.pace - 1);
                    }
                    if (imgui_keypressed(ImGuiKey_4, true)) {
                        ctrl.pace = std::min(ctrl.pace_max, ctrl.pace + 1);
                    }
                    // TODO: explain... apply to other ctrls?
                    if ((ctrl.pause2 || !ImGui::GetIO().WantCaptureKeyboard) &&
                        ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
                        ctrl.pause = !ctrl.pause;
                    }
                    // TODO: temp (this function turns out to be necessary...)
                    if (imgui_keypressed(ImGuiKey_M, true)) {
                        if (ctrl.pause) {
                            extra = ctrl.actual_pace();
                        }
                        ctrl.pause = true;
                    }
                }
                ImGui::EndTable();
            }
        }

        ImGui::ShowDemoWindow(); // TODO: remove (or comment-out) this when all done...
        logger::tempwindow();
        app_backend::end_frame();

        if (ctrl.rule != recorder.current()) {
            ctrl.rule = recorder.current();
            restart = true;
            ctrl.pause = false; // TODO: this should be configurable...
        }
        if (restart) {
            runner.restart(filler);
        }
        ctrl.run(runner, extra); // TODO: able to result in low fps...
    }

    app_backend::clear();
    return 0;
}
