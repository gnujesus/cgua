#ifndef CGUA_LEVENSHTEIN_H
#define CGUA_LEVENSHTEIN_H

#include <algorithm>
#include <string_view>
#include <vector>

namespace Levenshtein {

// Bottom-up DP, two rolling rows — O(m*n) time, O(min(m,n)) space.
// Swapping so that b is always the shorter string keeps the inner
// vector allocation at O(min(m,n)) rather than O(max(m,n)).
inline std::size_t distance(std::string_view a, std::string_view b) {
    if (a.size() < b.size()) std::swap(a, b);

    const std::size_t m = a.size();
    const std::size_t n = b.size();

    // prev[j] = edit distance for a[0..i-1], b[0..j-1] from the previous row
    std::vector<std::size_t> prev(n + 1), curr(n + 1);

    for (std::size_t j = 0; j <= n; ++j) prev[j] = j;  // base case: delete all of b

    for (std::size_t i = 1; i <= m; ++i) {
        curr[0] = i;  // base case: delete all of a[0..i-1]
        for (std::size_t j = 1; j <= n; ++j) {
            std::size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({
                prev[j]     + 1,     // deletion
                curr[j - 1] + 1,     // insertion
                prev[j - 1] + cost   // substitution
            });
        }
        std::swap(prev, curr);
    }

    return prev[n];  // after the last swap, prev holds the final row
}

} // namespace Levenshtein

#endif // CGUA_LEVENSHTEIN_H
