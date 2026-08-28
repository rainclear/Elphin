#ifndef ELPHIN_SKIPLIST_HPP
#define ELPHIN_SKIPLIST_HPP

#include <string>
#include <vector>
#include <memory>
#include <random>
#include <iostream>

namespace elphin::store {

constexpr int MAX_LEVEL = 24;
constexpr double SKIPLIST_P = 0.25;

struct SkipListNode {
    std::string member;
    double score;
    std::vector<SkipListNode*> forward;

    SkipListNode(std::string m, double s, int level)
        : member(std::move(m)), score(s), forward(level, nullptr) {}
};

class SkipList {
public:
    SkipList();
    ~SkipList();

    // Disable copying
    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    void insert(const std::string& member, double score);
    bool erase(const std::string& member, double score);
    std::vector<std::pair<std::string, double>> get_range_by_score(double min_score, double max_score) const;

private:
    int random_level();

    SkipListNode* head_;
    int level_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_;
};

} // namespace elphin::store

#endif // ELPHIN_SKIPLIST_HPP