/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "List.hpp"

#include "../Common.hpp"
#include "Value.hpp"

List::List(std::vector<int> value) : as{std::move(value)} {}
List::List(std::vector<double> value) : as{std::move(value)} {}
List::List(std::vector<std::string> value) : as{std::move(value)} {}
List::List(std::vector<char> value) : as{std::move(value)} {}
List::List(std::vector<Value *> value) : as{std::move(value)} {}
List::List(std::vector<std::unique_ptr<List>> value) : as{std::move(value)} {}

List make_list(List::tag type) {
    switch (type) {
        case List::tag::INT_LIST: return List{std::vector<int>{}};
        case List::tag::FLOAT_LIST: return List{std::vector<double>{}};
        case List::tag::STRING_LIST: return List{std::vector<std::string>{}};
        case List::tag::BOOL_LIST: return List{std::vector<char>{}};
        case List::tag::REF_LIST: return List{std::vector<Value *>{}};
        case List::tag::LIST_LIST: return List{std::vector<std::unique_ptr<List>>{}};
        default: break;
    }
    unreachable();
}

bool List::operator==(const List &other) const noexcept {
    return as == other.as;
}

std::size_t List::size() noexcept {
    if (is_int_list()) {
        return to_int_list().size();
    } else if (is_float_list()) {
        return to_float_list().size();
    } else if (is_string_list()) {
        return to_string_list().size();
    } else if (is_bool_list()) {
        return to_bool_list().size();
    } else if (is_ref_list()) {
        return to_ref_list().size();
    } else if (is_list_list()) {
        return to_list_list().size();
    }
    unreachable();
}

void List::resize(std::size_t size) noexcept {
    if (is_int_list()) {
        to_int_list().resize(size);
    } else if (is_float_list()) {
        to_float_list().resize(size);
    } else if (is_string_list()) {
        to_string_list().resize(size);
    } else if (is_bool_list()) {
        to_bool_list().resize(size);
    } else if (is_ref_list()) {
        to_ref_list().resize(size);
    } else if (is_list_list()) {
        to_list_list().resize(size);
    }
}

Value List::at(std::size_t index) noexcept {
    if (is_int_list()) {
        return Value{to_int_list()[index]};
    } else if (is_float_list()) {
        return Value{to_float_list()[index]};
    } else if (is_string_list()) {
        return Value{to_string_list()[index]};
    } else if (is_bool_list()) {
        return Value{static_cast<bool>(to_bool_list()[index])};
    } else if (is_ref_list()) {
        return Value{to_ref_list()[index]};
    } else if (is_list_list()) {
        return Value{SharedUniquePtr<List>{to_list_list()[index]}};
    }
    unreachable();
}

Value List::assign_at(std::size_t index, Value &value) noexcept {
    if (is_int_list()) {
        return Value{to_int_list()[index] = value.to_int()};
    } else if (is_float_list()) {
        return Value{to_float_list()[index] = value.to_double()};
    } else if (is_string_list()) {
        return Value{to_string_list()[index] = value.to_string()};
    } else if (is_bool_list()) {
        return Value{static_cast<bool>((to_bool_list()[index] = value.to_bool()))};
    } else if (is_ref_list()) {
        return Value{to_ref_list()[index] = value.to_referred()};
    } else if (is_list_list()) {
        return Value{SharedUniquePtr<List>{[this, &index, &value]() -> std::unique_ptr<List> & {
            List &list = value.to_list();
          if (std::unique_ptr<List> &assigned = to_list_list()[index]; assigned == nullptr) {
              std::unique_ptr<List> temp = std::make_unique<List>(make_list(list.type()));
              assigned.swap(temp);
          }
            List &assigned = *to_list_list()[index];
            assigned.resize(list.size());
#define DO_COPY(cond, method)                                                                                          \
    if (list.cond) {                                                                                                   \
        std::copy(list.method.begin(), list.method.end(), assigned.method.begin());                                    \
    }
            DO_COPY(is_int_list(), to_int_list())
            DO_COPY(is_float_list(), to_float_list())
            DO_COPY(is_string_list(), to_string_list())
            DO_COPY(is_ref_list(), to_ref_list())
            DO_COPY(is_bool_list(), to_bool_list())
            if (is_list_list()) {
                for (std::size_t i = 0; i < list.size(); i++) {
                    Value to_assign = assigned.at(i);
                    list.assign_at(i, to_assign);
                }
            }
            return to_list_list()[index];
        }()}};
    }
    unreachable();
#undef DO_COPY
}

Value List::assign_at(std::size_t index, Value &&value) noexcept {
    if (is_int_list()) {
        return Value{to_int_list()[index] = std::move(value).to_int()};
    } else if (is_float_list()) {
        return Value{to_float_list()[index] = std::move(value).to_double()};
    } else if (is_string_list()) {
        return Value{to_string_list()[index] = std::move(value.to_string())};
    } else if (is_bool_list()) {
        return Value{static_cast<bool>((to_bool_list()[index] = std::move(value).to_bool()))};
    } else if (is_ref_list()) {
        return Value{to_ref_list()[index] = std::move(value).to_referred()};
    } else if (is_list_list()) {
        return Value{SharedUniquePtr<List>{[this, &index, &value]() -> std::unique_ptr<List> & {
            List &list = value.to_list();
            if (std::unique_ptr<List> &assigned = to_list_list()[index]; assigned == nullptr) {
                std::unique_ptr<List> temp = std::make_unique<List>(make_list(list.type()));
                assigned.swap(temp);
            }
            List &assigned = *to_list_list()[index];
            assigned = std::move(list);
            return to_list_list()[index];
        }()}};
    }
    unreachable();
}