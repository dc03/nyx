/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "AST/AST.hpp"

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
        case Type::TUPLE: {
            auto *tuple = dynamic_cast<TupleType *>(node);
            result += "{";
            auto begin = tuple->types.begin();
            for (; begin != tuple->types.end() - 1; begin++) {
                result += stringify(begin->get()) + ", ";
            }
            result += stringify(begin->get()) + "}";
            break;
        }
        default: unreachable();
    }
    return result;
}

std::string stringify_short(const BaseType *node, bool consider_const, bool consider_ref) {
    std::string result{};
    if (node->is_const && consider_const) {
        result += "c%";
    }

    if (node->is_ref && consider_ref) {
        result += "r%";
    }

    switch (node->primitive) {
        case Type::INT: result += "i"; break;
        case Type::BOOL: result += "b"; break;
        case Type::STRING: result += "s"; break;
        case Type::NULL_: result += "n"; break;
        case Type::FLOAT: result += "f"; break;
        case Type::CLASS: {
            auto type = dynamic_cast<UserDefinedType *>(node);
            result += type->name.lexeme;
            break;
        }
        case Type::LIST: {
            using namespace std::string_literals;
            auto type = dynamic_cast<ListType *>(node);
            result += "["s + stringify_short(type->contained.get(), consider_const, consider_ref) + "]";
            break;
        }
        case Type::TUPLE: {
            auto *tuple = dynamic_cast<TupleType *>(node);
            result += "{";
            auto begin = tuple->types.begin();
            for (; begin != tuple->types.end() - 1; begin++) {
                result += stringify_short(begin->get(), consider_const, consider_ref) + ",";
            }
            result += stringify_short(begin->get(), consider_const, consider_ref) + "}";
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
        return allocate_node(UserDefinedType, type->primitive, type->is_const, type->is_ref, type->name, type->class_);
    } else if (node->type_tag() == NodeType::ListType) {
        auto *type = dynamic_cast<ListType *>(node);
        return allocate_node(
            ListType, type->primitive, type->is_const, type->is_ref, TypeNode{copy_type(type->contained.get())});
    } else if (node->type_tag() == NodeType::TupleType) {
        auto *tuple = dynamic_cast<TupleType *>(node);
        std::vector<TypeNode> types{};
        for (auto &type : tuple->types) {
            types.emplace_back(copy_type(type.get()));
        }
        return allocate_node(TupleType, tuple->primitive, tuple->is_const, tuple->is_ref, std::move(types));
    }
    unreachable();
}

std::size_t vartuple_size(IdentifierTuple::TupleType &tuple) {
    std::size_t len = 0;
    for (auto &elem : tuple) {
        if (elem.index() == IdentifierTuple::IDENT_TUPLE) {
            len += vartuple_size(std::get<IdentifierTuple>(elem).tuple);
        } else {
            len += 1;
        }
    }
    return len;
}

bool is_trivial_type(Type type) {
    switch (type) {
        case Type::BOOL:
        case Type::INT:
        case Type::FLOAT:
        case Type::STRING:
        case Type::NULL_: return true;
        default: return false;
    }
}

bool is_trivial_type(BaseType *node) {
    return is_trivial_type(node->primitive);
}

bool is_nontrivial_type(Type type) {
    switch (type) {
        case Type::CLASS:
        case Type::LIST:
        case Type::TUPLE: return true;
        default: return false;
    }
}

bool is_nontrivial_type(BaseType *node) {
    return is_nontrivial_type(node->primitive);
}

bool is_constructor(FunctionStmt *stmt) {
    return stmt->class_ != nullptr && stmt->name == stmt->class_->name;
}

bool is_destructor(FunctionStmt *stmt) {
    return stmt->class_ != nullptr && stmt->name.lexeme[0] == '~' &&
           stmt->name.lexeme.substr(1) == stmt->class_->name.lexeme;
}