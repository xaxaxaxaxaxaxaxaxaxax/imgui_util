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
#include <ranges>
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
            int                       unread_count = 0;
        };

        [[nodiscard]] inline auto &get_state() noexcept {
            static state s;
            return s;
        }

        inline void decrement_unread_if(state &s, const notification &e) noexcept {
            if (!e.read) {
                --s.unread_count;
            }
        }

        /// @brief Return a short text icon for the given severity (e.g. "[i]", "[!]").
        [[nodiscard]] constexpr const char *severity_icon(const severity sev) noexcept {
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
            std::unreachable();
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
            std::unreachable();
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

        inline void render_toolbar(state &s) {
            if (ImGui::Button("Mark All Read")) {
                for (auto &e: s.entries)
                    e.read = true;
                s.unread_count = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear All")) {
                s.entries.clear();
                s.unread_count = 0;
            }
            ImGui::SameLine();
            {
                const fmt_buf<32> badge("{} unread", s.unread_count);
                dim_text(badge.sv());
            }
        }

        /// @brief Render a single notification row. Returns true if the user clicked dismiss.
        inline bool render_notification_row(notification &e, int row, const std::size_t /*idx*/, state &s) {
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
                    if (!e.read) {
                        e.read = true;
                        --s.unread_count;
                    }
                }
                ImGui::SameLine();
            }

            {
                const fmt_buf<32> dismiss_label("Dismiss##d{}", row);
                if (ImGui::SmallButton(dismiss_label.c_str())) {
                    return true;
                }
            }

            // Mark as read on hover
            if (ImGui::IsItemHovered() && !e.read) {
                e.read = true;
                --s.unread_count;
            }

            ImGui::Separator();
            return false;
        }

    } // namespace detail

    /// @brief Return the number of unread notifications.
    [[nodiscard]] inline int unread_count() noexcept {
        return detail::get_state().unread_count;
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
        auto &[entries, unread_count] = detail::get_state();
        entries.push_back({
            .title        = std::move(title),
            .detail       = std::move(detail_text),
            .sev          = sev,
            .action_label = std::move(action_label),
            .action       = std::move(action),
            .timestamp    = std::chrono::steady_clock::now(),
            .read         = false,
        });
        ++unread_count;
    }

    /**
     * @brief Render the notification panel (toolbar + scrollable list, newest first).
     * @param panel_id ImGui window ID.
     * @param open     Optional pointer to a bool controlling window visibility.
     */
    inline void render_panel(const char *panel_id, bool *open = nullptr) {
        auto &s       = detail::get_state();
        auto &entries = s.entries;

        if (const window win{panel_id, open}) {
            detail::render_toolbar(s);
            ImGui::Separator();

            // Scrollable list (newest first)
            if (const child child_scope{"##notif_list"}) {
                const int                count = static_cast<int>(entries.size());
                std::vector<std::size_t> to_dismiss;

                for (int row = 0; row < count; ++row) {
                    const auto idx = static_cast<std::size_t>(count - 1 - row);

                    if (auto &e = entries[idx]; detail::render_notification_row(e, row, idx, s))
                        to_dismiss.push_back(idx);
                }

                for (const auto idx: std::ranges::reverse_view(to_dismiss)) {
                    detail::decrement_unread_if(s, entries[idx]);
                    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(idx));
                }
            }
        }
    }

    /// @brief Mark every notification as read.
    inline void mark_all_read() noexcept {
        auto &[entries, unread_count] = detail::get_state();
        for (auto &e: entries)
            e.read = true;
        unread_count = 0;
    }

    /// @brief Remove a notification by index.
    inline void dismiss(const std::size_t index) {
        if (auto &s = detail::get_state(); index < s.entries.size()) {
            detail::decrement_unread_if(s, s.entries[index]);
            s.entries.erase(s.entries.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    /// @brief Remove all notifications.
    inline void clear_all() noexcept {
        auto &[entries, unread_count] = detail::get_state();
        entries.clear();
        unread_count = 0;
    }

} // namespace imgui_util::notification_center
