#pragma once

#include<optional>
#include<iostream>
#include<variant>

#include "./arena.hpp"
#include "./tokenization.hpp"

struct NodeTermIntLit{
    Token int_lit;
};

struct NodeTermIdent{
    Token ident;
};

struct NodeExpr;

struct NodeTermParen{
    NodeExpr* expr;
};

struct NodeBinExprAdd{
    NodeExpr* lhs;
    NodeExpr* rhs;
};

struct NodeBinExprSub{
    NodeExpr* lhs;
    NodeExpr* rhs;
};

struct NodeBinExprMul{
    NodeExpr* lhs;
    NodeExpr* rhs;
};

struct NodeBinExprDiv{
    NodeExpr* lhs;
    NodeExpr* rhs;
};

struct NodeBinExprMod{
    NodeExpr* lhs;
    NodeExpr* rhs;
};

struct NodeBinExpr{
    std::variant<NodeBinExprAdd*, NodeBinExprMul*, NodeBinExprSub*, NodeBinExprDiv*, NodeBinExprMod*> var;
};

struct NodeTerm{
    std::variant<NodeTermIntLit*, NodeTermIdent*, NodeTermParen*> var;
};

struct NodeExpr{
    std::variant<NodeTerm*, NodeBinExpr*> var;
};

struct NodeStmtExit{
    NodeExpr* expr;
};

struct NodeStmtLet{
    Token ident;
    NodeExpr* expr {};
};

struct NodeStmt;

struct NodeScope{
    std::vector<NodeStmt*> stmts;
};

struct NodeIfPred;

struct NodeIfPredElif{
    NodeExpr* expr {};
    NodeScope* scope {};
    std::optional<NodeIfPred*> pred;
};

struct NodeIfPredElse{
    NodeScope* scope;
};

struct NodeIfPred{
    std::variant<NodeIfPredElif*, NodeIfPredElse*> var; 
};

struct NodeStmtIf{
    struct NodeExpr* expr {};
    struct NodeScope* scope {};
    std::optional<NodeIfPred*> pred;
};

struct NodeStmtAssign{
    Token ident;
    NodeExpr* expr {};
};

struct NodeStmt{
    std::variant<NodeStmtExit*, NodeStmtLet*, NodeScope*, NodeStmtIf*, NodeStmtAssign*> var;
};

struct NodeProg{
    std::vector<NodeStmt*> stmts;
};

class Parser{

public:

    explicit Parser(std::vector<Token> tokens)
        : m_tokens(std::move(tokens))
        , m_allocator(1024* 1024* 4)
    {
    }

    void error_expected(const std::string& msg) const
    {
        std::cerr << "[Parse error] Expected " << msg << " on line " << peek(-1).value().line << std::endl;
        exit(EXIT_FAILURE);
    }
   
    std::optional<NodeTerm*> parse_term()
    {
        if(auto int_lit = try_consume(TokenType::int_lit)){
            auto term_int_lit = m_allocator.emplace<NodeTermIntLit>(int_lit.value());
            auto term = m_allocator.emplace<NodeTerm>(term_int_lit);
            return term;
        }
        if(auto ident = try_consume(TokenType::ident)){
            auto term_ident = m_allocator.emplace<NodeTermIdent>(ident.value());
            auto term = m_allocator.emplace<NodeTerm>(term_ident);
            return term;
        }
        if(try_consume(TokenType::open_paren)){
            auto expr = parse_expr();
            if(!expr.has_value()){
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            auto term_paren = m_allocator.emplace<NodeTermParen>(expr.value());
            auto term = m_allocator.emplace<NodeTerm>(term_paren);
            return term;
        }       
        return {};
    }

    std::optional<NodeExpr*> parse_expr(const int min_prec = 0)
    {
        std::optional<NodeTerm*> term_lhs = parse_term();
        if(!term_lhs.has_value()){
            return {};
        }
        auto expr_lhs = m_allocator.emplace<NodeExpr>(term_lhs.value());
        while(true){
            std::optional<Token> cur_token = peek();
            std::optional<int> prec;
            if(cur_token.has_value()){
                prec = bin_prec(cur_token.value().type);
                if(!prec.has_value() || prec.value() <  min_prec){
                    break;
                }
            }
            else{
                break;
            }
            const Token op = consume();
            const int next_min_prec = prec.value() + 1;
            auto expr_rhs = parse_expr(next_min_prec);
            if(!expr_rhs.has_value()){
               error_expected("expression");
            }
            auto bin_expr = m_allocator.emplace<NodeBinExpr>();
            auto expr_lhs_new = m_allocator.emplace<NodeExpr>();
            if(op.type == TokenType::plus){
                expr_lhs_new->var = expr_lhs->var;
                auto bin_expr_add = m_allocator.emplace<NodeBinExprAdd>(expr_lhs_new, expr_rhs.value());
                bin_expr->var = bin_expr_add;
            }
            else if(op.type == TokenType::star){
                expr_lhs_new->var = expr_lhs->var;
                auto bin_expr_mul = m_allocator.emplace<NodeBinExprMul>(expr_lhs_new, expr_rhs.value());
                bin_expr->var = bin_expr_mul;
            }
            else if(op.type == TokenType::minus){
                expr_lhs_new->var = expr_lhs->var;
                auto bin_expr_sub = m_allocator.emplace<NodeBinExprSub>(expr_lhs_new, expr_rhs.value());
                bin_expr->var = bin_expr_sub;
            }
            else if(op.type == TokenType::fslash){
                expr_lhs_new->var = expr_lhs->var;
                auto bin_expr_div = m_allocator.emplace<NodeBinExprDiv>(expr_lhs_new, expr_rhs.value());
                bin_expr->var = bin_expr_div;
            }
            else if(op.type == TokenType::percent){
                expr_lhs_new->var = expr_lhs->var;
                auto bin_expr_Mod = m_allocator.emplace<NodeBinExprMod>(expr_lhs_new, expr_rhs.value());
                bin_expr->var = bin_expr_Mod;
            }else{
                assert(false);
            }
            expr_lhs->var = bin_expr;
        }
        return expr_lhs;
    }
    
    std::optional<NodeScope*> parse_scope()
    {
        if(try_consume(TokenType::open_curly)){
            auto scope = m_allocator.alloc<NodeScope>();
            while(auto stmt = parse_stmt()){
                scope->stmts.push_back(stmt.value());
            }
            try_consume_err(TokenType::close_curly);
            return scope;     
        }
        return {};
    }

    std::optional<NodeIfPred*> parse_if_pred()
    {
        if(try_consume(TokenType::elif)){
            try_consume_err(TokenType::open_paren);
            auto if_pred_elif = m_allocator.alloc<NodeIfPredElif>();
            if(auto expr = parse_expr()){
                if_pred_elif->expr = expr.value();
            }
            else{
               error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            if(auto scope = parse_scope()){
                if_pred_elif->scope = scope.value();
            }
            else{
                error_expected("scope");
            }
            if_pred_elif->pred = parse_if_pred();
            auto if_pred = m_allocator.emplace<NodeIfPred>(if_pred_elif);
            return if_pred;
        }
        if(try_consume(TokenType::_else)){
            auto if_pred_else = m_allocator.alloc<NodeIfPredElse>();
            if(auto scope = parse_scope()){
                if_pred_else->scope = scope.value();
            }
            else{
                error_expected("scope");
            }
            auto if_pred = m_allocator.emplace<NodeIfPred>(if_pred_else);
            return if_pred;
        }
        return {};
    }

    std::optional<NodeStmt*> parse_stmt()
    {
        if(try_consume(TokenType::exit)){
            try_consume_err(TokenType::open_paren);
            auto stmt_exit = m_allocator.alloc<NodeStmtExit>();
            if(auto expr_node = parse_expr()){
                stmt_exit->expr = expr_node.value();
            }
            else{
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_exit);
            return stmt;
        }
        if(try_consume(TokenType::let)){
            auto ident = try_consume_err(TokenType::ident);
            auto stmt_let = m_allocator.emplace<NodeStmtLet>(ident);
            try_consume_err(TokenType::eq);
            if(auto expr = parse_expr()){
                stmt_let->expr = expr.value();
            }
            else{
                error_expected("expression");
            }
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_let);
            return stmt;
        }
        if(auto scope = parse_scope()){
            auto stmt = m_allocator.emplace<NodeStmt>(scope.value());
            return stmt;
        }
        if(try_consume(TokenType::_if)){
            try_consume_err(TokenType::open_paren);
            auto stmt_if = m_allocator.alloc<NodeStmtIf>();
            if(auto expr = parse_expr()){
                stmt_if->expr = expr.value();
            }else{
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            if(auto scope = parse_scope()){
                stmt_if->scope = scope.value();
            }
            else{
                error_expected("scope");
            }
            stmt_if->pred = parse_if_pred();
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_if);
            return stmt;
        }
        if(auto ident = try_consume(TokenType::ident)){
            auto stmt = m_allocator.alloc<NodeStmt>();
            if(peek().has_value() && peek().value().type == TokenType::semi){
                consume();
            }else if(peek().has_value() && peek().value().type == TokenType::eq){
                consume();
                auto stmt_assign = m_allocator.emplace<NodeStmtAssign>(ident.value());
                if(auto expr = parse_expr()){
                    stmt_assign->expr = expr.value();
                }else{
                    error_expected("expression");
                }
                try_consume_err(TokenType::semi);
                stmt->var = stmt_assign;
            }else{
                error_expected("expression");
            }
            return stmt;
        }
        return {};
    }

    std::optional<NodeProg> parse_prog()
    {
        NodeProg prog;
        while(peek().has_value()){
            if(auto stmt = parse_stmt()){
                prog.stmts.push_back(stmt.value());
            }else{
                error_expected("statement");
            }
        }
        return prog;
    }

private:

    [[nodiscard]] std::optional<Token> peek(int offset = 0) const
    {
        if(m_index + offset >= m_tokens.size()){
            return  {};
        }
        else{
            return m_tokens.at(m_index + offset);
        }
    }

    Token consume () 
    {
        return m_tokens.at(m_index++);
    }

    Token try_consume_err(const TokenType type)
    {
        if(peek().has_value() && peek().value().type == type){
            return consume();
        }
        error_expected(to_string(type));
        return {};
    }

    std::optional<Token> try_consume(const TokenType type)
    {
        if(peek().has_value() && peek().value().type == type){
            return consume();
        }
        return {};
    }

    const std::vector<Token> m_tokens;
    size_t m_index = 0;
    ArenaAllocator m_allocator;

};