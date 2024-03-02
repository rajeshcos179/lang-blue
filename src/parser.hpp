#pragma once

#include <optional>
#include <iostream>
#include <variant>

#include "./arena.hpp"
#include "./tokenization.hpp"

enum class DataType
{
    _int,
    _char,
    _float,
};

struct NodeTermIntLit
{
    Token int_lit;
};

struct NodeTermCharLit
{
    Token char_lit;
};

struct NodeTermFloatLit
{
    Token float_lit;
};

struct NodeTermIdent
{
    Token ident;
};

struct NodeExpr;

struct NodeTermParen
{
    NodeExpr *expr;
};

struct NodeBinExprAdd
{
    NodeExpr *lhs;
    NodeExpr *rhs;
};

struct NodeBinExprSub
{
    NodeExpr *lhs;
    NodeExpr *rhs;
};

struct NodeBinExprMul
{
    NodeExpr *lhs;
    NodeExpr *rhs;
};

struct NodeBinExprDiv
{
    NodeExpr *lhs;
    NodeExpr *rhs;
};

struct NodeBinExprMod
{
    NodeExpr *lhs;
    NodeExpr *rhs;
};

// Binary expressions consists of two expressions
struct NodeBinExpr
{
    std::variant<NodeBinExprAdd *, NodeBinExprMul *, NodeBinExprSub *, NodeBinExprDiv *, NodeBinExprMod *> var;
};

// Term can be an integer literal, an identifier or an expression
struct NodeTerm
{
    std::variant<NodeTermIntLit *, NodeTermCharLit *, NodeTermFloatLit *, NodeTermIdent *, NodeTermParen *> var;
};

// Expression can be a term or binary expression
struct NodeExpr
{
    std::variant<NodeTerm *, NodeBinExpr *> var;
};

struct NodeStmtExit
{
    NodeExpr *expr;
};

struct NodeStmtLet
{
    Token ident;
    NodeExpr *expr{};
};

struct NodeStmt;

struct NodeScope
{
    std::vector<NodeStmt *> stmts;
};

struct NodeIfPred;

//
struct NodeIfPredElif
{
    NodeExpr *expr{};
    NodeScope *scope{};
    std::optional<NodeIfPred *> pred;
};

struct NodeIfPredElse
{
    NodeScope *scope;
};

// if statment can be followed by an else or elif statment
struct NodeIfPred
{
    std::variant<NodeIfPredElif *, NodeIfPredElse *> var;
};

// if statement has an expression, scope and an optional else or elif
struct NodeStmtIf
{
    struct NodeExpr *expr{};
    struct NodeScope *scope{};
    std::optional<NodeIfPred *> pred;
};

struct NodeStmtAssign
{
    Token ident;
    NodeExpr *expr{};
};

struct NodeStmtPrint
{
    struct NodeExpr *expr{};
};

struct NodeFunction
{
    struct NodeTermIdent *function_name;
    std::vector<NodeTermIdent *> parameters;
    struct NodeScope *scope{};
};

struct NodeFunctionCall
{
    struct NodeTermIdent *function_name;
    std::vector<NodeExpr *> arguments;
};

// statements available now
struct NodeStmt
{
    std::variant<NodeStmtExit *, NodeStmtLet *, NodeScope *, NodeStmtIf *, NodeStmtAssign *, NodeStmtPrint *, NodeFunction *, NodeFunctionCall *> var;
};

struct NodeProg
{
    std::vector<NodeStmt *> stmts;
};

class Parser
{

public:
    // vector of tokens and allocated memory as arguments to construct parse tree
    explicit Parser(std::vector<Token> tokens)
        : m_tokens(std::move(tokens)), m_allocator(1024 * 1024 * 4)
    {
    }

    // expected parsing errors
    void error_expected(const std::string &msg) const
    {
        std::cerr << "[Parse error] Expected " << msg << " on line " << peek(-1).value().line << std::endl;
        exit(EXIT_FAILURE);
    }

    std::optional<NodeTerm *> parse_term()
    {
        if (auto int_lit = try_consume(TokenType::int_lit))
        {
            auto term_int_lit = m_allocator.emplace<NodeTermIntLit>(int_lit.value());
            auto term = m_allocator.emplace<NodeTerm>(term_int_lit);
            return term;
        }
        if (auto char_lit = try_consume(TokenType::char_lit))
        {
            auto term_char_lit = m_allocator.emplace<NodeTermCharLit>(char_lit.value());
            auto term = m_allocator.emplace<NodeTerm>(term_char_lit);
            return term;
        }
        if (auto float_lit = try_consume(TokenType::float_lit))
        {
            auto term_float_lit = m_allocator.emplace<NodeTermFloatLit>(float_lit.value());
            auto term = m_allocator.emplace<NodeTerm>(term_float_lit);
            return term;
        }
        if (auto ident = try_consume(TokenType::ident))
        {
            auto term_ident = m_allocator.emplace<NodeTermIdent>(ident.value());
            auto term = m_allocator.emplace<NodeTerm>(term_ident);
            return term;
        }
        if (try_consume(TokenType::open_paren))
        {
            auto expr = parse_expr();
            if (!expr.has_value())
            {
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            auto term_paren = m_allocator.emplace<NodeTermParen>(expr.value());
            auto term = m_allocator.emplace<NodeTerm>(term_paren);
            return term;
        }
        return {};
    }

    std::optional<NodeExpr *> parse_expr(const int min_prec = 0)
    {
        std::optional<NodeTerm *> term_lhs = parse_term();
        if (!term_lhs.has_value())
        {
            return {};
        }
        auto expr_lhs = m_allocator.emplace<NodeExpr>(term_lhs.value());
        while (true)
        {
            std::optional<Token> cur_token = peek();
            std::optional<int> prec;
            if (cur_token.has_value())
            {
                prec = bin_prec(cur_token.value().type);
                if (!prec.has_value() || prec.value() < min_prec)
                {
                    break;
                }
            }
            else
            {
                break;
            }
            const Token op = consume();
            const int next_min_prec = prec.value() + 1;
            auto expr_rhs = parse_expr(next_min_prec);
            if (!expr_rhs.has_value())
            {
                error_expected("expression");
            }
            auto bin_expr = m_allocator.emplace<NodeBinExpr>();
            auto expr_lhs_new = m_allocator.emplace<NodeExpr>();
            if (op.type == TokenType::plus)
            {
                expr_lhs_new->var = expr_lhs->var;
                auto bin_expr_add = m_allocator.emplace<NodeBinExprAdd>(expr_lhs_new, expr_rhs.value());
                bin_expr->var = bin_expr_add;
            }
            else if (op.type == TokenType::star)
            {
                expr_lhs_new->var = expr_lhs->var;
                auto bin_expr_mul = m_allocator.emplace<NodeBinExprMul>(expr_lhs_new, expr_rhs.value());
                bin_expr->var = bin_expr_mul;
            }
            else if (op.type == TokenType::minus)
            {
                expr_lhs_new->var = expr_lhs->var;
                auto bin_expr_sub = m_allocator.emplace<NodeBinExprSub>(expr_lhs_new, expr_rhs.value());
                bin_expr->var = bin_expr_sub;
            }
            else if (op.type == TokenType::fslash)
            {
                expr_lhs_new->var = expr_lhs->var;
                auto bin_expr_div = m_allocator.emplace<NodeBinExprDiv>(expr_lhs_new, expr_rhs.value());
                bin_expr->var = bin_expr_div;
            }
            else if (op.type == TokenType::percent)
            {
                expr_lhs_new->var = expr_lhs->var;
                auto bin_expr_Mod = m_allocator.emplace<NodeBinExprMod>(expr_lhs_new, expr_rhs.value());
                bin_expr->var = bin_expr_Mod;
            }
            else
            {
                assert(false);
            }
            expr_lhs->var = bin_expr;
        }
        return expr_lhs;
    }

    std::optional<NodeScope *> parse_scope()
    {
        if (try_consume(TokenType::open_curly))
        {
            auto scope = m_allocator.alloc<NodeScope>();
            while (auto stmt = parse_stmt())
            {
                scope->stmts.push_back(stmt.value());
            }
            try_consume_err(TokenType::close_curly);
            return scope;
        }
        return {};
    }

    std::optional<NodeIfPred *> parse_if_pred()
    {
        if (try_consume(TokenType::elif))
        {
            try_consume_err(TokenType::open_paren);
            auto if_pred_elif = m_allocator.alloc<NodeIfPredElif>();
            if (auto expr = parse_expr())
            {
                if_pred_elif->expr = expr.value();
            }
            else
            {
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            if (auto scope = parse_scope())
            {
                if_pred_elif->scope = scope.value();
            }
            else
            {
                error_expected("scope");
            }
            if_pred_elif->pred = parse_if_pred();
            auto if_pred = m_allocator.emplace<NodeIfPred>(if_pred_elif);
            return if_pred;
        }
        if (try_consume(TokenType::_else))
        {
            auto if_pred_else = m_allocator.alloc<NodeIfPredElse>();
            if (auto scope = parse_scope())
            {
                if_pred_else->scope = scope.value();
            }
            else
            {
                error_expected("scope");
            }
            auto if_pred = m_allocator.emplace<NodeIfPred>(if_pred_else);
            return if_pred;
        }
        return {};
    }

    std::optional<NodeStmt *> parse_stmt()
    {
        if (try_consume(TokenType::exit))
        {
            try_consume_err(TokenType::open_paren);
            auto stmt_exit = m_allocator.alloc<NodeStmtExit>();
            if (auto expr_node = parse_expr())
            {
                stmt_exit->expr = expr_node.value();
            }
            else
            {
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_exit);
            return stmt;
        }
        if (try_consume(TokenType::let))
        {
            auto ident = try_consume_err(TokenType::ident);
            auto stmt_let = m_allocator.alloc<NodeStmtLet>();
            try_consume_err(TokenType::eq);
            stmt_let->ident = ident;
            if (auto expr = parse_expr())
            {
                stmt_let->expr = expr.value();
            }
            else
            {
                error_expected("expression");
            }
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_let);
            return stmt;
        }
        if (auto scope = parse_scope())
        {
            auto stmt = m_allocator.emplace<NodeStmt>(scope.value());
            return stmt;
        }
        if (try_consume(TokenType::_if))
        {
            try_consume_err(TokenType::open_paren);
            auto stmt_if = m_allocator.alloc<NodeStmtIf>();
            if (auto expr = parse_expr())
            {
                stmt_if->expr = expr.value();
            }
            else
            {
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            if (auto scope = parse_scope())
            {
                stmt_if->scope = scope.value();
            }
            else
            {
                error_expected("scope");
            }
            stmt_if->pred = parse_if_pred();
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_if);
            return stmt;
        }
        if (auto ident = try_consume(TokenType::ident))
        {
            auto stmt = m_allocator.alloc<NodeStmt>();
            if (peek().has_value() && peek().value().type == TokenType::semi)
            {
                consume();
            }
            else if (peek().has_value() && peek().value().type == TokenType::eq)
            {
                consume();
                auto stmt_assign = m_allocator.emplace<NodeStmtAssign>(ident.value());
                if (auto expr = parse_expr())
                {
                    stmt_assign->expr = expr.value();
                }
                else
                {
                    error_expected("expression");
                }
                try_consume_err(TokenType::semi);
                stmt->var = stmt_assign;
            }
            else if (try_consume(TokenType::open_paren))
            {
                std::vector<NodeExpr *> arguments;
                while (auto argument = parse_expr())
                {
                    arguments.push_back(argument.value());
                    if (peek().has_value() && peek().value().type == TokenType::close_paren)
                    {
                        break;
                    }
                    else
                    {
                        try_consume_err(TokenType::comma);
                    }
                }
                try_consume_err(TokenType::close_paren);
                try_consume_err(TokenType::semi);
                auto function_name = m_allocator.emplace<NodeTermIdent>(ident.value());
                auto function_call = m_allocator.emplace<NodeFunctionCall>(function_name, arguments);
                auto stmt = m_allocator.emplace<NodeStmt>(function_call);
                return stmt;
            }
            else
            {
                error_expected("expression");
            }
            return stmt;
        }
        if (try_consume(TokenType::print))
        {
            try_consume_err(TokenType::open_paren);
            auto expr = parse_expr();
            auto stmt_print = m_allocator.alloc<NodeStmtPrint>();
            stmt_print->expr = expr.value();
            try_consume_err(TokenType::close_paren);
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_print);
            return stmt;
        }
        if (try_consume(TokenType::function))
        {
            auto ident = try_consume_err(TokenType::ident);
            auto function_name = m_allocator.emplace<NodeTermIdent>(ident);
            try_consume_err(TokenType::open_paren);
            std::vector<NodeTermIdent *> parameters;
            while (auto ident = try_consume(TokenType::ident))
            {
                auto parameter = m_allocator.emplace<NodeTermIdent>(ident.value());
                parameters.push_back(parameter);
                if (peek().has_value() && peek().value().type == TokenType::close_paren)
                {
                    break;
                }
                else
                {
                    try_consume_err(TokenType::comma);
                }
            }
            try_consume_err(TokenType::close_paren);

            auto function = m_allocator.emplace<NodeFunction>(function_name, parameters);
            if (auto scope = parse_scope())
            {
                function->scope = scope.value();
            }
            else
            {
                error_expected("scope");
            }
            auto stmt = m_allocator.emplace<NodeStmt>(function);
            return stmt;
        }
        return {};
    }

    std::optional<NodeProg> parse_prog()
    {
        NodeProg prog;
        while (peek().has_value())
        {
            if (auto stmt = parse_stmt())
            {
                prog.stmts.push_back(stmt.value());
            }
            else
            {
                error_expected("statement");
            }
        }
        return prog;
    }

private:
    // peeking next token
    [[nodiscard]] std::optional<Token> peek(int offset = 0) const
    {
        if (m_index + offset >= m_tokens.size())
        {
            return {};
        }
        else
        {
            return m_tokens.at(m_index + offset);
        }
    }

    // consuming token
    Token consume()
    {
        return m_tokens.at(m_index++);
    }

    // consuming an expected token otherwise error
    Token try_consume_err(const TokenType type)
    {
        if (peek().has_value() && peek().value().type == type)
        {
            return consume();
        }
        error_expected(to_string(type));
        return {};
    }

    // try to consume a particular token
    std::optional<Token> try_consume(const TokenType type)
    {
        if (peek().has_value() && peek().value().type == type)
        {
            return consume();
        }
        return {};
    }

    const std::vector<Token> m_tokens;
    size_t m_index = 0;
    ArenaAllocator m_allocator;
};