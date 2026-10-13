// 23 Interpreter — modern form: the AST as a std::variant, operations as visitors.
//
// pattern.cpp puts each operation (interpret, to_string) inside every node
// class. That suits a grammar whose node types grow while operations stay
// fixed. Rule engines usually go the other way: the grammar is stable, but
// operations keep arriving — evaluate, pretty-print, list referenced fields
// (to validate against the schema), estimate cost, compile to SQL…
//
// With a variant AST:
//   - nodes are plain structs, no base class, no virtuals
//   - each operation is one function with one `overloaded` visitor; adding an
//     operation touches no node type
//   - std::visit fails to compile if a visitor misses a node type, so a new
//     node type is caught everywhere it must be handled
// Recursion goes through std::unique_ptr, because a variant cannot hold
// itself by value.

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "check.hpp"
#include "rules.hpp"

// ---- AST ---------------------------------------------------------------------------
struct LabelIs { std::string label; };
struct ZoneIs { std::string zone; };
struct MinConfidence { double min; };
struct TimeWindow { Window w; };
struct And;
struct Or;
struct Not;

using Node = std::variant<LabelIs, ZoneIs, MinConfidence, TimeWindow,
                          std::unique_ptr<And>, std::unique_ptr<Or>, std::unique_ptr<Not>>;

struct And { Node l, r; };
struct Or { Node l, r; };
struct Not { Node x; };

template <class... F>
struct overloaded : F... { using F::operator()...; };

// ---- Operation 1: evaluate ---------------------------------------------------------
bool eval(const Node& n, const Event& e) {
    return std::visit(overloaded{
        [&](const LabelIs& t) { return e.label == t.label; },
        [&](const ZoneIs& t) { return e.zone == t.zone; },
        [&](const MinConfidence& t) { return e.confidence >= t.min; },
        [&](const TimeWindow& t) { return in_window(t.w, e.ts_ms); },
        [&](const std::unique_ptr<And>& t) { return eval(t->l, e) && eval(t->r, e); },
        [&](const std::unique_ptr<Or>& t) { return eval(t->l, e) || eval(t->r, e); },
        [&](const std::unique_ptr<Not>& t) { return !eval(t->x, e); },
    }, n);
}

// ---- Operation 2: which fields does a rule read? (added without touching any node)
void fields(const Node& n, std::set<std::string>& out) {
    std::visit(overloaded{
        [&](const LabelIs&) { out.insert("label"); },
        [&](const ZoneIs&) { out.insert("zone"); },
        [&](const MinConfidence&) { out.insert("confidence"); },
        [&](const TimeWindow&) { out.insert("time"); },
        [&](const std::unique_ptr<And>& t) { fields(t->l, out), fields(t->r, out); },
        [&](const std::unique_ptr<Or>& t) { fields(t->l, out), fields(t->r, out); },
        [&](const std::unique_ptr<Not>& t) { fields(t->x, out); },
    }, n);
}

// ---- Parser: same grammar as pattern.cpp, building variant nodes --------------------
class Parser {
public:
    explicit Parser(std::string_view text) : tokens_(tokenize(text)) {}

    Node parse() {
        Node n = parse_or();
        if (pos_ != tokens_.size()) fail("unexpected '" + tokens_[pos_] + "'");
        return n;
    }

private:
    Node parse_or() {
        Node left = parse_and();
        while (accept("OR")) left = std::make_unique<Or>(Or{std::move(left), parse_and()});
        return left;
    }
    Node parse_and() {
        Node left = parse_unary();
        while (accept("AND")) left = std::make_unique<And>(And{std::move(left), parse_unary()});
        return left;
    }
    Node parse_unary() {
        if (accept("NOT")) return std::make_unique<Not>(Not{parse_unary()});
        if (accept("(")) {
            Node inner = parse_or();
            if (!accept(")")) fail("missing ')'");
            return inner;
        }
        return parse_term();
    }
    Node parse_term() {
        if (pos_ >= tokens_.size()) fail("unexpected end of rule");
        const std::string t = tokens_[pos_++];
        if (t == "time") {
            if (pos_ >= tokens_.size()) fail("'time' needs HH:MM-HH:MM");
            return TimeWindow{parse_window(tokens_[pos_++])};
        }
        if (t.starts_with("zone=")) return ZoneIs{t.substr(5)};
        if (t.starts_with("confidence>=")) return MinConfidence{parse_number(t.substr(12))};
        if (t.find_first_of("=<>") != std::string::npos) fail("unknown field in '" + t + "'");
        if (t == "AND" || t == "OR" || t == ")") fail("expected a term before '" + t + "'");
        return LabelIs{t};
    }

    bool accept(std::string_view tok) {
        if (pos_ < tokens_.size() && tokens_[pos_] == tok) return ++pos_, true;
        return false;
    }
    [[noreturn]] static void fail(const std::string& why) { throw std::invalid_argument("rule: " + why); }

    std::vector<std::string> tokens_;
    std::size_t pos_ = 0;
};

int count_hits(std::string_view text, const std::vector<Event>& events) {
    const Node rule = Parser(text).parse();  // parse once
    int n = 0;
    for (const auto& e : events) n += eval(rule, e);
    return n;
}

int main() {
    const auto events = simulated_events();

    CHECK_EQ(count_hits(kNightEntrance, events), kNightEntranceHits);
    CHECK_EQ(count_hits(kParkingVehicle, events), kParkingVehicleHits);
    CHECK_EQ(count_hits(kConfidentDaytime, events), kConfidentDaytimeHits);
    CHECK_EQ(count_hits(kPeopleOrCarsOutsideParking, events), kPeopleOrCarsOutsideParkingHits);
    CHECK_EQ(count_hits(kPrecedence, events), kPrecedenceHits);

    std::set<std::string> used;
    fields(Parser(kNightEntrance).parse(), used);
    CHECK((used == std::set<std::string>{"label", "time", "zone"}));

    std::cout << "23-interpreter/modern: variant AST agrees on all 5 rules; night-entrance reads "
              << used.size() << " fields\n";
}
