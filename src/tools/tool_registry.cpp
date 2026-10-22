// ECE 309 Project 3 - ToolRegistry
// A simple hash table using separate chaining (one small vector per bucket).

#include "tools/tool_registry.h"
#include <cstdint>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(16), count_(0) {}

ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    // FNV-1a: look at every letter in the name, in order.
    std::uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : key) {
        h = h ^ c;
        h = h * 1099511628211ULL;
    }
    return static_cast<std::size_t>(h);
}

void ToolRegistry::grow() {
    std::vector<std::vector<std::unique_ptr<Tool>>> bigger(buckets_.size() * 2);

    for (auto& chain : buckets_) {
        for (auto& tool : chain) {
            std::size_t index = hash(tool->name()) % bigger.size();
            bigger[index].push_back(std::move(tool));
        }
    }
    buckets_ = std::move(bigger);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (tool == nullptr) {
        return false;
    }

    // Check the name before storing. A duplicate is discarded automatically.
    std::string name = tool->name();
    if (find(name) != nullptr) {
        return false;
    }

    // Double when the next entry would go above a load factor of 0.75.
    if ((count_ + 1) * 4 > buckets_.size() * 3) {
        grow();
    }

    std::size_t index = hash(name) % buckets_.size();
    buckets_[index].push_back(std::move(tool));
    ++count_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    std::size_t index = hash(name) % buckets_.size();
    for (const auto& tool : buckets_[index]) {
        if (tool->name() == name) {
            return tool.get();
        }
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return count_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& chain : buckets_) {
        for (const auto& tool : chain) {
            fn(*tool);
        }
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(count_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& chain : buckets_) {
        if (chain.size() > longest) {
            longest = chain.size();
        }
    }
    return longest;
}
