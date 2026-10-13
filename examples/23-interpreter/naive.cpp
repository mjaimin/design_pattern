// 23 Interpreter — the naive version.
//
// Rules arrive as text from the UI. The direct approach: split the rule string
// on every event and check each token, assuming the terms are joined by AND.
//
// Where it hurts — the moment an operator asks for
//     (person OR vehicle) AND NOT zone=parking
//   1. OR, NOT and parentheses need precedence and nesting. A flat token loop
//      cannot express them; you end up rewriting it as a parser anyway.
//   2. The rule is re-tokenized for every event: string work on the hot path.
//   3. Errors surface late or never. A typo like "zon=entrance" is not
//      recognised as a term, so it falls through as a *label* and the rule
//      silently never matches — no error at save time.
//   4. Every new term type is another branch in this one function.

#include <string_view>

#include "check.hpp"
#include "rules.hpp"

bool matches(std::string_view rule, const Event& e) {
    const auto tokens = tokenize(rule);  // again, for every event
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string_view t = tokens[i];
        if (t == "AND") continue;
        if (t == "time") {
            if (i + 1 >= tokens.size() || !in_window(parse_window(tokens[++i]), e.ts_ms)) return false;
        } else if (t.starts_with("zone=")) {
            if (t.substr(5) != e.zone) return false;
        } else if (t.starts_with("confidence>=")) {
            if (e.confidence < parse_number(t.substr(12))) return false;
        } else if (t != e.label) {  // anything else is taken to be a label
            return false;
        }
    }
    return true;
}

int count_hits(std::string_view rule, const std::vector<Event>& events) {
    int n = 0;
    for (const auto& e : events) n += matches(rule, e);
    return n;
}

int main() {
    const auto events = simulated_events();

    CHECK_EQ(count_hits(kNightEntrance, events), kNightEntranceHits);
    CHECK_EQ(count_hits(kParkingVehicle, events), kParkingVehicleHits);
    CHECK_EQ(count_hits(kConfidentDaytime, events), kConfidentDaytimeHits);

    // The typo is accepted and the rule silently matches nothing.
    CHECK_EQ(count_hits("person AND zon=entrance", events), 0);

    std::cout << "23-interpreter/naive: night-entrance=" << kNightEntranceHits
              << " parking-vehicle=" << kParkingVehicleHits
              << " confident-daytime=" << kConfidentDaytimeHits << " (OR/NOT unsupported)\n";
}
