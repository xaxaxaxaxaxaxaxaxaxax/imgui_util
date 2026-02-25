#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <GLFW/glfw3.h>
#include <imgui_util/core/fmt_buf.hpp>
#include <imgui_util/core/raii.hpp>
#include <imgui_util/table/table_builder.hpp>
#include <imgui_util/theme/theme.hpp>
#include <imgui_util/theme/theme_manager.hpp>
#include <imgui_util/widgets/context_menu.hpp>
#include <imgui_util/widgets/drag_drop.hpp>
#include <imgui_util/widgets/log_viewer.hpp>
#include <imgui_util/widgets/modal_builder.hpp>
#include <imgui_util/widgets/progress_bar.hpp>
#include <imgui_util/widgets/property_editor.hpp>
#include <imgui_util/widgets/spinner.hpp>
#include <imgui_util/widgets/splitter.hpp>
#include <imgui_util/widgets/tab_bar_builder.hpp>
#include <imgui_util/widgets/text.hpp>
#include <imgui_util/widgets/toast.hpp>
#include <imgui_util/widgets/toolbar.hpp>
#include <imnodes.h>
#include <implot.h>
#include <log.h>
#include <string>
#include <unistd.h>
#include <unordered_set>
#include <vector>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace iu    = imgui_util;
namespace theme = imgui_util::theme;

// ---------------------------------------------------------------------------
// Log viewer entries queued by button presses
// ---------------------------------------------------------------------------

struct pending_log {
    iu::log_viewer::level lvl;
    std::string           text;
};

static std::vector<pending_log> g_pending;

static void push_log(iu::log_viewer::level lvl, std::string text) {
    g_pending.push_back({lvl, std::move(text)});
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

static auto read_file_line(const std::filesystem::path &p) -> std::string {
    std::ifstream f{p};
    std::string   line;
    if (f) std::getline(f, line);
    return line;
}

static auto scan_processes() -> std::vector<process_row> {
    static const long        page_size = sysconf(_SC_PAGESIZE);
    std::vector<process_row> out;
    out.reserve(512);

    for (const auto &entry: std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;
        const auto &fname = entry.path().filename().string();
        int         pid   = 0;
        auto [ptr, ec]    = std::from_chars(fname.data(), fname.data() + fname.size(), pid);
        if (ec != std::errc{} || ptr != fname.data() + fname.size()) continue; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

        auto stat_line = read_file_line(entry.path() / "stat");
        if (stat_line.empty()) continue;

        // Parse /proc/[pid]/stat: "pid (comm) state ..."
        // comm can contain spaces/parens, so find last ')'.
        auto open  = stat_line.find('(');
        auto close = stat_line.rfind(')');
        if (open == std::string::npos || close == std::string::npos) continue;

        std::string comm = stat_line.substr(open + 1, close - open - 1);
        const char *rest = stat_line.c_str() + close + 2; // skip ") "

        // Fields after (comm): state ppid pgrp session tty_nr tpgid flags
        //   minflt cminflt majflt cmajflt utime stime ...
        // That's: field3=state, field14=utime, field15=stime
        char st    = 0;
        long utime = 0;
        long stime = 0;
        // NOLINTNEXTLINE(cert-err34-c)
        if (std::sscanf(rest,
                        "%c %*d %*d %*d %*d %*d %*u " // state .. flags
                        "%*lu %*lu %*lu %*lu "        // minflt..cmajflt
                        "%ld %ld",                    // utime stime
                        &st, &utime, &stime) < 3)
            continue;

        // RSS from /proc/[pid]/statm (field 2)
        long rss_pages = 0;
        auto statm     = read_file_line(entry.path() / "statm");
        if (!statm.empty()) {
            // NOLINTNEXTLINE(cert-err34-c)
            std::sscanf(statm.c_str(), "%*ld %ld", &rss_pages);
        }

        out.push_back({
            pid,
            std::move(comm),
            st,
            rss_pages * page_size,
            utime,
            stime,
        });
    }

    return out;
}

// ---------------------------------------------------------------------------
// Demo state
// ---------------------------------------------------------------------------

struct demo_state {
    theme::theme_manager themes;
    bool                 show_theme_editor = false;

    // property editor
    std::string name    = "Widget";
    double      value   = 3.14;
    float       scale   = 1.0f;
    int         count   = 42;
    bool        enabled = true;

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

    // context menu
    bool ctx_show_details = true;
    int  ctx_action_count = 0;

    // toast position
    int toast_pos_idx = 0;
};

// ---------------------------------------------------------------------------
// Sections
// ---------------------------------------------------------------------------

static const char *state_label(char st) {
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

static void section_table(demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Table Builder")) return;

    ImGui::TextWrapped("Live process list from /proc. Fluent table API with multi-column "
                       "sorting, Ctrl/Shift selection, and virtual clipping.");
    ImGui::Spacing();

    // Auto-refresh every 2 seconds
    bool   data_changed = false;
    double now          = ImGui::GetTime();
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
    iu::fmt_buf<48> stats{"{} processes, {} selected", s.processes.size(), s.selection.size()};
    ImGui::TextUnformatted(stats.c_str());
    ImGui::Spacing();

    auto table =
        iu::table_builder<process_row>{}
            .set_id("##procs")
            .set_flags(ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                       ImGuiTableFlags_BordersOuter | // NOLINT(hicpp-signed-bitwise)
                       ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                       ImGuiTableFlags_SortMulti)
            .set_scroll_freeze(0, 1)
            .set_selection(&s.selection)
            .set_row_id([](const process_row &r) { return r.pid; })
            .add_column("PID", 50, [](const process_row &r) { ImGui::Text("%d", r.pid); },
                        ImGuiTableColumnFlags_DefaultSort)
            .add_column("Name", 0, [](const process_row &r) { ImGui::TextUnformatted(r.name.c_str()); })
            .add_column("State", 70, [](const process_row &r) { ImGui::TextUnformatted(state_label(r.state)); })
            .add_column("RSS", 80, [](const process_row &r) {
        ImGui::Text("%s", iu::format_bytes(static_cast<std::uint64_t>(r.rss_bytes)).c_str());
    }).add_column("CPU time", 80, [](const process_row &r) {
        static const long tps   = sysconf(_SC_CLK_TCK);
        long              total = r.utime + r.stime;
        long              secs  = total / tps;
        long              frac  = (total % tps) * 100 / tps;
        ImGui::Text("%ld.%02lds", secs, frac);
    });

    if (table.begin(300)) {
        using cmp                         = iu::table_builder<process_row>::comparator_fn;
        constexpr std::array<cmp, 5> cmps = {
            [](const process_row &a, const process_row &b) { return a.pid < b.pid; },
            [](const process_row &a, const process_row &b) { return a.name < b.name; },
            [](const process_row &a, const process_row &b) { return a.state < b.state; },
            [](const process_row &a, const process_row &b) { return a.rss_bytes < b.rss_bytes; },
            [](const process_row &a, const process_row &b) { return (a.utime + a.stime) < (b.utime + b.stime); },
        };
        table.sort_if_dirty(s.processes, std::span<const cmp>{cmps}, data_changed);
        table.render_clipped(std::span<const process_row>{s.processes});
        table.end();
    }
}

static void section_property_editor(demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Property Editor")) return;

    ImGui::TextWrapped("Standardized label+input layout with type-specific helpers. "
                       "Consistent alignment without manual column math.");
    ImGui::Spacing();

    iu::property_text("Name", "##name", s.name);
    iu::property_double("Value", "##value", s.value);
    iu::property_float("Scale", "##scale", s.scale, "%.2f", 0.0f, 10.0f);
    iu::property_int("Count", "##count", s.count, 0, 1000);
    iu::property_checkbox("Enabled", "##enabled", s.enabled);
}

static void section_log_viewer(demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Log Viewer")) return;

    ImGui::TextWrapped("Ring-buffer log with level filtering, search, auto-scroll, "
                       "timestamps, and per-line copy. Handles burst writes efficiently.");
    ImGui::Spacing();

    static int msg_counter = 0;
    if (ImGui::Button("Info")) {
        push_log(iu::log_viewer::level::info, std::format("info message #{}", ++msg_counter));
    }
    ImGui::SameLine();
    if (ImGui::Button("Warning")) {
        push_log(iu::log_viewer::level::warning, std::format("warning message #{}", ++msg_counter));
    }
    ImGui::SameLine();
    if (ImGui::Button("Error")) {
        push_log(iu::log_viewer::level::error, std::format("error message #{}", ++msg_counter));
    }
    ImGui::SameLine();
    if (ImGui::Button("Burst x50")) {
        for (int i = 0; i < 50; ++i)
            push_log(iu::log_viewer::level::info, std::format("burst #{}", ++msg_counter));
    }

    s.log.render([](auto emit) {
        for (auto &e: g_pending)
            emit(e.lvl, e.text);
        g_pending.clear();
    }, "##log_search", "##log_child");
}

// ---------------------------------------------------------------------------
// New widget sections
// ---------------------------------------------------------------------------

static void section_progress_bar([[maybe_unused]] demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Progress Bar")) return;

    ImGui::TextWrapped("Semantic progress bar wrappers: plain, percentage-labeled, and custom-colored.");
    ImGui::Spacing();

    // Animate a progress value
    const float t = static_cast<float>(ImGui::GetTime());
    const float progress = std::fmod(t * 0.15f, 1.0f);

    iu::progress_bar(progress);
    ImGui::Spacing();
    iu::progress_bar_pct("Download", progress);
    iu::progress_bar_pct("Upload", std::fmod(t * 0.1f, 1.0f));
    ImGui::Spacing();

    ImGui::TextUnformatted("Colored variants:");
    iu::progress_bar_colored(0.3f, iu::colors::error, {-1, 0}, "Critical");
    iu::progress_bar_colored(0.6f, iu::colors::warning, {-1, 0}, "Warning");
    iu::progress_bar_colored(0.9f, iu::colors::success, {-1, 0}, "Healthy");
}

static void section_spinner([[maybe_unused]] demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Spinner")) return;

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

    ImGui::TextUnformatted("Determinate (progress arc):");
    const float t = static_cast<float>(ImGui::GetTime());
    const float p = std::fmod(t * 0.2f, 1.0f);
    ImGui::SameLine();
    iu::spinner_progress("##prog1", p, 10.0f, 2.5f);
    ImGui::SameLine();
    iu::spinner_progress("##prog2", 0.25f, 10.0f, 2.5f, iu::colors::error);
    ImGui::SameLine();
    iu::spinner_progress("##prog3", 0.75f, 10.0f, 2.5f, iu::colors::success);
}

static void section_toolbar(demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Toolbar")) return;

    ImGui::TextWrapped("Fluent toolbar builder with buttons, toggles, separators, and tooltips.");
    ImGui::Spacing();

    iu::toolbar()
        .button("New", [&] { iu::toast::show("New clicked"); }, "Create new item")
        .button("Open", [&] { iu::toast::show("Open clicked"); }, "Open existing item")
        .button("Save", [&] { iu::toast::show("Saved!", iu::toast::level::success); }, "Save current item")
        .separator()
        .toggle("Grid", &s.show_grid, "Toggle grid overlay")
        .toggle("Snap", &s.snap_enabled, "Toggle snap to grid")
        .render();

    ImGui::Spacing();
    iu::fmt_text("Grid: {}, Snap: {}", s.show_grid ? "ON" : "OFF", s.snap_enabled ? "ON" : "OFF");
}

static void section_context_menu(demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Context Menu")) return;

    ImGui::TextWrapped("Declarative context menu builder. Right-click the box below.");
    ImGui::Spacing();

    // A target area to right-click
    const ImVec2 box_size{300, 80};
    ImGui::Button("Right-click me##ctx_target", box_size);

    iu::context_menu("##demo_ctx")
        .item("Action A", [&] { ++s.ctx_action_count; iu::toast::show("Action A"); })
        .item("Action B", [&] { ++s.ctx_action_count; iu::toast::show("Action B"); })
        .separator()
        .checkbox("Show details", &s.ctx_show_details)
        .separator()
        .item("Disabled item", [] {}, false)
        .render();

    iu::fmt_text("Actions triggered: {}, Show details: {}", s.ctx_action_count, s.ctx_show_details ? "yes" : "no");
}

static void section_drag_drop(demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Drag & Drop")) return;

    ImGui::TextWrapped("Type-safe drag-drop helpers. Drag items to reorder.");
    ImGui::Spacing();

    for (int i = 0; i < static_cast<int>(s.dd_items.size()); ++i) {
        ImGui::Selectable(s.dd_items[i].c_str());

        // Drag source
        iu::drag_drop::source("DD_REORDER", i, s.dd_items[i].c_str());

        // Drop target
        if (const iu::drag_drop_target tgt{}) {
            if (auto src_idx = iu::drag_drop::accept_payload<int>("DD_REORDER")) {
                // Swap items
                if (*src_idx != i) {
                    std::swap(s.dd_items[*src_idx], s.dd_items[i]);
                }
            }
        }
    }
}

static void section_splitter(demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Splitter")) return;

    ImGui::TextWrapped("Resizable split panels. Drag the divider to resize.");
    ImGui::Spacing();

    const float panel_height = 150.0f;
    const float thickness = 8.0f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float usable = avail.x - thickness;

    // Left panel
    {
        const iu::child left{"##split_left", ImVec2(usable * s.split_ratio, panel_height), ImGuiChildFlags_Borders};
        ImGui::TextUnformatted("Left Panel");
        ImGui::Separator();
        for (int i = 0; i < 5; ++i)
            iu::fmt_text("Left item {}", i);
    }

    ImGui::SameLine(0, 0);
    (void)iu::splitter("##demo_split", iu::split_direction::horizontal, s.split_ratio, thickness);
    ImGui::SameLine(0, 0);

    // Right panel
    {
        const iu::child right{"##split_right", ImVec2(usable * (1.0f - s.split_ratio), panel_height), ImGuiChildFlags_Borders};
        ImGui::TextUnformatted("Right Panel");
        ImGui::Separator();
        for (int i = 0; i < 5; ++i)
            iu::fmt_text("Right item {}", i);
    }

    iu::fmt_text("Ratio: {:.0f}% / {:.0f}%", s.split_ratio * 100.0f, (1.0f - s.split_ratio) * 100.0f);
}

static void section_tab_bar([[maybe_unused]] demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Tab Bar Builder")) return;

    ImGui::TextWrapped("Declarative tab bar with fluent API.");
    ImGui::Spacing();

    iu::tab_bar_builder("##demo_tabs")
        .tab("General", [] {
            ImGui::TextUnformatted("General settings content goes here.");
            ImGui::Spacing();
            ImGui::BulletText("Option A");
            ImGui::BulletText("Option B");
            ImGui::BulletText("Option C");
        })
        .tab("Advanced", [] {
            ImGui::TextUnformatted("Advanced settings content.");
            ImGui::Spacing();
            static float val = 1.0f;
            ImGui::SliderFloat("Threshold##tab", &val, 0.0f, 10.0f);
        })
        .tab("About", [] {
            ImGui::TextUnformatted("imgui_util demo application.");
            iu::colored_text("Built with Dear ImGui + imgui_util", iu::colors::text_secondary);
        })
        .render();
}

static void section_modal(demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Modal Builder")) return;

    ImGui::TextWrapped("Fluent modal dialog builder with ok/cancel, danger mode, custom body, "
                       "and keyboard shortcuts (Enter/Escape).");
    ImGui::Spacing();

    if (ImGui::Button("Simple Confirm")) s.show_confirm_modal = true;
    ImGui::SameLine();
    if (ImGui::Button("Custom Body")) s.show_body_modal = true;

    // Simple confirmation dialog (danger mode)
    iu::modal_builder("Delete Item?")
        .message("Are you sure you want to delete this item? This action cannot be undone.")
        .ok_button("Delete", [&] { iu::toast::show("Deleted!", iu::toast::level::error); })
        .cancel_button("Cancel", [] {})
        .danger()
        .render(&s.show_confirm_modal);

    // Modal with custom body content
    iu::modal_builder("Adjust Settings")
        .body([&] {
            ImGui::TextUnformatted("Adjust the value below:");
            ImGui::SliderInt("##modal_slider", &s.modal_slider_val, 0, 100);
            iu::fmt_text("Current: {}", s.modal_slider_val);
        })
        .ok_button("Apply", [&] {
            iu::toast::show(std::format("Applied value: {}", s.modal_slider_val), iu::toast::level::success);
        })
        .cancel_button("Cancel", [] {})
        .size(350, 0)
        .render(&s.show_body_modal);
}

static void section_toast(demo_state &s) {
    if (!ImGui::CollapsingHeader("imgui_util: Toast Notifications")) return;

    ImGui::TextWrapped("Stackable toast notifications with fade-out, click-to-dismiss, "
                       "and configurable anchor position.");
    ImGui::Spacing();

    if (ImGui::Button("Info"))    iu::toast::show("This is an info toast.");
    ImGui::SameLine();
    if (ImGui::Button("Success")) iu::toast::show("Operation succeeded!", iu::toast::level::success);
    ImGui::SameLine();
    if (ImGui::Button("Warning")) iu::toast::show("Low disk space.", iu::toast::level::warning);
    ImGui::SameLine();
    if (ImGui::Button("Error"))   iu::toast::show("Connection failed!", iu::toast::level::error);
    ImGui::SameLine();
    if (ImGui::Button("Clear All")) iu::toast::clear();

    ImGui::Spacing();
    constexpr const char *positions[] = {"Bottom-Right", "Top-Right", "Bottom-Left", "Top-Left"};
    if (ImGui::Combo("Position", &s.toast_pos_idx, positions, 4)) {
        iu::toast::set_position(static_cast<iu::toast::position>(s.toast_pos_idx));
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    Log::setTimestamps(true);
    Log::autoDetectColors();

    glfwSetErrorCallback([](int, const char *msg) { Log::error("GLFW", msg); });
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    auto *window = glfwCreateWindow(1280, 800, "imgui_util demo", nullptr, nullptr);
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

    auto &io        = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    demo_state state;
    if (auto *preset = theme::theme_manager::find_preset("Obsidian"))
        state.themes.set_theme(theme::theme_config::from_preset(*preset));
    state.themes.get_current_theme().apply();

    Log::info("Demo", "imgui_util demo started");

    bool show_demo = true;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Standard ImGui demo window
        ImGui::ShowDemoWindow(&show_demo);

        // Append imgui_util sections into the same "Dear ImGui Demo" window.
        // ImGui allows multiple Begin() calls with the same name per frame —
        // content is appended to the existing window.
        if (ImGui::Begin("Dear ImGui Demo")) {
            ImGui::SeparatorText("imgui_util");

            if (ImGui::Button("Theme Editor")) state.show_theme_editor = !state.show_theme_editor;

            section_table(state);
            section_property_editor(state);
            section_log_viewer(state);
            section_progress_bar(state);
            section_spinner(state);
            section_toolbar(state);
            section_context_menu(state);
            section_drag_drop(state);
            section_splitter(state);
            section_tab_bar(state);
            section_modal(state);
            section_toast(state);
        }
        ImGui::End();

        // Theme editor as separate window
        if (state.show_theme_editor) state.themes.render_theme_editor(&state.show_theme_editor);

        // Toast notifications (render last, on top of everything)
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
