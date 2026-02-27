/// @file notification_center.hpp
/// @brief Persistent notification history panel with severity and actions.
///
/// Stores notifications persistently (unlike ephemeral toasts). Uses severity.hpp enum
/// and fmt_buf for relative timestamp formatting.
///
/// Usage:
/// @code
///   imgui_util::notification_center::push("Build complete", "All 42 tests passed",
///                                          imgui_util::severity::success);
///   imgui_util::notification_center::push("Error", "Connection lost", imgui_util::severity::error,
///                                          "Retry", [&]{ reconnect(); });
///
///   // Render the panel (typically in a side panel or popup):
///   static bool open = true;
///   imgui_util::notification_center::render_panel("##notifications", &open);
///
///   // Badge display:
///   if (int n = imgui_util::notification_center::unread_count(); n > 0) { ... show badge ... }
/// @endcode
#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <imgui.h>
#include <string>
#include <utility>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"
#include "imgui_util/theme/dynamic_colors.hpp"
#include "imgui_util/widgets/severity.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util::notification_center {

    /// @brief A single persistent notification entry.
    struct notification {
        std::string                           title;
        std::string                           detail;
        severity                              sev = severity::info;
        std::string                           action_label;
        std::move_only_function<void()>       action;
        std::chrono::steady_clock::time_point timestamp;
        bool                                  read = false;
    };

    namespace detail {

        struct state {
            std::vector<notification> entries;
        };

        [[nodiscard]] inline auto &get_state() noexcept {
            static state s;
            return s;
        }

        /// @brief Return a short text icon for the given severity (e.g. "[i]", "[!]").
        [[nodiscard]] inline const char *severity_icon(const severity sev) noexcept {
            switch (sev) {
                case severity::info:
                    return "[i]";
                case severity::warning:
                    return "[!]";
                case severity::error:
                    return "[x]";
                case severity::success:
                    return "[+]";
            }
            return "[i]";
        }

        [[nodiscard]] inline ImVec4 severity_color(const severity sev) noexcept {
            switch (sev) {
                case severity::info:
                    return theme::info_color();
                case severity::warning:
                    return theme::warning_color();
                case severity::error:
                    return theme::error_color();
                case severity::success:
                    return theme::success_color();
            }
            return theme::info_color();
        }

        /// @brief Format a time_point as a human-readable relative duration (e.g. "5m ago").
        [[nodiscard]] inline fmt_buf<32> relative_time(const std::chrono::steady_clock::time_point then) {
            const auto now  = std::chrono::steady_clock::now();
            const auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - then).count();

            if (diff < 60) return fmt_buf<32>("{}s ago", diff);
            if (diff < 3600) return fmt_buf<32>("{}m ago", diff / 60);
            if (diff < 86400) return fmt_buf<32>("{}h ago", diff / 3600);
            return fmt_buf<32>("{}d ago", diff / 86400);
        }

        inline void render_toolbar(std::vector<notification> &entries) {
            if (ImGui::Button("Mark All Read")) {
                for (auto &e: entries)
                    e.read = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear All")) {
                entries.clear();
            }
            ImGui::SameLine();
            {
                const fmt_buf<32> badge("{} unread", static_cast<int>(std::ranges::count_if(entries, [](const auto &e) {
                    return !e.read;
                })));
                dim_text(badge.sv());
            }
        }

        /// @brief Render a single notification row. Returns true if @p entries was mutated (dismiss).
        inline bool render_notification_row(notification &e, int row, const std::size_t idx,
                                            std::vector<notification> &entries) {
            const id entry_id{row};

            // Unread indicator: slightly brighter background
            if (!e.read) {
                const auto  pos   = ImGui::GetCursorScreenPos();
                const float avail = ImGui::GetContentRegionAvail().x;
                auto       *dl    = ImGui::GetWindowDrawList();
                dl->AddRectFilled(pos, {pos.x + avail, pos.y + ImGui::GetTextLineHeightWithSpacing() * 2.5f},
                                  IM_COL32(255, 255, 255, 8));
            }

            // Severity icon + title line
            const auto col = severity_color(e.sev);
            colored_text(severity_icon(e.sev), col);
            ImGui::SameLine();

            colored_text(e.title, e.read ? colors::text_secondary : colors::text_primary);

            ImGui::SameLine();
            {
                const auto ts = relative_time(e.timestamp);
                dim_text(ts.sv());
            }

            // Detail text
            if (!e.detail.empty()) {
                secondary_text(e.detail);
            }

            // Action button + dismiss
            if (!e.action_label.empty() && e.action) {
                if (ImGui::SmallButton(e.action_label.c_str())) {
                    e.action();
                    e.read = true;
                }
                ImGui::SameLine();
            }

            {
                const fmt_buf<32> dismiss_label("Dismiss##d{}", row);
                if (ImGui::SmallButton(dismiss_label.c_str())) {
                    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(idx));
                    return true; // iterator invalidated
                }
            }

            // Mark as read on hover
            if (ImGui::IsItemHovered()) {
                e.read = true;
            }

            ImGui::Separator();
            return false;
        }

    } // namespace detail

    /// @brief Return the number of unread notifications.
    [[nodiscard]] inline int unread_count() noexcept {
        const auto &entries = detail::get_state().entries;
        return static_cast<int>(std::ranges::count_if(entries, [](const auto &e) { return !e.read; }));
    }

    /**
     * @brief Push a persistent notification into the center.
     * @param title        Notification heading.
     * @param detail_text  Body text shown below the title.
     * @param sev          Severity level (controls icon and color).
     * @param action_label Optional button label (empty to omit).
     * @param action       Callback invoked when the action button is clicked.
     */
    inline void push(std::string title, std::string detail_text, const severity sev = severity::info,
                     std::string action_label = {}, std::move_only_function<void()> action = {}) {
        detail::get_state().entries.push_back({
            .title        = std::move(title),
            .detail       = std::move(detail_text),
            .sev          = sev,
            .action_label = std::move(action_label),
            .action       = std::move(action),
            .timestamp    = std::chrono::steady_clock::now(),
            .read         = false,
        });
    }

    /**
     * @brief Render the notification panel (toolbar + scrollable list, newest first).
     * @param panel_id ImGui window ID.
     * @param open     Optional pointer to a bool controlling window visibility.
     */
    inline void render_panel(const char *panel_id, bool *open = nullptr) {
        auto &entries = detail::get_state().entries;

        if (const window win{panel_id, open}) {
            detail::render_toolbar(entries);
            ImGui::Separator();

            // Scrollable list (newest first)
            if (const child child_scope{"##notif_list"}) {
                ImGuiListClipper clipper;
                const int        count = static_cast<int>(entries.size());
                clipper.Begin(count);

                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                        const auto idx = static_cast<std::size_t>(count - 1 - row);

                        if (auto &e = entries[idx]; detail::render_notification_row(e, row, idx, entries))
                            break; // iterator invalidated by dismiss
                    }
                }
            }
        }
    }

    /// @brief Mark every notification as read.
    inline void mark_all_read() noexcept {
        for (auto &e: detail::get_state().entries)
            e.read = true;
    }

    /// @brief Remove a notification by index.
    inline void dismiss(const std::size_t index) {
        if (auto &entries = detail::get_state().entries; index < entries.size()) {
            entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    /// @brief Remove all notifications.
    inline void clear_all() noexcept {
        detail::get_state().entries.clear();
    }

} // namespace imgui_util::notification_center
