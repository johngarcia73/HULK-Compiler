#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class StringPool {
private:
    std::unordered_map<std::string, std::string> pool;
public:
    
    std::string getOrCreateId(const std::string& str) {
        auto it = pool.find(str);
        if (it != pool.end()) {
            return it->second;
        }

        std::string id = "string_literal_pointer" + std::to_string(pool.size());
        pool[str] = id;
        return id;
    }

};
