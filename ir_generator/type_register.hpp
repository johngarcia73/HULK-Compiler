#pragma once
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <memory>
#include "type_utils.hpp"
#include "../parser/AST_Builder/ast_node.hpp"

// ====================== TypeInfo ======================
class TypeInfo {
public:
    // Pointer to the AST node that defined this type.
    // Assumed to stay alive as long as this TypeInfo exists.
    const TypeDeclNode* declaration = nullptr;

    // field name -> (word_size_string, byte_offset)
    std::map<std::string, std::pair<std::string, int>> fieldInfo;

    int totalSize = 0;   // total size in bytes

    TypeInfo() = default;

    /// Returns byte offset of the given field, or -1 if not found.
    int getOffset(const std::string& fieldName) const {
        auto it = fieldInfo.find(fieldName);
        if (it != fieldInfo.end())
            return it->second.second;
        return -1;
    }

    /// Returns word size string ("w" or "l") for the given field.
    std::string getWordSize(const std::string& fieldName) const {
        auto it = fieldInfo.find(fieldName);
        if (it != fieldInfo.end())
            return it->second.first;
        return "";
    }
};

// ====================== TypeRegister ======================
class TypeRegister {
public:
    /// Returns the TypeInfo for the named type (throws std::out_of_range if missing).
    TypeInfo& getInfo(const std::string& name) {
        return typeInfos_.at(name);
    }

    bool hasInfo(const std::string& name) const {
        return typeInfos_.find(name) != typeInfos_.end();
    }

    /// Registers a new type from a TypeDeclNode.
    /// The declaration must stay valid throughout the compiler's lifetime.
    /// Parent type (if any) must already be registered.
    void registerFromDeclaration(TypeDeclNode* decl) {
        TypeInfo info;
          // store pointer, no data copied

        // 1. Start offset after parent size
        int currentOffset = 0;
        
        /* This can be usefull later, but righ now, is buggy.
        if (!decl->parentType.empty()) {
            TypeInfo& parent = getInfo(decl->parentType);
            currentOffset = parent.totalSize;
        } */

        // 2. Process own attribute declarations
        for (auto memberPtr : decl->members) {
            auto attr = dynamic_cast<AttributeDeclNode*>(memberPtr);
            if (!attr) continue;   // skip non-field members
            
            int fieldSize = TypeUtils::getByteSize(attr->type);
            std::string fieldWordSize= TypeUtils::toQbeType(attr->type);
            
            info.fieldInfo[attr->name] = std::make_pair(fieldWordSize, currentOffset);
            currentOffset += fieldSize;
        }
        info.declaration = decl; 
        info.totalSize = currentOffset;
        typeInfos_[decl->name] = std::move(info);
    }

private:
    std::map<std::string, TypeInfo> typeInfos_;
};