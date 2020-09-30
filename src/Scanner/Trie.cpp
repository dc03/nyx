#include "Trie.hpp"

[[nodiscard]] constexpr std::size_t Trie::get_index(char ch) const noexcept {
    assert((std::isalnum(ch) || ch == '_') && "Only english characters allowed for now.");

    if (ch >= 'a' && ch <= 'z') {
        return ch - 'a';
    } else {
        return (ch - 'A') + 26;
    }
}

void Trie::insert(const std::string_view key, TokenType type) {
    if (head == nullptr) {
        head = Node::make_unique();
    }

    Node *current{head.get()};
    for (char ch : key) {
        std::size_t index{get_index(ch)};
        if (current->nodes[index] != nullptr &&
            current->nodes[index]->value == ch) {
            current = current->nodes[index].get();
        } else {
            current->nodes[index] = Node::make_unique();
            current->nodes[index]->value = ch;
            current = current->nodes[index].get();
        }
    }

    current->is_last = true;
    current->type = type;
}

[[nodiscard]] TokenType Trie::search(const std::string_view key) const noexcept {
    Node *current{head.get()};
    for (char ch : key) {
        std::size_t index{get_index(ch)};
        if (current->nodes[index] != nullptr &&
            current->nodes[index]->value == ch) {
            current = current->nodes[index].get();
        } else if (current->nodes[index] == nullptr) {
            return TokenType::NONE;
        }
    }

    if (current->is_last) {
        return current->type;
    } else {
        return TokenType::NONE;
    }
}
