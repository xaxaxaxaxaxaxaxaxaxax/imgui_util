#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <GLFW/glfw3.h>
#include <imgui_util/core/fmt_buf.hpp>
#include <imgui_util/core/raii.hpp>
#include <imgui_util/layout/helpers.hpp>
#include <imgui_util/table/table_builder.hpp>
#include <imgui_util/theme/theme.hpp>
#include <imgui_util/theme/theme_manager.hpp>
#include <imgui_util/widgets.hpp>
#include <imnodes.h>
#include <implot.h>
#include <log.h>
#include <numbers>
#include <string>
#include <unistd.h>
#include <unordered_set>
#include <utility>
#include <vector>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace iu    = imgui_util;
namespace theme = imgui_util::theme;

namespace {

    // ---------------------------------------------------------------------------
    // Log viewer entries queued by button presses
    // ---------------------------------------------------------------------------

    struct pending_log {
        iu::log_viewer::level lvl;
        std::string           text;
    };

    std::vector<pending_log> &pending_logs() {
        static std::vector<pending_log> instance;
        return instance;
    }

    void push_log(const iu::log_viewer::level lvl, std::string text) {
        pending_logs().push_back({.lvl = lvl, .text = std::move(text)});
    }

    // ---------------------------------------------------------------------------
    // Live process data from /proc
    // ---------------------------------------------------------------------------

    struct process_row {
        int         pid;
        std::string name;
        char        state; // R, S, D, Z, T, etc.
        long        rss_bytes;
        long        utime; // clock ticks (cumulative)
        long        stime;
    };

    [[nodiscard]] std::string read_file_line(const std::filesystem::path &p) {
        std::ifstream f{p};
        std::string   line;
        if (f) std::getline(f, line);
        return line;
    }

    [[nodiscard]] std::vector<process_row> scan_processes() {
        static const long        page_size = sysconf(_SC_PAGESIZE);
        std::vector<process_row> out;
        out.reserve(512);

        for (const auto &entry: std::filesystem::directory_iterator("/proc")) {
            if (!entry.is_directory()) continue;
            const auto &fname    = entry.path().filename().string();
            int         pid      = 0;
            const auto [ptr, ec] = std::from_chars(fname.data(), fname.data() + fname.size(), pid);
            if (ec != std::errc{} || ptr != fname.data() + fname.size())
                continue; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

            const auto stat_line = read_file_line(entry.path() / "stat");
            if (stat_line.empty()) continue;

            const auto open  = stat_line.find('(');
            const auto close = stat_line.rfind(')');
            if (open == std::string::npos || close == std::string::npos) continue;

            std::string       comm = stat_line.substr(open + 1, close - open - 1);
            const char *const rest = stat_line.c_str() + close + 2;

            const char *const end = stat_line.c_str() + stat_line.size();

            // State char (field 3)
            const char *p = rest;
            if (p >= end) continue;
            const char st = *p++;

            // Skip 10 fields (4: ppid through 13: cmajflt)
            for (int n = 0; n < 10 && p < end; ++n) {
                while (p < end && *p == ' ')
                    ++p;
                while (p < end && *p != ' ')
                    ++p;
            }

            // Parse utime (field 14)
            while (p < end && *p == ' ')
                ++p;
            long utime     = 0;
            auto [p1, ec1] = std::from_chars(p, end, utime);
            if (ec1 != std::errc{}) continue;

            // Parse stime (field 15)
            while (p1 < end && *p1 == ' ')
                ++p1;
            long stime = 0;
            if (const auto [p2, ec2] = std::from_chars(p1, end, stime); ec2 != std::errc{}) continue;

            long       rss_pages = 0;
            const auto statm     = read_file_line(entry.path() / "statm");
            if (!statm.empty()) {
                const char       *sp = statm.c_str();
                const char *const se = sp + statm.size();
                while (sp < se && *sp != ' ')
                    ++sp; // skip size field
                while (sp < se && *sp == ' ')
                    ++sp;
                std::from_chars(sp, se, rss_pages);
            }

            out.push_back({.pid       = pid,
                           .name      = std::move(comm),
                           .state     = st,
                           .rss_bytes = rss_pages * page_size,
                           .utime     = utime,
                           .stime     = stime});
        }

        return out;
    }

    // ---------------------------------------------------------------------------
    // Demo state
    // ---------------------------------------------------------------------------

    struct demo_state {
        theme::theme_manager themes;
        bool                 show_theme_editor = false;


        // table
        std::vector<process_row> processes = scan_processes();
        std::unordered_set<int>  selection;
        double                   last_refresh = 0.0;

        // log viewer
        iu::log_viewer log{10'000};

        // toolbar toggles
        bool show_grid    = false;
        bool snap_enabled = true;

        // splitter
        float split_ratio = 0.4f;

        // drag-drop reorder list
        std::vector<std::string> dd_items = {"Alpha", "Bravo", "Charlie", "Delta", "Echo"};

        // modal
        bool show_confirm_modal = false;
        bool show_body_modal    = false;
        int  modal_slider_val   = 50;

        // toast position
        int toast_pos_idx = 0;

        // settings panel
        iu::settings_panel settings;
        float              settings_font_size = 14.0f;
        bool               settings_vsync     = true;
        int                settings_theme_idx = 0;

        // undo stack
        struct undo_demo_state {
            int counter = 0;
        };
        iu::undo_stack<undo_demo_state> undo{undo_demo_state{}};
        bool                            show_undo_history = true;

        // command palette
        iu::command_palette palette;

        // curve editor
        iu::curve_editor          curve{{-1, 200}};
        std::vector<iu::keyframe> curve_keys = {{.time = 0.f, .value = 0.f},
                                                {.time = 0.3f, .value = 0.8f},
                                                {.time = 0.7f, .value = 0.4f},
                                                {.time = 1.f, .value = 1.f}};

        // diff viewer
        iu::diff_viewer diff;

        // hex viewer
        iu::hex_viewer         hex{16};
        std::vector<std::byte> hex_data = [] {
            constexpr std::string_view src = "Hello, imgui_util hex viewer! "
                                             "This is sample data for the demo. "
                                             "\x00\x01\x02\x03\xFF\xFE\xFD\xFC";
            std::vector<std::byte>     v(src.size());
            for (std::size_t i = 0; i < src.size(); ++i)
                v[i] = static_cast<std::byte>(src[i]);
            return v;
        }();

        // inline edit
        std::string inline_text = "Double-click to edit me";

        // key binding
        iu::key_combo kb_save{.key = ImGuiKey_S, .mods = ImGuiMod_Ctrl};
        iu::key_combo kb_open{.key = ImGuiKey_O, .mods = ImGuiMod_Ctrl};

        // notification center
        bool show_notifications = false;

        // range slider
        float range_lo     = 20.0f;
        float range_hi     = 80.0f;
        int   range_int_lo = 10;
        int   range_int_hi = 90;

        // reorder list
        std::vector<std::string> reorder_items = {"First", "Second", "Third", "Fourth", "Fifth"};

        // search bar
        iu::search::search_bar<128> search;

        // tag input
        std::vector<std::string> tags = {"C++", "ImGui", "RAII"};

        // timeline
        iu::timeline                    tl{200.0f};
        std::vector<iu::timeline_event> tl_events = {
            {.start = 0.0f, .end = 5.0f, .label = "Intro", .color = IM_COL32(100, 150, 255, 200), .track = 0},
            {.start = 3.0f, .end = 10.0f, .label = "Audio", .color = IM_COL32(255, 100, 100, 200), .track = 1},
            {.start = 6.0f, .end = 12.0f, .label = "Effects", .color = IM_COL32(100, 255, 100, 200), .track = 0},
            {.start = 11.0f, .end = 18.0f, .label = "Credits", .color = IM_COL32(255, 200, 50, 200), .track = 1},
        };
        float tl_playhead = 0.0f;
    };

    // ---------------------------------------------------------------------------
    // Sections
    // ---------------------------------------------------------------------------

    [[nodiscard]] constexpr const char *state_label(const char st) noexcept {
        switch (st) {
            case 'R':
                return "running";
            case 'S':
                return "sleeping";
            case 'D':
                return "disk wait";
            case 'Z':
                return "zombie";
            case 'T':
                return "stopped";
            case 't':
                return "traced";
            case 'I':
                return "idle";
            default:
                return "?";
        }
    }

    void section_table(demo_state &s) {
        if (!ImGui::CollapsingHeader("Table Builder")) return;

        ImGui::TextWrapped("Live process list from /proc. Fluent table API with multi-column "
                           "sorting, Ctrl/Shift selection, and virtual clipping.");
        ImGui::Spacing();

        bool         data_changed = false;
        const double now          = ImGui::GetTime();
        if (now - s.last_refresh > 2.0) {
            s.processes    = scan_processes();
            s.last_refresh = now;
            data_changed   = true;
        }
        if (ImGui::Button("Refresh")) {
            s.processes    = scan_processes();
            s.last_refresh = now;
            data_changed   = true;
        }
        ImGui::SameLine();
        const iu::fmt_buf<48> stats{"{} processes, {} selected", s.processes.size(), s.selection.size()};
        ImGui::TextUnformatted(stats.c_str());
        ImGui::Spacing();

        auto table =
            iu::table_builder<process_row>{}
                .set_id("##procs")
                .set_flags(ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
                           | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY
                           | ImGuiTableFlags_SortMulti)
                .set_scroll_freeze(0, 1)
                .set_selection(&s.selection)
                .set_row_id([](const process_row &r) { return r.pid; })
                .add_column("PID", 50, [](const process_row &r) { ImGui::Text("%d", r.pid); },
                            ImGuiTableColumnFlags_DefaultSort)
                .add_column("Name", 0, [](const process_row &r) { ImGui::TextUnformatted(r.name.c_str()); })
                .add_column("State", 70, [](const process_row &r) { ImGui::TextUnformatted(state_label(r.state)); })
                .add_column("RSS", 80, [](const process_row &r) {
            ImGui::Text("%s", iu::format_bytes(static_cast<std::int64_t>(r.rss_bytes)).c_str());
        }).add_column("CPU time", 80, [](const process_row &r) {
            static const long tps   = sysconf(_SC_CLK_TCK);
            const long        total = r.utime + r.stime;
            const long        secs  = total / tps;
            const long        frac  = (total % tps) * 100 / tps;
            ImGui::Text("%ld.%02lds", secs, frac);
        });

        if (table.begin(300)) {
            using cmp               = iu::table_builder<process_row>::comparator_fn;
            std::array<cmp, 5> cmps = {
                cmp{[](const process_row &a, const process_row &b) { return a.pid < b.pid; }},
                cmp{[](const process_row &a, const process_row &b) { return a.name < b.name; }},
                cmp{[](const process_row &a, const process_row &b) { return a.state < b.state; }},
                cmp{[](const process_row &a, const process_row &b) { return a.rss_bytes < b.rss_bytes; }},
                cmp{[](const process_row &a, const process_row &b) {
                return (a.utime + a.stime) < (b.utime + b.stime);
            }},
            };
            table.sort_if_dirty(s.processes, std::span<cmp>{cmps}, data_changed);
            table.render_clipped(std::span<const process_row>{s.processes});
            iu::table_builder<process_row>::end();
        }
    }

    void section_log_viewer(demo_state &s) {
        if (!ImGui::CollapsingHeader("Log Viewer")) return;

        ImGui::TextWrapped("Ring-buffer log with level filtering, search, auto-scroll, and timestamps.");
        ImGui::Spacing();

        static int msg_counter = 0;
        if (ImGui::Button("Info"))
            push_log(iu::log_viewer::level::info, std::format("info message #{}", ++msg_counter));
        ImGui::SameLine();
        if (ImGui::Button("Warning"))
            push_log(iu::log_viewer::level::warning, std::format("warning message #{}", ++msg_counter));
        ImGui::SameLine();
        if (ImGui::Button("Error"))
            push_log(iu::log_viewer::level::error, std::format("error message #{}", ++msg_counter));
        ImGui::SameLine();
        if (ImGui::Button("Burst x50"))
            for (int i = 0; i < 50; ++i)
                push_log(iu::log_viewer::level::info, std::format("burst #{}", ++msg_counter));

        (void) s.log.render([](auto emit) {
            for (auto &[lvl, text]: pending_logs())
                emit(lvl, text);
            pending_logs().clear();
        }, "##log_search", "##log_child");
    }

    void section_spinner(const demo_state & /**/) {
        if (!ImGui::CollapsingHeader("Spinner")) return;

        ImGui::TextWrapped("Indeterminate spinning arc and determinate progress arc.");
        ImGui::Spacing();

        ImGui::TextUnformatted("Indeterminate:");
        ImGui::SameLine();
        iu::spinner("##demo_spin", 10.0f, 2.5f);
        ImGui::SameLine();
        iu::spinner("##demo_spin2", 10.0f, 2.5f, iu::colors::teal);
        ImGui::SameLine();
        iu::spinner("##demo_spin3", 10.0f, 2.5f, iu::colors::warning);
        ImGui::Spacing();

        ImGui::TextUnformatted("Determinate:");
        const auto  t = static_cast<float>(ImGui::GetTime());
        const float p = std::fmod(t * 0.2f, 1.0f);
        ImGui::SameLine();
        iu::spinner_progress("##prog1", p, 10.0f, 2.5f);
        ImGui::SameLine();
        iu::spinner_progress("##prog2", 0.25f, 10.0f, 2.5f, iu::colors::error);
        ImGui::SameLine();
        iu::spinner_progress("##prog3", 0.75f, 10.0f, 2.5f, iu::colors::success);
    }

    void section_toolbar(demo_state &s) {
        if (!ImGui::CollapsingHeader("Toolbar")) return;

        ImGui::TextWrapped("Fluent toolbar builder with buttons, toggles, separators, and tooltips.");
        ImGui::Spacing();

        iu::toolbar()
            .button("New", [&] { iu::toast::show("New clicked"); }, "Create new item")
            .button("Open", [&] { iu::toast::show("Open clicked"); }, "Open existing item")
            .button("Save", [&] { iu::toast::show("Saved!", iu::severity::success); }, "Save current item")
            .separator()
            .toggle("Grid", &s.show_grid, "Toggle grid overlay")
            .toggle("Snap", &s.snap_enabled, "Toggle snap to grid")
            .render();

        ImGui::Spacing();
        iu::fmt_text("Grid: {}, Snap: {}", s.show_grid ? "ON" : "OFF", s.snap_enabled ? "ON" : "OFF");
    }

    void section_drag_drop(demo_state &s) {
        if (!ImGui::CollapsingHeader("Drag & Drop")) return;

        ImGui::TextWrapped("Type-safe drag-drop helpers. Drag items to reorder.");
        ImGui::Spacing();

        for (int i = 0; std::cmp_less(i, s.dd_items.size()); ++i) {
            ImGui::Selectable(s.dd_items[i].c_str());
            iu::drag_drop::source("DD_REORDER", i, s.dd_items[i]);
            if (const iu::drag_drop_target tgt{}) {
                if (const auto src_idx = iu::drag_drop::accept_payload<int>("DD_REORDER")) {
                    if (*src_idx != i) std::swap(s.dd_items[*src_idx], s.dd_items[i]);
                }
            }
        }
    }

    void section_settings_panel(demo_state &s) {
        if (!ImGui::CollapsingHeader("Settings Panel")) return;

        ImGui::TextWrapped("Tree-navigated settings panel with left-side navigation and right-side content.");
        ImGui::Spacing();

        s.settings
            .section("General",
                     [&] {
            ImGui::SliderFloat("Font Size", &s.settings_font_size, 8.0f, 24.0f);
            ImGui::Checkbox("VSync", &s.settings_vsync);
        })
            .section("Appearance", [&] {
            constexpr std::array<const char *, 3> themes = {"Dark", "Light", "System"};
            ImGui::Combo("Theme", &s.settings_theme_idx, themes.data(), static_cast<int>(themes.size()));
        }).section("Keybinds", "General", [&] {
            ImGui::TextUnformatted("Configure key bindings here...");
        }).render("##settings_demo");
    }

    void section_splitter(demo_state &s) {
        if (!ImGui::CollapsingHeader("Splitter")) return;

        ImGui::TextWrapped("Resizable split panels. Drag the divider to resize.");
        ImGui::Spacing();

        constexpr float thickness = 8.0f;
        const float     usable    = ImGui::GetContentRegionAvail().x - thickness;

        {
            const iu::child left{"##split_left", ImVec2(usable * s.split_ratio, 150), ImGuiChildFlags_Borders};
            ImGui::TextUnformatted("Left Panel");
            ImGui::Separator();
            for (int i = 0; i < 5; ++i)
                iu::fmt_text("Left item {}", i);
        }
        ImGui::SameLine(0, 0);
        (void) iu::splitter("##demo_split", iu::split_direction::horizontal, s.split_ratio, thickness);
        ImGui::SameLine(0, 0);
        {
            const iu::child right{"##split_right", ImVec2(usable * (1.0f - s.split_ratio), 150),
                                  ImGuiChildFlags_Borders};
            ImGui::TextUnformatted("Right Panel");
            ImGui::Separator();
            for (int i = 0; i < 5; ++i)
                iu::fmt_text("Right item {}", i);
        }
        iu::fmt_text("Ratio: {:.0f}% / {:.0f}%", s.split_ratio * 100.0f, (1.0f - s.split_ratio) * 100.0f);
    }

    void section_modal(demo_state &s) {
        if (!ImGui::CollapsingHeader("Modal Builder")) return;

        ImGui::TextWrapped("Fluent modal dialog builder with ok/cancel, danger mode, and keyboard shortcuts.");
        ImGui::Spacing();

        if (ImGui::Button("Simple Confirm")) s.show_confirm_modal = true;
        ImGui::SameLine();
        if (ImGui::Button("Custom Body")) s.show_body_modal = true;

        iu::modal_builder("Delete Item?")
            .message("Are you sure? This action cannot be undone.")
            .ok_button("Delete", [&] { iu::toast::show("Deleted!", iu::severity::error); })
            .cancel_button("Cancel", [] {})
            .danger()
            .render(&s.show_confirm_modal);

        iu::modal_builder("Adjust Settings")
            .body([&] {
            ImGui::TextUnformatted("Adjust the value below:");
            ImGui::SliderInt("##modal_slider", &s.modal_slider_val, 0, 100);
            iu::fmt_text("Current: {}", s.modal_slider_val);
        })
            .ok_button(
                "Apply",
                [&] { iu::toast::show(std::format("Applied value: {}", s.modal_slider_val), iu::severity::success); })
            .cancel_button("Cancel", [] {})
            .size(350, 0)
            .render(&s.show_body_modal);
    }

    void section_toast(demo_state &s) {
        if (!ImGui::CollapsingHeader("Toast Notifications")) return;

        ImGui::TextWrapped("Stackable toast notifications with fade-out, click-to-dismiss, and configurable position.");
        ImGui::Spacing();

        if (ImGui::Button("Info")) iu::toast::show("This is an info toast.");
        ImGui::SameLine();
        if (ImGui::Button("Success")) iu::toast::show("Operation succeeded!", iu::severity::success);
        ImGui::SameLine();
        if (ImGui::Button("Warning")) iu::toast::show("Low disk space.", iu::severity::warning);
        ImGui::SameLine();
        if (ImGui::Button("Error")) iu::toast::show("Connection failed!", iu::severity::error);
        ImGui::SameLine();
        if (ImGui::Button("Clear All")) iu::toast::clear();

        ImGui::Spacing();
        constexpr std::array positions = {"Bottom-Right", "Top-Right", "Bottom-Left", "Top-Left"};
        if (ImGui::Combo("Position", &s.toast_pos_idx, positions.data(), static_cast<int>(positions.size())))
            iu::toast::set_position(static_cast<iu::toast::position>(s.toast_pos_idx));
    }

    void section_tree_view(const demo_state & /**/) {
        if (!ImGui::CollapsingHeader("Tree View")) return;

        ImGui::TextWrapped("Callback-based tree view with selection and right-click context menus.");
        ImGui::Spacing();

        struct scene_node {
            const char                 *name;
            std::span<const scene_node> children;
        };

        static constexpr std::array meshes = {
            scene_node{.name = "Cube", .children = {}},
            scene_node{.name = "Sphere", .children = {}},
            scene_node{.name = "Cylinder", .children = {}},
        };
        static constexpr std::array lights = {
            scene_node{.name = "Point Light", .children = {}},
            scene_node{.name = "Spot Light", .children = {}},
        };
        static constexpr std::array root = {
            scene_node{.name = "Meshes", .children = meshes},
            scene_node{.name = "Lights", .children = lights},
            scene_node{.name = "Camera", .children = {}},
        };

        static const scene_node *last_selected = nullptr;

        iu::tree_view<scene_node> tv{"##scene_tree"};
        tv.set_children([](const scene_node &n) { return n.children; })
            .set_label([](const scene_node &n) { return n.name; })
            .set_on_select([](const scene_node &n) { last_selected = &n; })
            .set_on_context_menu([](const scene_node &n) {
            if (ImGui::MenuItem("Rename")) iu::toast::show(std::format("Rename: {}", n.name));
            if (ImGui::MenuItem("Delete")) iu::toast::show(std::format("Delete: {}", n.name), iu::severity::error);
        }).render(std::span{root});

        ImGui::Spacing();
        if (last_selected)
            iu::fmt_text("Selected: {}", last_selected->name);
        else
            ImGui::TextDisabled("Click a node to select");
    }

    void section_undo_stack(demo_state &s) {
        if (!ImGui::CollapsingHeader("Undo Stack")) return;

        ImGui::TextWrapped("Generic undo/redo with Ctrl+Z/Y shortcuts and clickable history panel.");
        ImGui::Spacing();

        iu::fmt_text("Counter: {}", s.undo.current().counter);
        ImGui::Spacing();

        if (ImGui::Button("Increment")) {
            auto next = s.undo.current();
            ++next.counter;
            s.undo.push("Increment", next);
        }
        ImGui::SameLine();
        if (ImGui::Button("Decrement")) {
            auto next = s.undo.current();
            --next.counter;
            s.undo.push("Decrement", next);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            s.undo.push("Reset", {.counter = 0});
        }
        ImGui::SameLine();
        ImGui::Checkbox("History Panel", &s.show_undo_history);

        if (s.undo.handle_shortcuts()) iu::toast::show("Undo/Redo");

        if (s.show_undo_history) s.undo.render_history_panel("Undo History##demo");
    }

    void section_layout_helpers(const demo_state & /**/) {
        if (!ImGui::CollapsingHeader("Layout Helpers")) return;

        ImGui::TextWrapped("Centering, right-alignment, and horizontal layout utilities.");
        ImGui::Spacing();

        ImGui::SeparatorText("center_next / right_align_next");
        iu::layout::center_next(200.0f);
        ImGui::Button("Centered (200px)", {200, 0});
        iu::layout::right_align_next(150.0f);
        ImGui::Button("Right (150px)", {150, 0});

        ImGui::SeparatorText("horizontal_layout");
        iu::layout::horizontal_layout h{4.0f};
        for (int i = 0; i < 6; ++i) {
            h.next();
            ImGui::Button(iu::fmt_buf<16>("Btn {}", i).c_str(), {60, 0});
        }
        ImGui::NewLine();
    }

    void section_menu_bar_builder(const demo_state & /**/) {
        if (!ImGui::CollapsingHeader("Menu Bar Builder")) return;

        ImGui::TextWrapped("Fluent menu bar builder rendered inside this section.");
        ImGui::Spacing();

        static bool auto_save   = false;
        static bool show_grid_m = false;

        iu::menu_bar_builder()
            .menu("File",
                  [&](auto &m) {
            (void) m.item("New", [&] { iu::toast::show("File > New"); }, "Ctrl+N")
                .item("Open", [&] { iu::toast::show("File > Open"); }, "Ctrl+O")
                .separator()
                .item("Save", [&] { iu::toast::show("File > Save", iu::severity::success); }, "Ctrl+S")
                .separator()
                .item("Quit", [] {}, "Alt+F4", false);
        })
            .menu("Edit",
                  [&](auto &m) {
            (void) m.item("Undo", [&] { iu::toast::show("Edit > Undo"); }, "Ctrl+Z")
                .item("Redo", [&] { iu::toast::show("Edit > Redo"); }, "Ctrl+Y")
                .separator()
                .checkbox("Auto-save", &auto_save);
        })
            .menu("View", [&](auto &m) {
            (void) m.checkbox("Grid", &show_grid_m).separator().item("Reset Layout", [&] {
                iu::toast::show("Layout reset");
            });
        }).render();

        iu::fmt_text("Auto-save: {}, Grid: {}", auto_save ? "ON" : "OFF", show_grid_m ? "ON" : "OFF");
    }

    void section_confirm_button(const demo_state & /**/) {
        if (!ImGui::CollapsingHeader("Confirm Button")) return;

        ImGui::TextWrapped("Click-to-arm, click-again-to-confirm button for destructive actions.");
        ImGui::Spacing();

        if (iu::confirm_button("Delete Item", "##del")) iu::toast::show("Item deleted!", iu::severity::error);
        ImGui::SameLine();
        if (iu::confirm_button("Reset All", "##reset", 5.0f))
            iu::toast::show("Everything reset!", iu::severity::warning);
    }

    void section_controls(const demo_state & /**/) {
        if (!ImGui::CollapsingHeader("Controls")) return;

        ImGui::TextWrapped("Styled buttons, combo boxes, and convenience wrappers.");
        ImGui::Spacing();

        ImGui::SeparatorText("styled_button");
        if (iu::styled_button("Delete", {0.8f, 0.2f, 0.2f, 1.0f}))
            iu::toast::show("Delete clicked", iu::severity::error);
        ImGui::SameLine();
        if (iu::styled_button("Accept", {0.2f, 0.7f, 0.3f, 1.0f})) iu::toast::show("Accepted", iu::severity::success);
        ImGui::SameLine();
        if (iu::styled_button("Info", {0.2f, 0.4f, 0.8f, 1.0f})) iu::toast::show("Info clicked");

        ImGui::SeparatorText("column_combo");
        static int                                   combo_idx   = 0;
        static constexpr std::array<const char *, 4> combo_items = {"Option A", "Option B", "Option C", "Option D"};
        if (iu::column_combo("Choose##cc", combo_idx, std::span<const char *const>{combo_items}))
            iu::toast::show(std::format("Selected: {}", combo_items[combo_idx]));

        ImGui::SeparatorText("checkbox_action");
        static bool cb_val = false;
        (void) iu::checkbox_action("Enable feature", &cb_val, [] { iu::toast::show("Feature toggled"); });
    }

    void section_curve_editor(demo_state &s) {
        if (!ImGui::CollapsingHeader("Curve Editor")) return;

        ImGui::TextWrapped("Keyframe curve editor with cubic hermite interpolation. "
                           "Double-click to add, Delete to remove, drag to move.");
        ImGui::Spacing();

        if (s.curve.render("##curve", s.curve_keys)) iu::toast::show("Curve modified");

        ImGui::Spacing();
        const float t   = std::fmod(static_cast<float>(ImGui::GetTime()) * 0.2f, 1.0f);
        const float val = iu::curve_editor::evaluate(s.curve_keys, t);
        iu::fmt_text("t={:.2f}  value={:.3f}  keyframes={}", t, val, s.curve_keys.size());
    }

    void section_diff_viewer(const demo_state &s) {
        if (!ImGui::CollapsingHeader("Diff Viewer")) return;

        ImGui::TextWrapped("Side-by-side diff viewer with synchronized scrolling and line numbers.");
        ImGui::Spacing();

        static const std::vector<iu::diff_line> left = {
            {.kind = iu::diff_kind::same, .text = "int main() {"},
            {.kind = iu::diff_kind::removed, .text = R"(    printf("hello\n");)"},
            {.kind = iu::diff_kind::same, .text = "    int x = 0;"},
            {.kind = iu::diff_kind::changed, .text = "    x = x + 1;"},
            {.kind = iu::diff_kind::same, .text = "    return 0;"},
            {.kind = iu::diff_kind::same, .text = "}"},
        };
        static const std::vector<iu::diff_line> right = {
            {.kind = iu::diff_kind::same, .text = "int main() {"},
            {.kind = iu::diff_kind::added, .text = "    std::println(\"hello\");"},
            {.kind = iu::diff_kind::same, .text = "    int x = 0;"},
            {.kind = iu::diff_kind::changed, .text = "    x += 1;"},
            {.kind = iu::diff_kind::same, .text = "    return 0;"},
            {.kind = iu::diff_kind::same, .text = "}"},
        };

        s.diff.render("##diff_demo", left, right);
    }

    void section_hex_viewer(demo_state &s) {
        if (!ImGui::CollapsingHeader("Hex Viewer")) return;

        ImGui::TextWrapped("Memory/hex byte viewer with address gutter, ASCII column, and editing. "
                           "Double-click a byte to edit.");
        ImGui::Spacing();

        s.hex.add_highlight(0, 5, IM_COL32(100, 200, 255, 40));
        if (s.hex.render_editable("##hex_demo", std::span{s.hex_data}, 0x1000))
            iu::toast::show("Byte modified", iu::severity::warning);
        s.hex.clear_highlights();
    }

    void section_inline_edit(demo_state &s) {
        if (!ImGui::CollapsingHeader("Inline Edit")) return;

        ImGui::TextWrapped("Click-to-edit text label. Double-click to enter edit mode, "
                           "Enter to commit, Escape to cancel.");
        ImGui::Spacing();

        ImGui::TextUnformatted("Label:");
        ImGui::SameLine();
        if (iu::inline_edit("##inline_demo", s.inline_text, 300.0f))
            iu::toast::show(std::format("Committed: {}", s.inline_text), iu::severity::success);
    }

    void section_key_binding(demo_state &s) {
        if (!ImGui::CollapsingHeader("Key Binding")) return;

        ImGui::TextWrapped("Key binding capture widget. Click the button, then press a key combo to bind.");
        ImGui::Spacing();

        if (iu::key_binding_editor("Save", &s.kb_save))
            iu::toast::show(std::format("Save bound to: {}", iu::to_string(s.kb_save).sv()));
        if (iu::key_binding_editor("Open", &s.kb_open))
            iu::toast::show(std::format("Open bound to: {}", iu::to_string(s.kb_open).sv()));
    }

    void section_notification_center(demo_state &s) {
        if (!ImGui::CollapsingHeader("Notification Center")) return;

        ImGui::TextWrapped("Persistent notification history panel with severity, actions, and relative timestamps.");
        ImGui::Spacing();

        if (ImGui::Button("Push Info"))
            iu::notification_center::push("Build Complete", "All 42 tests passed", iu::severity::success);
        ImGui::SameLine();
        if (ImGui::Button("Push Error"))
            iu::notification_center::push("Deploy Failed", "Connection timed out", iu::severity::error, "Retry",
                                          [] { iu::toast::show("Retrying..."); });
        ImGui::SameLine();
        if (ImGui::Button("Toggle Panel")) s.show_notifications = !s.show_notifications;
        ImGui::SameLine();
        iu::fmt_text("{} unread", iu::notification_center::unread_count());

        if (s.show_notifications)
            iu::notification_center::render_panel("Notifications##nc_panel", &s.show_notifications);
    }

    void section_range_slider(demo_state &s) {
        if (!ImGui::CollapsingHeader("Range Slider")) return;

        ImGui::TextWrapped("Dual-handle range slider for min/max selection.");
        ImGui::Spacing();

        (void) iu::range_slider("Float Range", &s.range_lo, &s.range_hi, 0.0f, 100.0f, "{:.1f}");
        (void) iu::range_slider("Int Range", &s.range_int_lo, &s.range_int_hi, 0, 100);

        iu::fmt_text("Float: [{:.1f}, {:.1f}]  Int: [{}, {}]", s.range_lo, s.range_hi, s.range_int_lo, s.range_int_hi);
    }

    void section_reorder_list(demo_state &s) {
        if (!ImGui::CollapsingHeader("Reorder List")) return;

        ImGui::TextWrapped("Drag-to-reorder list with grip handles and insertion indicator.");
        ImGui::Spacing();

        if (iu::reorder_list("##reorder_demo", s.reorder_items,
                             [](const std::string &item) { ImGui::TextUnformatted(item.c_str()); }))
            iu::toast::show("Order changed");
    }

    void section_search_bar(demo_state &s) {
        if (!ImGui::CollapsingHeader("Search Bar")) return;

        ImGui::TextWrapped("Case-insensitive search bar with clear button. Filters the list below.");
        ImGui::Spacing();

        static constexpr std::array items = {"Apple", "Banana", "Cherry",   "Date", "Elderberry",
                                             "Fig",   "Grape",  "Honeydew", "Kiwi", "Lemon"};

        (void) s.search.render("Filter...", 200.0f, "##search_demo");
        ImGui::Spacing();

        for (const auto *item: items) {
            if (s.search.matches(std::string_view{item})) ImGui::BulletText("%s", item);
        }
    }

    void section_tag_input(demo_state &s) {
        if (!ImGui::CollapsingHeader("Tag Input")) return;

        ImGui::TextWrapped("Tag/chip input with pill rendering. Type and press Enter to add, X to remove.");
        ImGui::Spacing();

        if (iu::tag_input("Tags##demo", s.tags, 10)) iu::toast::show(std::format("{} tags", s.tags.size()));

        ImGui::Spacing();
        iu::fmt_text("Tags: {}", s.tags.size());
    }

    void section_text(const demo_state & /**/) {
        if (!ImGui::CollapsingHeader("Text Utilities")) return;

        ImGui::TextWrapped("Semantic text colors, alignment, truncation, and formatted text helpers.");
        ImGui::Spacing();

        ImGui::SeparatorText("Semantic Colors");
        iu::colored_text("Accent text", iu::colors::accent);
        iu::colored_text("Teal text", iu::colors::teal);
        iu::secondary_text("Secondary text");
        iu::dim_text("Dim text");
        iu::error_text("Error text");
        iu::status_message("Success status", iu::severity::success);
        iu::status_message("Warning status", iu::severity::warning);

        ImGui::SeparatorText("fmt_text");
        iu::fmt_text("Formatted: pi = {:.4f}, count = {}", std::numbers::pi_v<float>, 42);

        ImGui::SeparatorText("Truncation");
        constexpr std::string_view long_text = "This is a very long string that will be truncated to fit";
        const auto                 t         = iu::truncate_to_width(long_text, 200.0f);
        ImGui::TextUnformatted(t.view().data(), t.view().data() + t.view().size());
        iu::fmt_text("Truncated: {}", t.was_truncated() ? "yes" : "no");

        ImGui::SeparatorText("format_count / format_bytes");
        iu::fmt_text("1500 -> {}", iu::format_count(1500).sv());
        iu::fmt_text("2500000 -> {}", iu::format_count(2500000).sv());
        iu::fmt_text("1536 bytes -> {}", iu::format_bytes(1536).sv());
        iu::fmt_text("1048576 bytes -> {}", iu::format_bytes(1048576).sv());
    }

    void section_timeline(demo_state &s) {
        if (!ImGui::CollapsingHeader("Timeline")) return;

        ImGui::TextWrapped("Horizontal timeline with tracks, draggable events, and playhead. "
                           "Drag events to move/resize, click ruler to scrub playhead.");
        ImGui::Spacing();

        static constexpr std::array<std::string_view, 2> track_labels = {"Video", "Audio"};
        (void) s.tl.set_snap(0.5f).set_track_labels(track_labels);

        if (s.tl.render("##timeline_demo", s.tl_events, s.tl_playhead, 0.0f, 20.0f))
            iu::toast::show(iu::fmt_buf<32>("Playhead: {:.1f}", s.tl_playhead).sv());
    }

    void section_helpers(const demo_state & /**/) {
        if (!ImGui::CollapsingHeader("Helpers")) return;

        ImGui::TextWrapped("Small reusable widgets: help markers, section headers, label-value rows, shortcuts.");
        ImGui::Spacing();

        iu::section_header("Section Header");
        iu::label_value("FPS:", "60.0");
        iu::label_value_colored("Status:", iu::colors::success, "Online");
        iu::help_marker("This is a help tooltip.");

        ImGui::Spacing();
        static constexpr std::array<iu::shortcut, 3> shortcuts = {{
            {.key = "Ctrl+S", .description = "Save project"},
            {.key = "Ctrl+Z", .description = "Undo"},
            {.key = "Ctrl+P", .description = "Command palette"},
        }};
        iu::shortcut_list("Keyboard Shortcuts", shortcuts);
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    Log::setTimestamps(true);
    Log::autoDetectColors();

    glfwSetErrorCallback([](int, const char *msg) { Log::error("GLFW", msg); });
    if (glfwInit() == 0) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    auto *const window = glfwCreateWindow(1280, 800, "imgui_util demo", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImPlot::CreateContext();

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    demo_state state;
    if (const auto *const preset = theme::theme_manager::find_preset("Obsidian"))
        state.themes.set_theme(theme::theme_config::from_preset(*preset));
    state.themes.get_current_theme().apply();

    state.palette.add("Toggle Theme Editor", [&] { state.show_theme_editor = !state.show_theme_editor; });
    state.palette.add("Add Info Log", [&] { push_log(iu::log_viewer::level::info, "Command palette log"); });
    state.palette.add("Add Warning Log", [&] { push_log(iu::log_viewer::level::warning, "Command palette warning"); });
    state.palette.add("Add Error Log", [&] { push_log(iu::log_viewer::level::error, "Command palette error"); });
    state.palette.add("Clear Toasts", [] { iu::toast::clear(); });
    state.palette.add("Show Success Toast", [] { iu::toast::show("From palette!", iu::severity::success); });
    state.palette.add("Refresh Processes", [&] { state.processes = scan_processes(); });

    Log::info("Demo", "imgui_util demo started");

    bool show_demo = true;

    while (glfwWindowShouldClose(window) == 0) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow(&show_demo);

        // Append imgui_util sections into the same "Dear ImGui Demo" window.
        if (const iu::window w{"Dear ImGui Demo"}) {
            ImGui::SeparatorText("imgui_util");
            if (ImGui::Button("Theme Editor")) state.show_theme_editor = !state.show_theme_editor;

            section_confirm_button(state);
            section_controls(state);
            section_curve_editor(state);
            section_diff_viewer(state);
            section_drag_drop(state);
            section_helpers(state);
            section_hex_viewer(state);
            section_inline_edit(state);
            section_key_binding(state);
            section_layout_helpers(state);
            section_log_viewer(state);
            section_menu_bar_builder(state);
            section_modal(state);
            section_notification_center(state);
            section_range_slider(state);
            section_reorder_list(state);
            section_search_bar(state);
            section_settings_panel(state);
            section_spinner(state);
            section_splitter(state);
            section_table(state);
            section_tag_input(state);
            section_text(state);
            section_timeline(state);
            section_toast(state);
            section_toolbar(state);
            section_tree_view(state);
            section_undo_stack(state);
        }

        if (state.show_theme_editor) state.themes.render_theme_editor(&state.show_theme_editor);

        if (ImGui::IsKeyPressed(ImGuiKey_P) && ImGui::GetIO().KeyCtrl) state.palette.open();
        state.palette.render();

        if (const iu::status_bar sb{}) {
            iu::fmt_text("imgui_util demo | {} processes", state.processes.size());
            sb.right_section();
            iu::fmt_text("{:.0f} FPS", ImGui::GetIO().Framerate);
        }

        iu::toast::render();

        ImGui::Render();
        int fb_w = 0;
        int fb_h = 0;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}
