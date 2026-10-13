// 23 Interpreter — classic form (one class per grammar rule + a parser).
//
// Force: the rules change constantly and are written by operators, not
//        programmers; the set of *term types* and operators (the grammar)
//        is small and changes rarely. The evaluation engine must not be
//        edited when a new rule is saved.
//
// Roles
//   AbstractExpression     : Expr          — interpret(context)
//   TerminalExpression     : LabelIs, ZoneIs, MinConfidence, TimeWindow
//   NonterminalExpression  : And, Or, Not  — hold child Exprs, combine results
//   Context                : Event         — what the rule is evaluated against
//   Client / parser        : parse_rule()  — text → tree, once, at save time
//
// GoF's Interpreter covers the tree and interpret(); it says nothing about how
// the tree is built. In practice you always pair it with a small
// recursive-descent parser — one function per grammar rule, as below.

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "check.hpp"
#include "rules.hpp"

// ---- Abstract expression ---------------------------------------------------------
class Expr {
public:
    virtual ~Expr() = default;
    virtual bool interpret(const Event& e) const = 0;
    virtual std::string to_string() const = 0;  // second operation: show the parsed tree
};
using ExprPtr = std::unique_ptr<Expr>;

// ---- Terminal expressions (leaves) -------------------------------------------------
class LabelIs final : public Expr {
public:
    explicit LabelIs(std::string label) : label_(std::move(label)) {}
    bool interpret(const Event& e) const override { return e.label == label_; }
    std::string to_string() const override { return label_; }

private:
    std::string label_;
};

class ZoneIs final : public Expr {
public:
    explicit ZoneIs(std::string zone) : zone_(std::move(zone)) {}
    bool interpret(const Event& e) const override { return e.zone == zone_; }
    std::string to_string() const override { return "zone=" + zone_; }

private:
    std::string zone_;
};

class MinConfidence final : public Expr {
public:
    explicit MinConfidence(double min) : min_(min) {}
    bool interpret(const Event& e) const override { return e.confidence >= min_; }
    std::string to_string() const override { return "confidence>=" + std::format("{}", min_); }

private:
    double min_;
};

class TimeWindow final : public Expr {
public:
    explicit TimeWindow(Window w) : w_(w) {}
    bool interpret(const Event& e) const override { return in_window(w_, e.ts_ms); }
    std::string to_string() const override {
        return std::format("time {:02}:{:02}-{:02}:{:02}", w_.from / 60, w_.from % 60, w_.to / 60, w_.to % 60);
    }

private:
    Window w_;
};

// ---- Nonterminal expressions (composites) ------------------------------------------
class And final : public Expr {
public:
    And(ExprPtr l, ExprPtr r) : l_(std::move(l)), r_(std::move(r)) {}
    bool interpret(const Event& e) const override { return l_->interpret(e) && r_->interpret(e); }
    std::string to_string() const override { return "(" + l_->to_string() + " AND " + r_->to_string() + ")"; }

private:
    ExprPtr l_, r_;
};

class Or final : public Expr {
public:
    Or(ExprPtr l, ExprPtr r) : l_(std::move(l)), r_(std::move(r)) {}
    bool interpret(const Event& e) const override { return l_->interpret(e) || r_->interpret(e); }
    std::string to_string() const override { return "(" + l_->to_string() + " OR " + r_->to_string() + ")"; }

private:
    ExprPtr l_, r_;
};

class Not final : public Expr {
public:
    explicit Not(ExprPtr x) : x_(std::move(x)) {}
    bool interpret(const Event& e) const override { return !x_->interpret(e); }
    std::string to_string() const override { return "NOT " + x_->to_string(); }

private:
    ExprPtr x_;
};

// ---- Parser: one method per grammar rule (see rules.hpp) ---------------------------
class Parser {
public:
    explicit Parser(std::string_view text) : tokens_(tokenize(text)) {}

    ExprPtr parse() {
        auto e = parse_or();
        if (pos_ != tokens_.size()) fail("unexpected '" + tokens_[pos_] + "'");
        return e;
    }

private:
    ExprPtr parse_or() {
        auto left = parse_and();
        while (accept("OR")) left = std::make_unique<Or>(std::move(left), parse_and());
        return left;
    }
    ExprPtr parse_and() {
        auto left = parse_unary();
        while (accept("AND")) left = std::make_unique<And>(std::move(left), parse_unary());
        return left;
    }
    ExprPtr parse_unary() {
        if (accept("NOT")) return std::make_unique<Not>(parse_unary());
        if (accept("(")) {
            auto inner = parse_or();
            if (!accept(")")) fail("missing ')'");
            return inner;
        }
        return parse_term();
    }
    ExprPtr parse_term() {
        if (pos_ >= tokens_.size()) fail("unexpected end of rule");
        const std::string t = tokens_[pos_++];
        if (t == "time") {
            if (pos_ >= tokens_.size()) fail("'time' needs HH:MM-HH:MM");
            return std::make_unique<TimeWindow>(parse_window(tokens_[pos_++]));
        }
        if (t.starts_with("zone=")) return std::make_unique<ZoneIs>(t.substr(5));
        if (t.starts_with("confidence>=")) return std::make_unique<MinConfidence>(parse_number(t.substr(12)));
        if (t.find_first_of("=<>") != std::string::npos) fail("unknown field in '" + t + "'");
        if (t == "AND" || t == "OR" || t == ")") fail("expected a term before '" + t + "'");
        return std::make_unique<LabelIs>(t);
    }

    bool accept(std::string_view tok) {
        if (pos_ < tokens_.size() && tokens_[pos_] == tok) return ++pos_, true;
        return false;
    }
    [[noreturn]] static void fail(const std::string& why) { throw std::invalid_argument("rule: " + why); }

    std::vector<std::string> tokens_;
    std::size_t pos_ = 0;
};

ExprPtr parse_rule(std::string_view text) { return Parser(text).parse(); }

int count_hits(const Expr& rule, const std::vector<Event>& events) {
    int n = 0;
    for (const auto& e : events) n += rule.interpret(e);  // no string work per event
    return n;
}

bool rejects(std::string_view text) {
    try {
        parse_rule(text);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    const auto events = simulated_events();

    // Same numbers as naive.cpp for the rules both can express.
    CHECK_EQ(count_hits(*parse_rule(kNightEntrance), events), kNightEntranceHits);
    CHECK_EQ(count_hits(*parse_rule(kParkingVehicle), events), kParkingVehicleHits);
    CHECK_EQ(count_hits(*parse_rule(kConfidentDaytime), events), kConfidentDaytimeHits);

    // Rules the naive version cannot express.
    CHECK_EQ(count_hits(*parse_rule(kPeopleOrCarsOutsideParking), events), kPeopleOrCarsOutsideParkingHits);
    CHECK_EQ(count_hits(*parse_rule(kPrecedence), events), kPrecedenceHits);

    // AND binds tighter than OR — visible in the parsed tree.
    CHECK_EQ(parse_rule(kPrecedence)->to_string(), std::string("(animal OR (bicycle AND confidence>=0.9))"));

    // Errors are caught when the rule is saved, not silently at match time.
    CHECK(rejects("person AND zon=entrance"));
    CHECK(rejects("(person OR vehicle"));
    CHECK(rejects("person AND"));
    CHECK(rejects("time 25:00-06:00"));

    std::cout << "23-interpreter/pattern: night-entrance=" << kNightEntranceHits
              << " parking-vehicle=" << kParkingVehicleHits
              << " confident-daytime=" << kConfidentDaytimeHits
              << " people-or-cars-outside-parking=" << kPeopleOrCarsOutsideParkingHits
              << " precedence=" << kPrecedenceHits << "\n";
}
