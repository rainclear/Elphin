#include "elphin/skiplist.hpp"
#include <random>

namespace elphin::store {

SkipList::SkipList()
    : level_(1), rng_(std::random_device{}()), dist_(0.0, 1.0) {
    head_ = new SkipListNode("", 0.0, MAX_LEVEL);
}

SkipList::~SkipList() {
    SkipListNode* curr = head_;
    while (curr) {
        SkipListNode* next = curr->forward[0];
        delete curr;
        curr = next;
    }
}

int SkipList::random_level() {
    int lvl = 1;
    while (dist_(rng_) < SKIPLIST_P && lvl < MAX_LEVEL) {
        lvl++;
    }
    return lvl;
}

void SkipList::insert(const std::string& member, double score) {
    std::vector<SkipListNode*> update(MAX_LEVEL, nullptr);
    SkipListNode* curr = head_;

    // Traverse levels from top to bottom
    for (int i = level_ - 1; i >= 0; --i) {
        while (curr->forward[i] && 
              (curr->forward[i]->score < score || 
              (curr->forward[i]->score == score && curr->forward[i]->member < member))) {
            curr = curr->forward[i];
        }
        update[i] = curr;
    }

    int new_level = random_level();
    if (new_level > level_) {
        for (int i = level_; i < new_level; ++i) {
            update[i] = head_;
        }
        level_ = new_level;
    }

    SkipListNode* new_node = new SkipListNode(member, score, new_level);
    for (int i = 0; i < new_level; ++i) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }
}

bool SkipList::erase(const std::string& member, double score) {
    std::vector<SkipListNode*> update(MAX_LEVEL, nullptr);
    SkipListNode* curr = head_;

    for (int i = level_ - 1; i >= 0; --i) {
        while (curr->forward[i] && 
              (curr->forward[i]->score < score || 
              (curr->forward[i]->score == score && curr->forward[i]->member < member))) {
            curr = curr->forward[i];
        }
        update[i] = curr;
    }

    curr = curr->forward[0];

    // Node found
    if (curr && curr->score == score && curr->member == member) {
        for (int i = 0; i < level_; ++i) {
            if (update[i]->forward[i] != curr) break;
            update[i]->forward[i] = curr->forward[i];
        }
        delete curr;

        while (level_ > 1 && head_->forward[level_ - 1] == nullptr) {
            level_--;
        }
        return true;
    }
    return false;
}

std::vector<std::pair<std::string, double>> SkipList::get_range_by_score(double min_score, double max_score) const {
    std::vector<std::pair<std::string, double>> result;
    SkipListNode* curr = head_;

    // Find first node with score >= min_score
    for (int i = level_ - 1; i >= 0; --i) {
        while (curr->forward[i] && curr->forward[i]->score < min_score) {
            curr = curr->forward[i];
        }
    }

    curr = curr->forward[0];

    // Collect all elements in range
    while (curr && curr->score <= max_score) {
        result.emplace_back(curr->member, curr->score);
        curr = curr->forward[0];
    }

    return result;
}

} // namespace elphin::store