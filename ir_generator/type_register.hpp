#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <memory>
#include <stdexcept>
#include "type_utils.hpp"
#include "../parser/AST_Builder/ast_node.hpp"

// ====================== TypeRegister ======================
class TypeRegister {
public:
    TypeDeclNode* getDeclaration(std::string typeName){
        return  declarations.at(typeName);
    }
    //Computes the layout if not already computed
    void computeLayout(std::string typeName)
    {
      //Check if found in computed layouts
      //if it's founded return 
      if(computedLayout[typeName]) return;

      int size;
      std::string wordSize;
      if(typeName=="Object"){
        
        size= TypeUtils::currentTarget.PointerSize;
        wordSize = TypeUtils::currentTarget.PointerType;
        //Not need to care about other invariants in this case
        
        return;
      }

      //Found the type declaration for typeName on declartions and store in decl
      TypeDeclNode* decl = declarations.at(typeName);
      computeLayout(decl->parentType);
      //Get the size of the parent layout and store it on currentOffset
      int currentOffset = decl->parentType.empty() ? 0 : sizes[decl->parentType];
      for(auto member: decl->members)
      {
        auto attr=dynamic_cast<AttributeDeclNode*>(member);
        if(attr){
            currentOffset+=TypeUtils::getByteSize(attr->type);
            
            //Add attr->name,currentOffset to the map with the key typeName on offsets
            offsets[typeName][attr->name] = currentOffset;
            wordSize = TypeUtils::toQbeType(decl->type);
            //Add attr->name,wordSize to the map on with key typeName on wordSizes
            wordSizes[typeName][attr->name] = wordSize;
        }
        
      }
      //Add typeName,currentOffset to sizes.
      sizes[typeName] = currentOffset;
      //Add typeName,true to computedLayout
      computedLayout[typeName] = true;
    
    }
    int totalSize(std::string typeName){
      computeLayout(typeName);
      //Found on sizes for key typeName;
      return sizes[typeName];
    } 
    // Returns byte offset of the given field, or -1 if not found.
    int getOffset(std::string typeName,std::string fieldName)
    {
        computeLayout(typeName);
        //Found on offsets with the key the typeName and then with the key fieldName
        auto& offMap = offsets[typeName];
        auto it = offMap.find(fieldName);
        return (it != offMap.end()) ? it->second : -1;
    }

    /// Returns word size string 
    std::string getWordSize(std::string typeName,std::string& fieldName)
    {
        computeLayout(typeName);
        //Found on wordSizes with the key the typeName and then with the key fieldName
        return wordSizes[typeName][fieldName];
    }
    void registerFromDeclaration(TypeDeclNode* decl) {
       //Adds the decl to  declarations, decl->name holds the type name wich will
       //be used as key.  
       declarations[decl->name] = decl;
    }

private:
    std::unordered_map<std::string, TypeDeclNode*> declarations;
    std::unordered_map<std::string, int> sizes;
    std::unordered_map<std::string,std::unordered_map<std::string,int>> offsets; 
    std::unordered_map<std::string,std::unordered_map<std::string,std::string>> wordSizes; 
    std::unordered_map<std::string,bool> computedLayout;
};