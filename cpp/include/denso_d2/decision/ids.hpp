#pragma once

#include <functional>
#include <string>

namespace denso_d2::decision {

// Strong identifiers: wrap a string so mixing an action id with a
// constraint id or a resource id is a compile error, while ids stay
// stable, comparable, and loggable.
struct ActionId {
    std::string value;
};

struct ConstraintId {
    std::string value;
};

struct ResourceId {
    std::string value;
};

struct LocationId {
    std::string value;
};

struct RouteId {
    std::string value;
};

struct MaterialId {
    std::string value;
};

struct TransportTaskId {
    std::string value;

    friend bool operator==(const TransportTaskId&, const TransportTaskId&) = default;
    friend bool operator!=(const TransportTaskId&, const TransportTaskId&) = default;
    friend bool operator<(const TransportTaskId& lhs, const TransportTaskId& rhs) {
        return lhs.value < rhs.value;
    }
};

struct AmrId {
    std::string value;

    friend bool operator==(const AmrId&, const AmrId&) = default;
    friend bool operator!=(const AmrId&, const AmrId&) = default;
    friend bool operator<(const AmrId& lhs, const AmrId& rhs) {
        return lhs.value < rhs.value;
    }
};

inline bool operator==(const ActionId& a, const ActionId& b) { return a.value == b.value; }
inline bool operator!=(const ActionId& a, const ActionId& b) { return !(a == b); }
inline bool operator<(const ActionId& a, const ActionId& b) { return a.value < b.value; }

inline bool operator==(const ConstraintId& a, const ConstraintId& b) { return a.value == b.value; }
inline bool operator!=(const ConstraintId& a, const ConstraintId& b) { return !(a == b); }
inline bool operator<(const ConstraintId& a, const ConstraintId& b) { return a.value < b.value; }

inline bool operator==(const ResourceId& a, const ResourceId& b) { return a.value == b.value; }
inline bool operator!=(const ResourceId& a, const ResourceId& b) { return !(a == b); }
inline bool operator<(const ResourceId& a, const ResourceId& b) { return a.value < b.value; }

inline bool operator==(const LocationId& a, const LocationId& b) { return a.value == b.value; }
inline bool operator!=(const LocationId& a, const LocationId& b) { return !(a == b); }
inline bool operator<(const LocationId& a, const LocationId& b) { return a.value < b.value; }

inline bool operator==(const RouteId& a, const RouteId& b) { return a.value == b.value; }
inline bool operator!=(const RouteId& a, const RouteId& b) { return !(a == b); }
inline bool operator<(const RouteId& a, const RouteId& b) { return a.value < b.value; }

inline bool operator==(const MaterialId& a, const MaterialId& b) { return a.value == b.value; }
inline bool operator!=(const MaterialId& a, const MaterialId& b) { return !(a == b); }
inline bool operator<(const MaterialId& a, const MaterialId& b) { return a.value < b.value; }

}  // namespace denso_d2::decision

// Hashing support so ids can key unordered_map lookups.
namespace std {
template <>
struct hash<denso_d2::decision::ActionId> {
    size_t operator()(const denso_d2::decision::ActionId& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};
template <>
struct hash<denso_d2::decision::ConstraintId> {
    size_t operator()(const denso_d2::decision::ConstraintId& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};
template <>
struct hash<denso_d2::decision::ResourceId> {
    size_t operator()(const denso_d2::decision::ResourceId& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};
template <>
struct hash<denso_d2::decision::LocationId> {
    size_t operator()(const denso_d2::decision::LocationId& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};
template <>
struct hash<denso_d2::decision::RouteId> {
    size_t operator()(const denso_d2::decision::RouteId& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};
template <>
struct hash<denso_d2::decision::MaterialId> {
    size_t operator()(const denso_d2::decision::MaterialId& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};

template <>
struct hash<denso_d2::decision::TransportTaskId> {
    std::size_t operator()(const denso_d2::decision::TransportTaskId& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};

template <>
struct hash<denso_d2::decision::AmrId> {
    std::size_t operator()(const denso_d2::decision::AmrId& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};
}  // namespace std
