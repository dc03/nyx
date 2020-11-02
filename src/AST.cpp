#include "AST.hpp"

#define unreachable() __builtin_unreachable()
#define allocate_node(T, ...)                                                                                          \
    new T {                                                                                                            \
        __VA_ARGS__                                                                                                    \
    }

std::string stringify(BaseType *node) {
    std::string result{};
    if (node->data.is_const) {
        result += "const ";
    }

    if (node->data.is_ref) {
        result += "ref ";
    }

    switch (node->data.type) {
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
            auto type = dynamic_cast<ListType *>(node);
            result += stringify(type->contained.get());
            break;
        }
        default: unreachable();
    }
    return result;
}

BaseTypeVisitorType copy_type(BaseType *node) {
    if (node->type_tag() == NodeType::PrimitiveType) {
        return allocate_node(PrimitiveType, node->data);
    } else if (node->type_tag() == NodeType::UserDefinedType) {
        UserDefinedType *type = dynamic_cast<UserDefinedType *>(node);
        return allocate_node(UserDefinedType, type->data, type->name);
    } else if (node->type_tag() == NodeType::ListType) {
        ListType *type = dynamic_cast<ListType *>(node);
        return allocate_node(ListType, type->data, type_node_t{copy_type(type->contained.get())}, nullptr);
    }
    unreachable();
}