#pragma once

#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "../common/source_span.hpp"

enum class SemanticDiagnosticSeverity {
    Error,
    Warning,
    Info
};

enum class SemanticPhase {
    Pipeline,
    Declarations,
    Dependencies,
    Inference,
    OverloadResolution,
    GlobalStatements
};

inline std::string to_string(SemanticDiagnosticSeverity severity) {
    switch (severity) {
        case SemanticDiagnosticSeverity::Error:
            return "error";
        case SemanticDiagnosticSeverity::Warning:
            return "warning";
        case SemanticDiagnosticSeverity::Info:
            return "info";
    }
    return "info";
}

inline std::string to_string(SemanticPhase phase) {
    switch (phase) {
        case SemanticPhase::Pipeline:
            return "pipeline";
        case SemanticPhase::Declarations:
            return "declarations";
        case SemanticPhase::Dependencies:
            return "dependencies";
        case SemanticPhase::Inference:
            return "inference";
        case SemanticPhase::OverloadResolution:
            return "overload";
        case SemanticPhase::GlobalStatements:
            return "globals";
    }
    return "pipeline";
}

struct SemanticDebugOptions {
    bool trace_pipeline = false;
    bool trace_inference = false;
    bool trace_overloads = false;
    bool dump_symbols = false;
    bool dump_dependency_graph = false;
    bool dump_ast = false;
};

struct SemanticDiagnostic {
    SemanticDiagnosticSeverity severity = SemanticDiagnosticSeverity::Info;
    SemanticPhase phase = SemanticPhase::Pipeline;
    std::string message;
    SourceSpan span;
    std::vector<std::string> notes;

    std::string format() const {
        std::ostringstream os;
        os << "[" << to_string(severity) << "][" << to_string(phase) << "]";
        if (span.isValid()) {
            os << " " << span.start.toString();
        }
        os << ": " << message;
        for (const auto& note : notes) {
            os << "\n  note: " << note;
        }
        return os.str();
    }
};

struct SemanticAnalysisResult {
    std::vector<SemanticDiagnostic> diagnostics;
    std::vector<std::string> traces;

    bool success() const {
        for (const auto& diagnostic : diagnostics) {
            if (diagnostic.severity == SemanticDiagnosticSeverity::Error) {
                return false;
            }
        }
        return true;
    }
};

class SemanticContext {
    SemanticDebugOptions options_;
    std::vector<SemanticDiagnostic> diagnostics_;
    std::vector<std::string> traces_;

    void maybeTrace(bool enabled, SemanticPhase phase, const std::string& message) {
        if (!enabled) {
            return;
        }
        traces_.push_back("[trace][" + to_string(phase) + "] " + message);
    }

public:
    explicit SemanticContext(SemanticDebugOptions options = {})
        : options_(options) {}

    const SemanticDebugOptions& options() const {
        return options_;
    }

    bool hasErrors() const {
        for (const auto& diagnostic : diagnostics_) {
            if (diagnostic.severity == SemanticDiagnosticSeverity::Error) {
                return true;
            }
        }
        return false;
    }

    void addDiagnostic(
        SemanticDiagnosticSeverity severity,
        SemanticPhase phase,
        std::string message,
        SourceSpan span = {},
        std::vector<std::string> notes = {}) {
        diagnostics_.push_back({severity, phase, std::move(message), span, std::move(notes)});
    }

    void error(
        SemanticPhase phase,
        const std::string& message,
        SourceSpan span = {},
        std::vector<std::string> notes = {}) {
        addDiagnostic(
            SemanticDiagnosticSeverity::Error,
            phase,
            message,
            span,
            std::move(notes));
    }

    void warning(
        SemanticPhase phase,
        const std::string& message,
        SourceSpan span = {},
        std::vector<std::string> notes = {}) {
        addDiagnostic(
            SemanticDiagnosticSeverity::Warning,
            phase,
            message,
            span,
            std::move(notes));
    }

    void tracePipeline(const std::string& message) {
        maybeTrace(options_.trace_pipeline, SemanticPhase::Pipeline, message);
    }

    void traceInference(const std::string& message) {
        maybeTrace(options_.trace_inference, SemanticPhase::Inference, message);
    }

    void traceOverload(const std::string& message) {
        maybeTrace(options_.trace_overloads, SemanticPhase::OverloadResolution, message);
    }

    void addDump(const std::string& header, const std::string& body) {
        traces_.push_back("[dump][" + header + "]\n" + body);
    }

    const std::vector<SemanticDiagnostic>& diagnostics() const {
        return diagnostics_;
    }

    const std::vector<std::string>& traces() const {
        return traces_;
    }

    SemanticAnalysisResult buildResult() const {
        return {diagnostics_, traces_};
    }

    void printReport(std::ostream& out) const {
        for (const auto& trace : traces_) {
            out << trace << "\n";
        }
        for (const auto& diagnostic : diagnostics_) {
            out << diagnostic.format() << "\n";
        }
    }
};
