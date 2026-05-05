#include <string>
#include <format>
#include <string_view>

class StringBuilder {
private:
    std::string buffer;
    int indent_level = 0;
    static constexpr int spaces_per_level = 4;
    int current_spaces_per_level = spaces_per_level;

    void apply_indent() {
        buffer.append(indent_level * current_spaces_per_level, ' ');
        current_spaces_per_level = 0;
    }

public:
    StringBuilder& indent() {
        indent_level++;
        return *this;
    }

    StringBuilder& unindent() {
        if (indent_level > 0) indent_level--;
        return *this;
    }
    
    
    // Variadic template to support any number of format arguments
    template<typename... Args>
    StringBuilder& add(std::string_view fmt, Args&&... args) {
        apply_indent();
        if constexpr (sizeof...(args) > 0) {
            buffer += std::vformat(fmt, std::make_format_args(args...));
        } else {
            for (size_t i = 0; i < fmt.size(); ++i) {
                if (fmt[i] == '{' && i + 1 < fmt.size() && fmt[i + 1] == '{') {
                    buffer += '{';
                    ++i;
                } else if (fmt[i] == '}' && i + 1 < fmt.size() && fmt[i + 1] == '}') {
                    buffer += '}';
                    ++i;
                } else {
                    buffer += fmt[i];
                }
            }
        }
        return *this;
    }

    StringBuilder& addLine() {
        add("");
        buffer += "\n";
        current_spaces_per_level = spaces_per_level;
        return *this;
    }
    // Variadic template to support any number of format arguments
    template<typename... Args>
    StringBuilder& addLine(std::string_view fmt, Args&&... args) {
        add(fmt, std::forward<Args>(args)...);
        buffer += "\n";
        current_spaces_per_level = spaces_per_level;
        return *this;
    }

    std::string toString() const { return buffer; }
};
