/// @file search_bar.hpp
/// @brief Case-insensitive search bar widget and string matching utilities.
///
/// search_bar owns a fixed-size char buffer and renders an InputText with a clear button.
/// Free functions contains_ignore_case() and matches_any() work standalone too.
///
/// Usage:
/// @code
///   static imgui_util::search::search_bar<128> bar;
///   bar.render("Filter...", 200.0f);
///   for (auto& item : items)
///       if (bar.matches(item.name, item.desc)) { ... render item ... }
/// @endcode
#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <imgui.h>
#include <optional>
#include <string_view>

#include "imgui_util/core/fmt_buf.hpp"

namespace imgui_util::search {

    [[nodiscard]] constexpr bool char_equal_ignore_case(const char a, const char b) noexcept {
        constexpr auto to_lower = [](const char c) constexpr noexcept -> char {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
        };
        return to_lower(a) == to_lower(b);
    }

    /// @brief Check whether @p haystack contains @p needle (case-insensitive).
    [[nodiscard]] constexpr bool contains_ignore_case(std::string_view haystack, std::string_view needle) noexcept {
        return std::ranges::contains_subrange(haystack, needle, char_equal_ignore_case);
    }

    /**
     * @brief Match a query against multiple string fields (short-circuits on first match).
     * @param query  Search string (empty query matches everything).
     * @param fields String fields to search against.
     * @return True if @p query is empty or any field contains it (case-insensitive).
     */
    template<typename... StringViews>
        requires(std::convertible_to<StringViews, std::string_view> && ...)
    [[nodiscard]] constexpr bool matches_any(std::string_view query, StringViews... fields) noexcept {
        if (query.empty()) return true;
        return (contains_ignore_case(fields, query) || ...);
    }

    /**
     * @brief Search bar widget with InputText, clear button, and case-insensitive matching.
     * @tparam BufferSize Size of the internal character buffer.
     */
    template<std::size_t BufferSize = 128>
    class search_bar {
        static_assert(BufferSize > 1, "BufferSize must be > 1 to hold at least one character plus null terminator");

    public:
        /**
         * @brief Render the search input. Returns true if the query changed this frame.
         * @param hint  Placeholder text shown when empty.
         * @param width Widget width (-1 = fill remaining).
         * @param id    ImGui ID string (default "##search"). Override to avoid ID collisions.
         * @return True if the query text changed.
         */
        [[nodiscard]] bool render(const char *hint = "Search...", const float width = -1.0f,
                                  const char *id = "##search") noexcept {
            if (focus_next_frame_) {
                ImGui::SetKeyboardFocusHere();
                focus_next_frame_ = false;
            }

            ImGui::SetNextItemWidth(width > 0.0f ? width : -1.0f);

            const bool changed = ImGui::InputTextWithHint(id, hint, buffer_.data(), buffer_.size());
            if (changed) {
                len_ = std::char_traits<char>::length(buffer_.data());
            }

            if (len_ > 0) {
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) {
                    clear();
                    return true;
                }
            }

            if (result_count_.has_value()) {
                ImGui::SameLine();
                const fmt_buf<32> badge("{} results", *result_count_);
                ImGui::TextDisabled("%s", badge.c_str());
            }

            return changed;
        }

        [[nodiscard]] constexpr std::string_view query() const noexcept {
            return std::string_view{buffer_.data(), len_};
        }

        [[nodiscard]] constexpr bool empty() const noexcept { return len_ == 0; }

        /**
         * @brief Test whether any of the given fields match the current query.
         * @param fields String fields to search against.
         * @return True if the query is empty or any field contains it.
         */
        template<typename... StringViews>
            requires(std::convertible_to<StringViews, std::string_view> && ...)
        [[nodiscard]] bool matches(StringViews... fields) const noexcept {
            return matches_any(query(), fields...);
        }

        constexpr void clear() noexcept {
            buffer_[0] = '\0';
            len_       = 0;
        }

        /// @brief Replace the current query text.
        void set_query(const std::string_view q) noexcept {
            const auto n = std::min(q.size(), BufferSize - 1);
            std::ranges::copy_n(q.data(), static_cast<std::ptrdiff_t>(n), buffer_.data());
            buffer_[n] = '\0';
            len_       = n;
        }

        /// @brief Request keyboard focus on the next frame.
        constexpr void focus() noexcept { focus_next_frame_ = true; }

        /// @brief Clear the query and request keyboard focus.
        constexpr void reset() noexcept {
            clear();
            focus();
        }

        /// @brief Set the result count badge. Pass std::nullopt to hide.
        void set_result_count(const std::optional<std::size_t> count) noexcept { result_count_ = count; }

    private:
        std::array<char, BufferSize> buffer_{};
        std::size_t                  len_              = 0;
        bool                         focus_next_frame_ = false;
        std::optional<std::size_t>   result_count_;
    };

} // namespace imgui_util::search
