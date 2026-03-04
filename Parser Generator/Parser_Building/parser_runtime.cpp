#include "parser_interface.hpp"
#include "lalr_builder.hpp"
#include <sstream>
#include <cassert>

// LALRParser Factory & Lifecycle
LALRParser LALRParser::from_grammar(
    const Grammar& grammar,
    uint32_t dollar_symbol)
{
    return LALRParser(grammar, dollar_symbol);
}

LALRParser::LALRParser(const Grammar& grammar, uint32_t dollar_symbol)
    : _grammar(&grammar), _dollar_symbol(dollar_symbol)
{
    _tables = LALRBuilder::build(grammar, dollar_symbol, _conflicts);
}

LALRParser::LALRParser(const LALRParser& other)
    : _grammar(other._grammar), _dollar_symbol(other._dollar_symbol),
      _tables(other._tables), _conflicts(other._conflicts)
{
}

LALRParser::LALRParser(LALRParser&& other) noexcept
    : _grammar(other._grammar), _dollar_symbol(other._dollar_symbol),
      _tables(std::move(other._tables)), _conflicts(std::move(other._conflicts))
{
}

LALRParser& LALRParser::operator=(const LALRParser& other)
{
    if (this != &other) {
        _grammar = other._grammar;
        _dollar_symbol = other._dollar_symbol;
        _tables = other._tables;
        _conflicts = other._conflicts;
    }
    return *this;
}

LALRParser& LALRParser::operator=(LALRParser&& other) noexcept
{
    if (this != &other) {
        _grammar = other._grammar;
        _dollar_symbol = other._dollar_symbol;
        _tables = std::move(other._tables);
        _conflicts = std::move(other._conflicts);
    }
    return *this;
}

// Query Methods
size_t LALRParser::state_count() const { return _tables.ACTION.size(); }
size_t LALRParser::transition_count() const { return 0; }
size_t LALRParser::conflict_count() const { return _conflicts.size(); }
bool LALRParser::has_conflicts() const { return !_conflicts.empty(); }

const std::vector<Conflict>& LALRParser::conflicts() const { return _conflicts; }

const Grammar& LALRParser::grammar() const { return *_grammar; }

// Error Formatting
std::string LALRParser::format_error(
    const ParseResult::Status status,
    const std::string& token_name,
    int line,
    int column)
{
    std::ostringstream os;
    
    switch (status) {
        case ParseResult::Status::SyntaxError:
            os << "Syntax error at line " << line << ", column " << column
               << ": unexpected token '" << token_name << "'";
            break;
        case ParseResult::Status::UnexpectedEOF:
            os << "Unexpected end of input at line " << line << ", column " << column;
            break;
        case ParseResult::Status::InternalError:
            os << "Internal parser error at line " << line << ", column " << column;
            break;
        default:
            os << "Unknown error at line " << line << ", column " << column;
    }
    
    return os.str();
}

void LALRParser::set_builder(std::unique_ptr<ASTBuilder> builder) {
    _builder = std::move(builder);
}

// Parser Execution
ParseResult LALRParser::parse(const std::vector<Token>& tokens)
{
    return run_parser(tokens);
}

ParseResult LALRParser::run_parser(const std::vector<Token>& tokens)
{
    ParseResult result;
    result.status = ParseResult::Status::Success;
    result.error_line = 0;
    result.error_column = 0;
    result.reduction_sequence.clear();
    result.ast = nullptr; // default null
    
    // States stack
    std::vector<uint32_t> state_stack;
    state_stack.push_back(0);
    
    std::vector<ASTNode*> value_stack;
    
    size_t token_idx = 0;
    
    while (true) {
        uint32_t current_state = state_stack.back();
        
        // Compute current symbol and token
        uint32_t current_symbol;
        Token current_token(0, "", 0, 0);
        if (token_idx < tokens.size()) {
            current_symbol = tokens[token_idx].symbol_id;
            current_token = tokens[token_idx];
        } else {
            current_symbol = _dollar_symbol;
            current_token = Token(_dollar_symbol, "$", 
                                  tokens.empty() ? 0 : tokens.back().line,
                                  tokens.empty() ? 0 : tokens.back().column);
        }
        
        // Find action in ACTION table
        auto& action_map = _tables.ACTION[current_state];
        auto it = action_map.find(current_symbol);
        
        if (it == action_map.end()) {
            
            result.status = ParseResult::Status::SyntaxError;
            if (token_idx < tokens.size()) {
                result.error_line = tokens[token_idx].line;
                result.error_column = tokens[token_idx].column;
                result.error_token = tokens[token_idx].value;
            } else {
                result.error_line = tokens.empty() ? 0 : tokens.back().line;
                result.error_column = tokens.empty() ? 0 : tokens.back().column;
                result.error_token = "$";
            }
            result.error_message = format_error(result.status, result.error_token, 
                                                result.error_line, result.error_column);
            return result;
        }
        
        Action action = it->second;
        
        if (action.kind == ActionKind::Shift) {
            // Shift: push next state and consume token
            uint32_t next_state = (uint32_t)action.value;
            state_stack.push_back(next_state);
            token_idx++;
            
            // If there is a builder, construct a leaf node from the token
            if (_builder) {
                ASTNode* node = nullptr;
                _builder->onShift(current_symbol, current_token, node);
                value_stack.push_back(node);
            }
        }
        else if (action.kind == ActionKind::Reduce) {
            uint32_t prod_id = (uint32_t)action.value;
            const Production& prod = _grammar->get_productions()[prod_id];
            
            // Pick up RHS nodes for builder before popping states
            std::vector<ASTNode*> rhs_nodes;
            if (_builder) {
                for (size_t i = 0; i < prod.rhs.size(); ++i) {
                    rhs_nodes.push_back(value_stack.back());
                    value_stack.pop_back();
                }
                std::reverse(rhs_nodes.begin(), rhs_nodes.end()); // ahora en orden RHS[0]..RHS[n-1]
            }
            
            // States pop
            for (size_t i = 0; i < prod.rhs.size(); ++i) {
                if (state_stack.size() > 1) {
                    state_stack.pop_back();
                }
            }
            
            result.reduction_sequence.push_back(prod_id);
            
            // Get new state from GOTO table
            uint32_t prev_state = state_stack.back();
            auto& goto_map = _tables.GOTO[prev_state];
            auto goto_it = goto_map.find(prod.lhs);
            if (goto_it == goto_map.end()) {
                result.status = ParseResult::Status::InternalError;
                result.error_message = "GOTO table inconsistent";
                return result;
            }
            state_stack.push_back(goto_it->second);
            
            // If builder, build new AST node
            if (_builder) {
                ASTNode* new_node = nullptr;
                _builder->onReduce(prod_id, rhs_nodes, new_node);
                value_stack.push_back(new_node);
            }
        }
        else if (action.kind == ActionKind::Accept) {
            result.status = ParseResult::Status::Success;
            
            // If there is a builder, the root node should be on top of value_stack
            if (_builder && !value_stack.empty()) {
                result.ast.reset(value_stack.back());
                value_stack.pop_back();
            }
            return result;
        }
        else {
            result.status = ParseResult::Status::InternalError;
            result.error_message = "Parser reached error state";
            return result;
        }
    }
}