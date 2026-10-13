// 12 Proxy — modern form: generic proxies as class templates.
//
// GoF calls one kind of proxy a "smart reference": an object that stands in
// for a pointer and does extra work on access. std::unique_ptr and
// std::shared_ptr are exactly that. The same idea gives reusable proxies that
// need no Camera interface at all:
//
//   Lazy<T>      — virtual proxy as a smart pointer: operator-> creates T on
//                  first use. Works for any T: a camera, a DB connection, a
//                  model loaded onto a GPU.
//   TtlCache<V>  — caching proxy for any "fetch a value at time t" call.
//
// Trade-off vs pattern.cpp: no inheritance and fully reusable, but a Lazy<T>
// is not a T — you cannot pass it where a Camera& is expected. The classic
// form is substitutable; this one is composable.

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "check.hpp"
#include "fixture.hpp"

// A plain device class: no base, no virtuals.
class RealCamera {
public:
    RealCamera(std::string host, Stats& stats) : host_(std::move(host)), stats_(stats) { ++stats_.connects; }
    std::string snapshot(gw::TimestampMs now) {
        ++stats_.fetches;
        return jpeg_at(now);
    }

private:
    std::string host_;
    Stats& stats_;
};

template <class T>
class Lazy {
public:
    explicit Lazy(std::function<std::unique_ptr<T>()> make) : make_(std::move(make)) {}

    T* operator->() {
        if (!obj_) obj_ = make_();
        return obj_.get();
    }
    T& operator*() { return *operator->(); }
    bool created() const { return obj_ != nullptr; }

private:
    std::function<std::unique_ptr<T>()> make_;
    std::unique_ptr<T> obj_;
};

template <class V>
class TtlCache {
public:
    TtlCache(std::function<V(gw::TimestampMs)> fetch, gw::TimestampMs ttl)
        : fetch_(std::move(fetch)), ttl_(ttl) {}

    const V& get(gw::TimestampMs now) {
        if (!value_ || now - at_ >= ttl_) {
            value_ = fetch_(now);
            at_ = now;
        }
        return *value_;
    }

private:
    std::function<V(gw::TimestampMs)> fetch_;
    gw::TimestampMs ttl_;
    std::optional<V> value_;
    gw::TimestampMs at_ = 0;
};

int main() {
    Stats stats;
    Lazy<RealCamera> cam1([&] { return std::make_unique<RealCamera>("10.0.0.11", stats); });
    Lazy<RealCamera> cam2([&] { return std::make_unique<RealCamera>("10.0.0.12", stats); });
    CHECK(!cam1.created());

    TtlCache<std::string> snapshots([&](gw::TimestampMs t) { return cam1->snapshot(t); }, kSnapshotTtl);
    CHECK(snapshots.get(0) == jpeg_at(0));
    CHECK(snapshots.get(500) == jpeg_at(0));
    CHECK(snapshots.get(1500) == jpeg_at(1500));

    CHECK(cam1.created());
    CHECK(!cam2.created());
    CHECK_EQ(stats.connects, kConnects);
    CHECK_EQ(stats.fetches, kFetches);

    std::cout << "12-proxy/modern: Lazy<T> and TtlCache<V> match pattern.cpp\n";
}
