#ifndef NAME_GENERATOR_HPP
#define NAME_GENERATOR_HPP

#include <string>
#include <unordered_map>

class NameGenerator {
public:
    // Generate a name with a custom base name (e.g., "tmp" -> tmp1, tmp2, ...)
    std::string generateName(const std::string& name) {
        int index = ++counters_[name];
        return name + std::to_string(index);
    }
    
    // Generate a default name (base "tmp" -> tmp1, tmp2, ...)
    std::string generateName() {
        return generateName("tmp");
    }

private:
    std::unordered_map<std::string, int> counters_;
};

#endif // NAME_GENERATOR_HPP