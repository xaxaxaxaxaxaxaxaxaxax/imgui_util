// search_bar.hpp - case-insensitive search bar widget and string matching utilities
//
// Usage:
//   static imgui_util::search::search_bar<128> bar;
//   bar.render("Filter...", 200.0f);
//   for (auto& item : items)
//       if (bar.matches(item.name, item.desc)) { ... render item ... }
//
// search_bar owns a fixed-size char buffer and renders an InputText with a clear button.
// Free functions contains_ignore_case() and matches_any() work standalone too.
#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <imgui.h>
#include <string_view>

namespace imgui_util::search {

    // Case-insensitive character comparison
    [[nodiscard]] constexpr bool char_equal_ignore_case(const char a, const char b) noexcept {
        auto to_lower = [](const char c) -> char {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
        };
        return to_lower(a) == to_lower(b);
    }

    // Case-insensitive substring check using std::ranges
    [[nodiscard]] constexpr bool contains_ignore_case(std::string_view haystack, std::string_view needle) noexcept {
        return std::ranges::contains_subrange(haystack, needle, char_equal_ignore_case);
    }

    // Match query against multiple string fields (short-circuits on first match)
    template<typename... StringViews>
        requires(std::convertible_to<StringViews, std::string_view> && ...)
    [[nodiscard]] constexpr bool matches_any(std::string_view query, StringViews... fields) noexcept {
        if (query.empty()) return true;
        return (contains_ignore_case(fields, query) || ...);
    }

    // Reusable search bar state and rendering
    template<std::size_t BufferSize = 128>
    class search_bar {
        static_assert(BufferSize > 1, "BufferSize must be > 1 to hold at least one character plus null terminator");

    public:
        // Render the search input. Returns true if query changed this frame.
        // @param hint   Placeholder text shown when empty.
        // @param width  Widget width (-1 = fill remaining).
        // @param id     ImGui ID string (default "##search"). Override to avoid ID collisions.
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

            return changed;
        }

        // Get current query as string_view (empty if no search active)
        [[nodiscard]] constexpr std::string_view query() const noexcept {
            return std::string_view{buffer_.data(), len_};
        }

        // Check if query is empty (show all items)
        [[nodiscard]] constexpr bool empty() const noexcept { return len_ == 0; }

        // Check if an item matches the current query
        template<typename... StringViews>
            requires(std::convertible_to<StringViews, std::string_view> && ...)
        [[nodiscard]] bool matches(StringViews... fields) const noexcept {
            return matches_any(query(), fields...);
        }

        // Clear the search buffer
        constexpr void clear() noexcept {
            buffer_[0] = '\0';
            len_       = 0;
        }

        // Set the query programmatically
        void set_query(const std::string_view q) noexcept {
            const auto n = std::min(q.size(), BufferSize - 1);
            std::ranges::copy_n(q.data(), static_cast<std::ptrdiff_t>(n), buffer_.data());
            buffer_[n] = '\0';
            len_       = n;
        }

        // Request focus on next render
        constexpr void focus() noexcept { focus_next_frame_ = true; }

        // Clear and request focus (for dialog open)
        constexpr void reset() noexcept {
            clear();
            focus();
        }

    private:
        std::array<char, BufferSize> buffer_{};
        std::size_t                  len_              = 0;
        bool                         focus_next_frame_ = false;
    };

} // namespace imgui_util::search
