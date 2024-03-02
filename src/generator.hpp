#pragma once

#include <sstream>
#include <cassert>
#include <algorithm>

#include "./parser.hpp"

class Generator
{

public:
    // takes parsed tree as argument
    explicit Generator(NodeProg prog)
        : m_prog(std::move(prog))
    {
    }

    void gen_term(const NodeTerm *term)
    {
        struct TermVisitor
        {
            Generator &gen;
            void operator()(const NodeTermIntLit *term_int_lit) const
            {
                // if a term is integer literal, move the value to rax register and push onto stack
                gen.m_output << "    mov rax, " << term_int_lit->int_lit.value.value() << "\n";
                gen.push("rax");
                gen.m_var_byte_size = 4;
            }

            void operator()(const NodeTermCharLit *term_char_lit) const
            {
                gen.m_output << "    mov rax, " << term_char_lit->char_lit.value.value() << "\n";
                gen.push("rax");
                gen.m_var_byte_size = 1;
            }

            
            void operator()(const NodeTermFloatLit *term_float_lit) const
            {
                gen.m_output << "    mov rax, " << term_float_lit->float_lit.value.value() << "\n";
                gen.push("rax");
                gen.m_var_byte_size = 8;
            }

            void operator()(const NodeTermIdent *term_ident) const
            {
                // if a term is an identifier, search the list of identifiers in m_vars in reverse
                // by searching in reverse order, it finds the identifier in local scope and then global scope
                const auto it = std::find_if(gen.m_vars.rbegin(), gen.m_vars.rend(), [&](const Var &var)
                                             { return var.name == term_ident->ident.value.value(); });
                if (it == gen.m_vars.rend())
                {
                    std::cerr << "Undeclared identifier: " << term_ident->ident.value.value() << std::endl;
                    exit(EXIT_FAILURE);
                }
                // pushing (copy) the value of identifier on top of the stack
                // its location is found by -> total stack size - location of identifier
                std::stringstream offset;
                offset << "QWORD [rsp + " << (gen.m_stack_size - it->stack_loc) * 8 << "]";
                gen.push(offset.str());
            }
            void operator()(const NodeTermParen *term_paren) const
            {
                // if a term is an expression, parse the expression
                gen.gen_expr(term_paren->expr);
            }
        };
        TermVisitor visitor{.gen = *this};
        std::visit(visitor, term->var);
    }

    void gen_bin_expr(const NodeBinExpr *bin_expr)
    {
        struct BinExprVisitor
        {
            Generator &gen;
            // lhs and rhs of binary expression is an expression, so generate it
            // lhs of binary expression is at the top of the stack
            // rhs of binary expression is at one below top of the stack
            // popping the values onto registers, perform operations and store onto top of the stack
            void operator()(const NodeBinExprAdd *bin_expr_add) const
            {
                gen.gen_expr(bin_expr_add->rhs);
                gen.gen_expr(bin_expr_add->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    add rax, rbx\n";
                gen.push("rax");
            }
            void operator()(const NodeBinExprSub *bin_expr_sub) const
            {
                gen.gen_expr(bin_expr_sub->rhs);
                gen.gen_expr(bin_expr_sub->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    sub rax, rbx\n";
                gen.push("rax");
            }
            void operator()(const NodeBinExprMul *bin_expr_mul) const
            {
                gen.gen_expr(bin_expr_mul->rhs);
                gen.gen_expr(bin_expr_mul->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    mul rbx\n"; // unsigned multiplication - rax = rax * rbx
                gen.push("rax");
            }
            void operator()(const NodeBinExprDiv *bin_expr_div) const
            {
                gen.gen_expr(bin_expr_div->rhs);
                gen.gen_expr(bin_expr_div->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    div rbx\n"; // unsigned division - rax = rax / rbx
                gen.push("rax");
            }
            void operator()(const NodeBinExprMod *bin_expr_mod) const
            {
                gen.gen_expr(bin_expr_mod->rhs);
                gen.gen_expr(bin_expr_mod->lhs);
                gen.m_output << "    xor rdx, rdx\n"; // setting rdx to 0
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    div rbx\n"; // on division of rax / rbx, the remainder is stored in rdx
                gen.m_output << "    mov rax, rdx\n";
                gen.push("rax");
            }
        };
        BinExprVisitor visitor{.gen = *this};
        std::visit(visitor, bin_expr->var);
    }

    void gen_expr(const NodeExpr *expr)
    {
        struct ExprVisitor
        {
            Generator &gen;
            void operator()(const NodeTerm *term) const
            {
                // if the expression is a term, generate the term
                gen.gen_term(term);
            }
            void operator()(const NodeBinExpr *bin_expr) const
            {
                // if the expression is a binary expression, generate the binary expression
                gen.gen_bin_expr(bin_expr);
            }
        };
        ExprVisitor visitor{.gen = *this};
        std::visit(visitor, expr->var);
    }

    void gen_scope(const NodeScope *scope)
    {
        // start the scope (resolve conflict b/w global and local variables) , generate statments, end the scope (delete local variables)
        begin_scope();
        for (const NodeStmt *stmt : scope->stmts)
        {
            gen_stmt(stmt);
        }
        end_scope();
    }

    void gen_if_pred(const NodeIfPred *if_pred, const std::string &end_label)
    {
        struct IfPredVisitor
        {
            Generator &gen;
            const std::string &end_label;
            void operator()(const NodeIfPredElif *if_pred_elif) const
            {
                gen.gen_expr(if_pred_elif->expr);       // generate expression for elif and now is at top of stack
                std::string label = gen.create_label(); // same procedure as of if stmt
                gen.pop("rax");
                gen.m_output << "    test rax, rax\n";
                gen.m_output << "    jz " << label << "\n";
                gen.gen_scope(if_pred_elif->scope);
                gen.m_output << "    jmp " << end_label << "\n";
                gen.m_output << label << ":\n";
                if (if_pred_elif->pred.has_value()) // if elif is followed by other elif or else, generate it
                {
                    gen.gen_if_pred(if_pred_elif->pred.value(), end_label);
                }
            }
            void operator()(const NodeIfPredElse *if_pred_else) const
            {
                gen.gen_scope(if_pred_else->scope); // for else, only generate the stmt, end label and others is taken care of by its if
            }
        };
        IfPredVisitor visitor{.gen = *this, .end_label = end_label};
        std::visit(visitor, if_pred->var);
    }

    void gen_stmt(const NodeStmt *stmt)
    {
        struct StmtVisitor
        {
            Generator &gen;
            void operator()(const NodeStmtExit *stmt_exit) const
            {
                gen.gen_expr(stmt_exit->expr);       // generate the expression in exit function
                gen.m_output << "    mov rax, 60\n"; // code 60 for exit in rax
                gen.pop("rdi");                      // value to be returned in rdi
                gen.m_output << "    syscall\n";
            }
            void operator()(const NodeStmtLet *stmt_let) const
            {
                // generate the expression to be stored and now it is at top of stack
                gen.gen_expr(stmt_let->expr);
                // calculating offset to find if an identifier with same name already exists in the same scope
                //  offset = total no. of identifiers before the start of current scope
                const int offset = gen.m_scopes.empty() ? 0 : gen.m_scopes.back();
                // then searching for the identifier from the start of current scope and to the last
                auto it = std::find_if(gen.m_vars.cbegin() + offset, gen.m_vars.cend(), [&](const Var &var)
                                       { return var.name == stmt_let->ident.value.value(); });
                if (it != gen.m_vars.cend())
                {
                    std::cerr << "Identifier already used: " << stmt_let->ident.value.value() << std::endl;
                    exit(EXIT_FAILURE);
                }

                // storing the name of the identifier and its location in stack (currently top) in m_vars
                gen.m_vars.push_back({.name = stmt_let->ident.value.value(), .stack_loc = gen.m_stack_size, .byte_size = gen.m_var_byte_size});
                gen.m_var_byte_size = 0;
            }
            void operator()(const NodeScope *scope) const
            {
                gen.gen_scope(scope); // generate scope (i.e: set of statements )
            }
            void operator()(const NodeStmtIf *stmt_if) const
            {
                gen.gen_expr(stmt_if->expr); // generate the expression to be checked and now is at top of stack
                std::string end_label{};     // if if-statement has else or elif followed, creates the end label
                if (stmt_if->pred.has_value())
                {
                    end_label = gen.create_label();
                }
                std::string label = gen.create_label();
                gen.pop("rax");
                gen.m_output << "    test rax, rax\n";      // test performs and of two operands and if it is 0, it sets the zero flag to 1 otherwise 0
                gen.m_output << "    jz " << label << "\n"; // jz - jumps to the label if previous command resulted in zero flag being set othewise not (i.e: condition is false)
                gen.gen_scope(stmt_if->scope);              // generate the if-scope - stmts to be executed if condition is true
                if (stmt_if->pred.has_value())              // if else or elif present
                {
                    gen.m_output << "    jmp " << end_label << "\n";   // if if-condition is true, jump to end label
                    gen.m_output << label << ":\n";                    // label for following else or elif, if if-condition is false, this is executed
                    gen.gen_if_pred(stmt_if->pred.value(), end_label); // generate else or elif block
                    gen.m_output << end_label << ":\n";                // if else of elif is true, jump to end label
                }
                else
                {
                    gen.m_output << label << ":\n"; // only if is present, one label is put on end of if
                }
            }
            void operator()(const NodeStmtAssign *stmt_assign) const
            {
                gen.gen_expr(stmt_assign->expr); // generate the expression to be assigned and is now at the top of stack
                // search for the identifier in reverse order to find the identifier in local scope and then global scope
                auto it = std::find_if(gen.m_vars.rbegin(), gen.m_vars.rend(), [&](const Var &var)
                                       { return var.name == stmt_assign->ident.value.value(); });
                if (it == gen.m_vars.rend())
                {
                    std::cerr << "Undeclared identifier: " << stmt_assign->ident.value.value() << std::endl;
                    exit(EXIT_FAILURE);
                }
                gen.pop("rax");                                                                            // store the expression at rax
                gen.m_output << "    mov [rsp + " << (gen.m_stack_size - it->stack_loc) * 8 << "], rax\n"; // find the location of the identifier at stack and store the expression at rax into it
            }
            void operator()(const NodeFunction *function) const
            {
                // auto function_label = gen.create_label();
                // gen.m_output << "    jmp " << function_label << "\n";
                // gen.m_output << function->function_name->ident.value.value() << ":\n";
                // gen.gen_scope(function->scope);
                // gen.m_output << "    ret\n";
                // gen.m_output << function_label << ":\n";
                assert(false);
            }
            void operator()(const NodeFunctionCall *function_call) const
            {
                assert(false);
                // gen.m_output << "    call " << function_call->function_name->ident.value.value() << "\n";
    
            }
            void operator()(const NodeStmtPrint *stmt_print) const
            {
                gen.gen_expr(stmt_print->expr);
                // gen.pop("rax");
                // gen.m_output << "    mov edi, buffer + 15\n";
                // gen.m_output << "    mov dword [edi], 0x0A\n";
                // gen.m_output << "convert:\n";
                // gen.m_output << "    dec edi\n";
                // gen.m_output << "    xor edx, edx\n";
                // gen.m_output << "    mov ecx, 10\n";
                // gen.m_output << "    div ecx\n";
                // gen.m_output << "    add dl, '0'\n";
                // gen.m_output << "    mov [edi], dl\n";
                // gen.m_output << "    test eax, eax\n";
                // gen.m_output << "    jnz convert\n";

                // gen.m_output << "    mov eax, 4\n";
                // gen.m_output << "    mov ebx, 1\n";
                // gen.m_output << "    lea ecx, [edi]\n";
                // gen.m_output << "    mov edx, buffer\n";
                // gen.m_output << "    sub edx, ecx\n";
                // gen.m_output << "    int 0x80\n";
                // gen.m_bss << "section .bss\n";
                // gen.m_bss << "    buffer resd 1\n";
                
            }
        };
        StmtVisitor visitor{.gen = *this};
        std::visit(visitor, stmt->var);
    }

    std::string gen_prog()
    {
        m_output << "global _start\n_start:\n"; //_start or main of the program
        for (const NodeStmt *stmt : m_prog.stmts)
        {
            gen_stmt(stmt); // generate each statement
        }
        // implicit exit with 0 after successful completion of program
        m_output << "    mov rax, 60\n";
        m_output << "    mov rdi, 0\n";
        m_output << "    syscall\n";
        m_output << m_bss.str();
        return m_output.str();
    }

private:
    // push register value to top of stack / pop top of stack to register and increment / decrement stack size for keeping track of identifiers
    void push(const std::string &reg)
    {
        m_output << "    push " << reg << "\n";
        m_stack_size++;
    }

    void pop(const std::string &reg)
    {
        m_output << "    pop " << reg << "\n";
        m_stack_size--;
    }

    void begin_scope()
    {
        // adding total no. of variables in the program before the start of scope to m_scopes
        m_scopes.push_back(m_vars.size());
    }

    void end_scope()
    {
        // to remove local varibles after the end of scope
        // finding no. of variables to be removed = total no. of variables - total no. of variables before the start of scope
        const size_t pop_count = m_vars.size() - m_scopes.back();
        if (pop_count != 0)
        {
            m_output << "    add rsp, " << pop_count * 8 << "\n"; // increasing rsp reduces stack size
        }
        m_stack_size -= pop_count;
        for (int i = 0; i < pop_count; i++)
        {
            m_vars.pop_back(); // pop the local variables from m_vars
        }
        m_scopes.pop_back(); // pop the scope from m_scope
    }

    std::string create_label()
    {
        return "label" + std::to_string(label_count++); // create distinct labels for looping and branching stmts
    }

    struct Var
    {
        std::string name;
        size_t stack_loc;
        size_t byte_size;
    };

    const NodeProg m_prog;          // parsed tree
    std::stringstream m_output;     // final assembly code
    size_t m_stack_size = 0;        // size of stack in assembly code
    std::vector<Var> m_vars{};      // variables in program
    std::vector<size_t> m_scopes{}; // for local variables in a scope
    size_t label_count = 0;         // for creating distinct labels
    std::stringstream m_bss;
    size_t m_var_byte_size = 0;
};