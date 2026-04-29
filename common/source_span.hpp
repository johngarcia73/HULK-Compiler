#pragma once

#include <string>

struct SourceLocation {
    int line = 0;
    int column = 0;

    bool isValid() const {
        return line > 0 && column > 0;
    }

    std::string toString() const {
        if (!isValid()) {
            return "<unknown>";
        }
        return std::to_string(line) + ":" + std::to_string(column);
    }
};

struct SourceSpan {
    SourceLocation start;
    SourceLocation end;

    bool isValid() const {
        return start.isValid() && end.isValid();
    }

    std::string toString() const {
        if (!isValid()) {
            return "<unknown>";
        }
        return start.toString() + "-" + end.toString();
    }

    static SourceSpan merge(const SourceSpan& left, const SourceSpan& right) {
        if (!left.isValid()) {
            return right;
        }
        if (!right.isValid()) {
            return left;
        }
        return {left.start, right.end};
    }
};
