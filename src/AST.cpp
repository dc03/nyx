/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "AST.hpp"

#include "Common.hpp"

std::string stringify(BaseType *node) {
    std::string result{};
    if (node->is_const) {
        result += "const ";
    }

    if (node->is_ref) {
        result += "ref ";
    }

    switch (node->primitive) {
        case Type::INT: result += "int"; break;
        case Type::BOOL: result += "bool"; break;
        case Type::STRING: result += "string"; break;
        case Type::NULL_: result += "null"; break;
        case Type::FLOAT: result += "float"; break;
        case Type::CLASS: {
            auto type = dynamic_cast<UserDefinedType *>(node);
            result += type->name.lexeme;
            break;
        }
        case Type::LIST: {
            using namespace std::string_literals;
            auto type = dynamic_cast<ListType *>(node);
            result += "["s + stringify(type->contained.get()) + "]"s;
            break;
        }
        default: unreachable();
    }
    return result;
}

BaseTypeVisitorType copy_type(BaseType *node) {
    if (node->type_tag() == NodeType::PrimitiveType) {
        return allocate_node(PrimitiveType, node->primitive, node->is_const, node->is_ref);
    } else if (node->type_tag() == NodeType::UserDefinedType) {
        auto *type = dynamic_cast<UserDefinedType *>(node);
        return allocate_node(UserDefinedType, type->primitive, type->is_const, type->is_ref, type->name);
    } else if (node->type_tag() == NodeType::ListType) {
        auto *type = dynamic_cast<ListType *>(node);
        return allocate_node(ListType, type->primitive, type->is_const, type->is_ref,
            TypeNode{copy_type(type->contained.get())}, nullptr);
    }
    unreachable();
}