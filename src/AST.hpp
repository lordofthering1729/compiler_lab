#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <vector>
#include <map>
#include <cassert>
#include <stdexcept>
#include "DCE.hpp"

// ------ Symbol Table ------
enum SymbolType
{
    CONSTANT, VAR, FUNCTION
};

struct SymbolInfo
{
    SymbolType type;
    int value; // For const/var
    std::string koopa_name;
    std::string ret_type; // For function: "int"/"void"
    std::vector<std::string> params; // function param names
    std::vector<std::string> param_types;
    bool is_global = false; // 新增字段：区分是否全局变量
};

class SymbolTable
{
public:
    SymbolTable *parent;
    std::map<std::string, SymbolInfo> table;
    int scope_id;
    int var_cnt;

    SymbolTable(SymbolTable *parent = nullptr)
        : parent(parent)
    {
        scope_id = parent ? parent->scope_id + 1 : 0;
        var_cnt = 0;
    }

    bool add(const std::string &name, SymbolType type, int value = 0, std::string koopa_name = "",
             std::string ret_type = "", const std::vector<std::string>& params = {},
             const std::vector<std::string>& param_types = {}, bool is_global = false)
    {
        if (table.count(name))
        {
            return false;
        }
        table[name] = {type, value, koopa_name, ret_type, params, param_types, is_global};
        return true;
    }

    std::string get_unique_name(const std::string &name)
    {
        var_cnt++;
        return "@" + name + "_" + std::to_string(scope_id) + "_" + std::to_string(var_cnt);
    }

    SymbolInfo *lookup(const std::string &name)
    {
        if (table.count(name))
        {
            return &table[name];
        }
        if (parent)
        {
            return parent->lookup(name);
        }
        return nullptr;
    }

    void Print(std::ostream& os = std::cout, int indent = 0) const
    {
        std::string ind = std::string(indent * 2, ' ');
        os << ind << "SymbolTable (scope_id=" << scope_id << ")\n";
        for (const auto& kv : table)
        {
            const auto& name = kv.first;
            const auto& info = kv.second;
            os << ind << "  [" << name << "] ";
            switch(info.type)
            {
                case CONSTANT:
                    os << "CONSTANT, value=" << info.value;
                    break;
                case VAR:
                    os << "VAR, value=" << info.value << ", koopa_name=" << info.koopa_name;
                    break;
                case FUNCTION:
                    os << "FUNCTION, ret_type=" << info.ret_type;
                    if(!info.params.empty())
                    {
                        os << ", params=(";
                        for(size_t i=0; i<info.params.size(); ++i)
                        {
                            os << info.param_types[i] << " " << info.params[i];
                            if(i+1 != info.params.size()) os << ", ";
                        }
                        os << ")";
                    }
                    break;
            }
            os << ", is_global=" << (info.is_global ? "true" : "false") << "\n";
        }
        if(parent)
        {
            os << ind << "  Parent:\n";
            parent->Print(os, indent+1);
        }
    }
};

inline int koopa_tmp_id = 0;
inline std::vector<std::string> break_stack;
inline std::vector<std::string> continue_stack;
inline int loop_depth = 0;

inline std::string make_indent(int indent)
{
    return std::string(indent * 2, ' ');
}

inline bool ends_with_ret(const std::vector<std::string>& code)
{
    for (auto it = code.rbegin(); it != code.rend(); ++it)
    {
        if (it->empty()) continue;
        std::string line = *it;
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.substr(0, 3) == "ret") return true;
        break;
    }
    return false;
}

inline bool ends_with_jump(const std::vector<std::string>& code)
{
    for (auto it = code.rbegin(); it != code.rend(); ++it)
    {
        if (it->empty()) continue;
        std::string line = *it;
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.find("jump ") == 0)
        {
            return true;
        }
        break;
    }
    return false;
}

// ------ SysY Library ------
inline void register_sysy_lib(SymbolTable& symtab)
{
    symtab.add("getint", FUNCTION, 0, "", "int", {}, {});
    symtab.add("getch", FUNCTION, 0, "", "int", {}, {});
    symtab.add("getarray", FUNCTION, 0, "", "int", {"arr"}, {"int[]"});
    symtab.add("putint", FUNCTION, 0, "", "void", {"x"}, {"int"});
    symtab.add("putch", FUNCTION, 0, "", "void", {"x"}, {"int"});
    symtab.add("putarray", FUNCTION, 0, "", "void", {"n", "arr"}, {"int", "int[]"});
    symtab.add("starttime", FUNCTION, 0, "", "void", {}, {});
    symtab.add("stoptime", FUNCTION, 0, "", "void", {}, {});
}

inline std::string koopa_sysy_lib_decls()
{
    std::string decls;
    decls += "decl @getint(): i32\n";
    decls += "decl @getch(): i32\n";
    decls += "decl @getarray(*i32): i32\n";
    decls += "decl @putint(i32)\n";
    decls += "decl @putch(i32)\n";
    decls += "decl @putarray(i32, *i32)\n";
    decls += "decl @starttime()\n";
    decls += "decl @stoptime()\n";
    return decls;
}
// ------ AST Base ------
class BaseAST
{
public:
    virtual ~BaseAST() = default;
    virtual void Dump(std::ostream& os, int indent = 0) const = 0;
    virtual std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const = 0;
    virtual int ConstEval(SymbolTable &symtab) const { throw std::runtime_error("Not a const expr"); }
    virtual void SemanticCheck(SymbolTable &symtab) = 0;
};

// ------ Expression ASTs ------
class NumberAST : public BaseAST
{
public:
    int value;
    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "NumberAST { " << value << " }\n";
    }
    std::string EmitKoopa(std::vector<std::string> &, SymbolTable &) const override
    {
        return std::to_string(value);
    }
    int ConstEval(SymbolTable &) const override { return value; }
    void SemanticCheck(SymbolTable &) override {}
};

class PrimaryExpAST : public BaseAST
{
public:
    bool is_number = false;
    std::unique_ptr<BaseAST> exp;
    int number_value = 0;

    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "PrimaryExpAST { ";
        if (is_number)
            os << number_value;
        else
        {
            os << "\n";
            if (exp) exp->Dump(os, indent+1);
            os << make_indent(indent);
        }
        os << " }\n";
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        if (is_number)
            return std::to_string(number_value);
        else
            return exp->EmitKoopa(code, symtab);
    }

    int ConstEval(SymbolTable &symtab) const override
    {
        if (is_number)
            return number_value;
        return exp->ConstEval(symtab);
    }

    void SemanticCheck(SymbolTable &) override {}
};

class IdentAST : public BaseAST
{
public:
    std::string name;

    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "IdentAST { " << name << " }\n";
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        auto *info = symtab.lookup(name);
        if (!info)
            throw std::runtime_error("未定义标识符: " + name + " (EmitKoopa)");
        if (info->type == CONSTANT)
            return std::to_string(info->value);
        else
        {
            if (info->is_global) {
                std::string tmp = "%" + std::to_string(koopa_tmp_id++);
                code.push_back(tmp + " = load " + info->koopa_name);
                return tmp;
            } else {
                std::string tmp = "%" + std::to_string(koopa_tmp_id++);
                code.push_back(tmp + " = load " + info->koopa_name);
                return tmp;
            }
        }
    }

    int ConstEval(SymbolTable &symtab) const override
    {
        auto *info = symtab.lookup(name);
        if (!info || info->type != CONSTANT)
            throw std::runtime_error("ConstEval要求常量: " + name);
        return info->value;
    }

    void SemanticCheck(SymbolTable &symtab) override
    {
        auto *info = symtab.lookup(name);
        if (!info)
            throw std::runtime_error("未定义标识符: " + name + " (SemanticCheck)");
    }
};

class UnaryExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> exp;

    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "UnaryExpAST { op: " << op << ", exp:\n";
        if (exp) exp->Dump(os, indent+1);
        os << make_indent(indent) << "}\n";
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        std::string val = exp->EmitKoopa(code, symtab);
        if (op == "+")
            return val;
        else if (op == "-")
        {
            std::string res = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(res + " = sub 0, " + val);
            return res;
        }
        else if (op == "!")
        {
            std::string res = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(res + " = eq " + val + ", 0");
            return res;
        }
        return val;
    }

    int ConstEval(SymbolTable &symtab) const override
    {
        int v = exp->ConstEval(symtab);
        if (op == "+") return v;
        if (op == "-") return -v;
        if (op == "!") return !v;
        throw std::runtime_error("不支持的const一元操作符: " + op);
    }

    void SemanticCheck(SymbolTable &) override {}
};

class FuncCallAST : public BaseAST
{
public:
    std::string name;
    std::vector<std::unique_ptr<BaseAST>> args;

    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "FuncCallAST " << name << "\n";
        for (auto& a : args) a->Dump(os, indent+1);
    }
    std::string EmitKoopa(std::vector<std::string>& code, SymbolTable& symtab) const override
    {
        auto *info = symtab.lookup(name);
        if (!info || info->type != FUNCTION)
            throw std::runtime_error("未定义函数: " + name);

        std::string args_str;
        for (auto& a : args)
        {
            args_str += a->EmitKoopa(code, symtab) + ", ";
        }
        if (!args_str.empty())
            args_str = args_str.substr(0, args_str.size()-2);

        if (info->ret_type == "void")
        {
            code.push_back("call @" + name + "(" + args_str + ")");
            return "";
        }
        else
        {
            std::string res = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(res + " = call @" + name + "(" + args_str + ")");
            return res;
        }
    }
    void SemanticCheck(SymbolTable& symtab) override
    {
        auto *info = symtab.lookup(name);
        if (!info || info->type != FUNCTION)
            throw std::runtime_error("未定义函数: " + name);
        if (args.size() != info->params.size())
            throw std::runtime_error("函数参数数量不匹配: " + name);
        for (auto& a : args) a->SemanticCheck(symtab);
    }
};

class BinaryExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> lhs;
    std::unique_ptr<BaseAST> rhs;

    void Dump(std::ostream& os, int indent = 0) const override {
        os << make_indent(indent) << "BinaryExpAST { op: " << op << ",\n";
        os << make_indent(indent+1) << "lhs:\n";
        if (lhs) lhs->Dump(os, indent+2);
        os << make_indent(indent+1) << "rhs:\n";
        if (rhs) rhs->Dump(os, indent+2);
        os << make_indent(indent) << "}\n";
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        if (op == "||")
        {
            std::string lhs_val = lhs->EmitKoopa(code, symtab);
            std::string cmp_lhs = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(cmp_lhs + " = ne " + lhs_val + ", 0");
            std::string true_bb = "%logic_true_" + std::to_string(koopa_tmp_id++);
            std::string false_bb = "%logic_false_" + std::to_string(koopa_tmp_id++);
            std::string end_bb = "%logic_end_" + std::to_string(koopa_tmp_id++);
            std::string tmp_alloc = symtab.get_unique_name("logic_tmp");
            code.push_back(tmp_alloc + " = alloc i32");

            code.push_back("br " + cmp_lhs + ", " + true_bb + ", " + false_bb);

            code.push_back(true_bb + ":");
            code.push_back("store 1, " + tmp_alloc);
            code.push_back("jump " + end_bb);

            code.push_back(false_bb + ":");
            std::string rhs_val = rhs->EmitKoopa(code, symtab);
            std::string cmp_rhs = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(cmp_rhs + " = ne " + rhs_val + ", 0");
            code.push_back("store " + cmp_rhs + ", " + tmp_alloc);
            code.push_back("jump " + end_bb);

            code.push_back(end_bb + ":");
            std::string res = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(res + " = load " + tmp_alloc);
            return res;
        }
        else if (op == "&&")
        {
            std::string lhs_val = lhs->EmitKoopa(code, symtab);
            std::string cmp_lhs = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(cmp_lhs + " = ne " + lhs_val + ", 0");
            std::string false_bb = "%logic_false_" + std::to_string(koopa_tmp_id++);
            std::string true_bb = "%logic_true_" + std::to_string(koopa_tmp_id++);
            std::string end_bb = "%logic_end_" + std::to_string(koopa_tmp_id++);
            std::string tmp_alloc = symtab.get_unique_name("logic_tmp");
            code.push_back(tmp_alloc + " = alloc i32");

            code.push_back("br " + cmp_lhs + ", " + true_bb + ", " + false_bb);

            code.push_back(false_bb + ":");
            code.push_back("store 0, " + tmp_alloc);
            code.push_back("jump " + end_bb);

            code.push_back(true_bb + ":");
            std::string rhs_val = rhs->EmitKoopa(code, symtab);
            std::string cmp_rhs = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(cmp_rhs + " = ne " + rhs_val + ", 0");
            code.push_back("store " + cmp_rhs + ", " + tmp_alloc);
            code.push_back("jump " + end_bb);

            code.push_back(end_bb + ":");
            std::string res = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(res + " = load " + tmp_alloc);
            return res;
        }

        // 其它二元运算保持原样
        std::string l = lhs->EmitKoopa(code, symtab);
        std::string r = rhs->EmitKoopa(code, symtab);
        std::string res = "%" + std::to_string(koopa_tmp_id++);
        std::string koopa_op;
        if (op == "+") koopa_op = "add";
        else if (op == "-") koopa_op = "sub";
        else if (op == "*") koopa_op = "mul";
        else if (op == "/") koopa_op = "div";
        else if (op == "%") koopa_op = "mod";
        else if (op == "<") koopa_op = "lt";
        else if (op == ">") koopa_op = "gt";
        else if (op == "<=") koopa_op = "le";
        else if (op == ">=") koopa_op = "ge";
        else if (op == "==") koopa_op = "eq";
        else if (op == "!=") koopa_op = "ne";
        else if (op == "&") koopa_op = "and";
        else if (op == "|") koopa_op = "or";
        else koopa_op = op;
        code.push_back(res + " = " + koopa_op + " " + l + ", " + r);
        return res;
    }

    int ConstEval(SymbolTable &symtab) const override
    {
        int lv = lhs->ConstEval(symtab);
        int rv = rhs->ConstEval(symtab);
        if (op == "+") return lv + rv;
        if (op == "-") return lv - rv;
        if (op == "*") return lv * rv;
        if (op == "/") return lv / rv;
        if (op == "%") return lv % rv;
        if (op == "==") return lv == rv;
        if (op == "!=") return lv != rv;
        if (op == "<") return lv < rv;
        if (op == ">") return lv > rv;
        if (op == "<=") return lv <= rv;
        if (op == ">=") return lv >= rv;
        if (op == "&&") return lv && rv;
        if (op == "||") return lv || rv;
        throw std::runtime_error("不支持的const二元操作符: " + op);
    }

    void SemanticCheck(SymbolTable &symtab) override
    {
        lhs->SemanticCheck(symtab);
        rhs->SemanticCheck(symtab);
    }
};

class ExpAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> lor_exp;

    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "ExpAST {\n";
        if (lor_exp) lor_exp->Dump(os, indent+1);
        os << make_indent(indent) << "}\n";
    }
    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        return lor_exp->EmitKoopa(code, symtab);
    }
    int ConstEval(SymbolTable &symtab) const override
    {
        return lor_exp->ConstEval(symtab);
    }
    void SemanticCheck(SymbolTable &) override {}
};
// ------ Statement ASTs ------

class StmtAST : public BaseAST
{
public:
    enum StmtKind
    {
        ASSIGN,
        RETURN,
        EXPR,
        BLOCK,
        WHILE_STMT,
        BREAK_STMT,
        CONTINUE_STMT
    } kind;
    std::string lval;
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> block;
    std::unique_ptr<BaseAST> while_stmt;
    bool has_exp = false;

    StmtAST() : kind(EXPR), has_exp(false) {}

    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "StmtAST { ";
        switch (kind)
        {
        case ASSIGN:
            os << "ASSIGN, lval: " << lval << ", exp:\n";
            if (exp) exp->Dump(os, indent+1);
            break;
        case RETURN:
            os << "RETURN, exp:\n";
            if (has_exp && exp) exp->Dump(os, indent+1);
            break;
        case EXPR:
            os << "EXPR, exp:\n";
            if (has_exp && exp) exp->Dump(os, indent+1);
            break;
        case BLOCK:
            os << "\n";
            if (block) block->Dump(os, indent+1);
            break;
        case WHILE_STMT:
            os << "WHILE, stmt:\n";
            if (while_stmt) while_stmt->Dump(os, indent+1);
            break;
        case BREAK_STMT:
            os << "BREAK\n";
            break;
        case CONTINUE_STMT:
            os << "CONTINUE\n";
            break;
        }
        os << make_indent(indent) << "}\n";
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        switch (kind)
        {
        case ASSIGN:
        {
            auto *info = symtab.lookup(lval);
            if (!info || info->type != VAR)
                throw std::runtime_error("赋值语句左值必须为变量: " + lval);
            std::string v = exp->EmitKoopa(code, symtab);
            code.push_back("store " + v + ", " + info->koopa_name);
            return "";
        }
        case RETURN:
        {
            if (has_exp && exp)
            {
                try
                {
                    int v = exp->ConstEval(symtab);
                    code.push_back("ret " + std::to_string(v));
                }
                catch (...)
                {
                    std::string v = exp->EmitKoopa(code, symtab);
                    code.push_back("ret " + v);
                }
            }
            else
            {
                code.push_back("ret");
            }
            return "";
        }
        case EXPR:
        {
            if (has_exp && exp)
                exp->EmitKoopa(code, symtab);
            return "";
        }
        case BLOCK:
        {
            block->EmitKoopa(code, symtab);
            return "";
        }
        case WHILE_STMT:
        {
            if (while_stmt)
                while_stmt->EmitKoopa(code, symtab);
            return "";
        }
        case BREAK_STMT:
        {
            // 关键处理
            assert(!break_stack.empty() && "break must be inside loop!");
            code.push_back("jump " + break_stack.back());
            return "";
        }
        case CONTINUE_STMT:
        {
            assert(!continue_stack.empty() && "continue must be inside loop!");
            code.push_back("jump " + continue_stack.back());
            return "";
        }
        }
        return "";
    }

    void SemanticCheck(SymbolTable &symtab) override
    {
        switch (kind)
        {
        case ASSIGN:
        {
            auto *info = symtab.lookup(lval);
            if (!info)
                throw std::runtime_error("变量未定义: " + lval);
            if (info->type != VAR)
                throw std::runtime_error("不能给常量赋值: " + lval);
            exp->SemanticCheck(symtab);
            break;
        }
        case RETURN:
        {
            if (has_exp && exp)
                exp->SemanticCheck(symtab);
            break;
        }
        case EXPR:
        {
            if (has_exp && exp)
                exp->SemanticCheck(symtab);
            break;
        }
        case BLOCK:
        {
            block->SemanticCheck(symtab);
            break;
        }
        case WHILE_STMT:
        {
            if (while_stmt)
                while_stmt->SemanticCheck(symtab);
            break;
        }
        case BREAK_STMT:
        {
            if (loop_depth == 0)
                throw std::runtime_error("break not in loop!");
            break;
        }
        case CONTINUE_STMT:
        {
            if (loop_depth == 0)
                throw std::runtime_error("continue not in loop!");
            break;
        }
        }
    }
};

// If 语句 AST

class IfStmtAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> cond;
    std::unique_ptr<BaseAST> then_stmt;
    std::unique_ptr<BaseAST> else_stmt; // nullptr表示无else
    bool has_else = false;

    void Dump(std::ostream& os, int indent = 0) const override {
        os << make_indent(indent) << "IfStmtAST {\n";
        os << make_indent(indent+1) << "cond:\n";
        if (cond) cond->Dump(os, indent+2);
        os << make_indent(indent+1) << "then:\n";
        if (then_stmt) then_stmt->Dump(os, indent+2);
        if (has_else && else_stmt) {
            os << make_indent(indent+1) << "else:\n";
            else_stmt->Dump(os, indent+2);
        }
        os << make_indent(indent) << "}\n";
    }

    void SemanticCheck(SymbolTable &symtab) override
    {
        cond->SemanticCheck(symtab);
        then_stmt->SemanticCheck(symtab);
        if (has_else && else_stmt)
            else_stmt->SemanticCheck(symtab);
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        std::string then_bb = "%then_" + std::to_string(koopa_tmp_id++);
        std::string else_bb = has_else ? "%else_" + std::to_string(koopa_tmp_id++) : "";
        std::string end_bb = "%end_" + std::to_string(koopa_tmp_id++);

        std::string cond_val = cond->EmitKoopa(code, symtab);

        code.push_back("br " + cond_val + ", " + then_bb + ", " + (has_else ? else_bb : end_bb));

        // THEN 分支
        code.push_back(then_bb + ":");
        then_stmt->EmitKoopa(code, symtab);
        if (!ends_with_ret(code) && !ends_with_jump(code))
            code.push_back("jump " + end_bb);

        // ELSE 分支
        if (has_else && else_stmt)
        {
            code.push_back(else_bb + ":");
            else_stmt->EmitKoopa(code, symtab);
            if (!ends_with_ret(code) && !ends_with_jump(code))
                code.push_back("jump " + end_bb);
        }

        code.push_back(end_bb + ":");
        return "";
    }
};

// 循环语句 AST

class WhileStmtAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> cond;
    std::unique_ptr<BaseAST> body;
    void Dump(std::ostream& os, int indent = 0) const override {/*略*/}
    void SemanticCheck(SymbolTable &symtab) override 
    {
        ++loop_depth;
        cond->SemanticCheck(symtab);
        body->SemanticCheck(symtab);
        --loop_depth;
    }
    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        std::string while_cond_bb = "%while_cond_" + std::to_string(koopa_tmp_id++);
        std::string while_body_bb = "%while_body_" + std::to_string(koopa_tmp_id++);
        std::string while_end_bb = "%while_end_" + std::to_string(koopa_tmp_id++);
        // 记录break/continue目标块
        break_stack.push_back(while_end_bb);
        continue_stack.push_back(while_cond_bb);

        code.push_back("jump " + while_cond_bb);
        code.push_back(while_cond_bb + ":");
        std::string cond_val = cond->EmitKoopa(code, symtab);
        code.push_back("br " + cond_val + ", " + while_body_bb + ", " + while_end_bb);

        code.push_back(while_body_bb + ":");
        body->EmitKoopa(code, symtab);
        if (!ends_with_ret(code) && !ends_with_jump(code))
            code.push_back("jump " + while_cond_bb);

        code.push_back(while_end_bb + ":");
        break_stack.pop_back();
        continue_stack.pop_back();
        return "";
    }
};

class BreakAST : public BaseAST 
{
public:
    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "BreakAST {}\n";
    }
    std::string EmitKoopa(std::vector<std::string>& code, SymbolTable& symtab) const override 
    {
        // 需要从上下文获取break目标块
        extern std::vector<std::string> break_stack;
        assert(!break_stack.empty() && "break must be inside loop!");
        code.push_back("jump " + break_stack.back());
        return "";
    }
    void SemanticCheck(SymbolTable&) override 
    {
        extern int loop_depth;
        if (loop_depth == 0)
        {
            throw std::runtime_error("break not in loop!");
        }
    }
};

class ContinueAST : public BaseAST 
{
public:
    void Dump(std::ostream& os, int indent = 0) const override 
    {
        os << make_indent(indent) << "ContinueAST {}\n";
    }
    std::string EmitKoopa(std::vector<std::string>& code, SymbolTable& symtab) const override 
    {
        extern std::vector<std::string> continue_stack;
        assert(!continue_stack.empty() && "continue must be inside loop!");
        code.push_back("jump " + continue_stack.back());
        return "";
    }
    void SemanticCheck(SymbolTable&) override 
    {
        extern int loop_depth;
        if (loop_depth == 0)
            throw std::runtime_error("continue not in loop!");
    }
};

// ------ Declaration ASTs ------

class ConstDeclAST : public BaseAST
{
public:
    struct Def
    {
        std::string name;
        std::unique_ptr<BaseAST> val;
    };
    bool is_global = false; // 新增：是否为全局常量
    std::vector<Def> defs;

    void Dump(std::ostream& os, int indent = 0) const override {
        os << make_indent(indent) << "ConstDeclAST {\n";
        for (size_t i = 0; i < defs.size(); ++i)
        {
            os << make_indent(indent+1) << "name: " << defs[i].name << ", val:\n";
            if (defs[i].val) defs[i].val->Dump(os, indent+2);
        }
        os << make_indent(indent) << "}\n";
    }

    std::string EmitKoopa(std::vector<std::string> &, SymbolTable &symtab) const override
    {
        std::cerr << "you should see me in constdecl\n";
        for (const auto &def : defs)
        {
            int v = def.val->ConstEval(symtab);
            symtab.add(def.name, CONSTANT, v, "", "", {}, {}, is_global);
        }
        std::cerr << "constdecl done\n";
        return "";
    }

    void SemanticCheck(SymbolTable &symtab) override
    {
        for (auto &def : defs)
        {
            int v = def.val->ConstEval(symtab);
            if (!symtab.add(def.name, CONSTANT, v, "", "", {}, {}, is_global))
                throw std::runtime_error("重复定义: " + def.name);
        }
    }
};

class VarDeclAST : public BaseAST
{
public:
    struct Def
    {
        std::string name;
        std::unique_ptr<BaseAST> val;
        bool has_init;
    };
    bool is_global = false; // 新增：是否为全局变量,可以写在外面，因为VarDeclAST是整体的
    std::vector<Def> defs;

    void Dump(std::ostream& os, int indent = 0) const override {
        os << make_indent(indent) << "VarDeclAST {\n";
        for (size_t i = 0; i < defs.size(); ++i)
        {
            os << make_indent(indent+1) << "name: " << defs[i].name;
            if (defs[i].has_init) {
                os << ", init:\n";
                if (defs[i].val) defs[i].val->Dump(os, indent+2);
            }
            os << "\n";
        }
        os << make_indent(indent) << "}\n";
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        if (is_global)
        {
            std::string koopa_ir_global;
            for (const auto &def : defs)
            {
                std::cerr << "you should see me in global vardecl\n";
                // 全局变量分配 Koopa IR
                std::string koopa_name = "@" + def.name;
                std::string init_val = def.has_init ? std::to_string(def.val->ConstEval(symtab)) : "zeroinit";
                koopa_ir_global += "global " + koopa_name + " = alloc i32, " + init_val + "\n";
                symtab.add(def.name, VAR, def.has_init ? def.val->ConstEval(symtab) : 0, koopa_name, "", {}, {}, true);
            }
            return koopa_ir_global;
        }
        else
        {
            for (const auto &def : defs)
            {
                std::string alloc_name = symtab.get_unique_name(def.name);
                code.push_back(alloc_name + " = alloc i32");
                symtab.add(def.name, VAR, 0, alloc_name);
                if (def.has_init)
                {
                    std::string v = def.val->EmitKoopa(code, symtab);
                    code.push_back("store " + v + ", " + alloc_name);
                }
            }
            return "";
        }
    }

    void SemanticCheck(SymbolTable &symtab) override
    {
        if (is_global)
        {
            for (const auto &def : defs)
            {
                std::cerr << "you should see me in global vardecl\n";
                // 全局变量分配 Koopa IR
                std::string koopa_name = "@" + def.name;
                std::string init_val = def.has_init ? std::to_string(def.val->ConstEval(symtab)) : "zeroinit";
                if (!symtab.add(def.name, VAR, def.has_init ? def.val->ConstEval(symtab) : 0, koopa_name, "", {}, {}, true))
                    throw std::runtime_error("重复定义: " + def.name);
            }
        }
        else
        {
            for (const auto &def : defs)
            {
                if (!symtab.add(def.name, VAR, 0, "", "", {}, {}, 0))
                    throw std::runtime_error("重复定义: " + def.name);
            }
        }
    }
};

class BlockAST : public BaseAST
{
public:
    std::vector<std::unique_ptr<BaseAST>> items;

    void Dump(std::ostream& os, int indent = 0) const override {
        os << make_indent(indent) << "BlockAST {\n";
        for (auto &item : items)
            item->Dump(os, indent+1);
        os << make_indent(indent) << "}\n";
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &parent_tab) const override
    {
        SymbolTable local_tab(&parent_tab);
        for (auto &item : items)
        {
            StmtAST *stmt = dynamic_cast<StmtAST *>(item.get());
            if (stmt)
            {
                switch (stmt->kind)
                {
                case StmtAST::BREAK_STMT:
                case StmtAST::CONTINUE_STMT:
                case StmtAST::RETURN:
                    stmt->EmitKoopa(code, local_tab);
                    return ""; // 或break;
                default:
                    stmt->EmitKoopa(code, local_tab);
                    break;
                }
            }
            else
            {
                item->EmitKoopa(code, local_tab);
            }
        }
        return "";
    }

    void SemanticCheck(SymbolTable &parent_tab) override
    {
        SymbolTable local_tab(&parent_tab);
        for (auto &item : items)
            item->SemanticCheck(local_tab);
    }
};
// ------ Function ASTs ------

// ------ Function Param AST ------
class FuncTypeAST : public BaseAST
{
public:
    std::string type;
    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "FuncTypeAST { " << type << " }\n";
    }
    std::string EmitKoopa(std::vector<std::string> &, SymbolTable &) const override
    {
        return type == "int" ? "i32 " : "";
    }
    void SemanticCheck(SymbolTable &) override {}
};

class FuncFParamAST : public BaseAST
{
public:
    std::string type; // "int"
    std::string name;
    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "Param " << type << " " << name << "\n";
    }
    std::string EmitKoopa(std::vector<std::string> &, SymbolTable &) const override { return ""; }
    void SemanticCheck(SymbolTable &) override {}
};

class FuncFParamsAST : public BaseAST
{
public:
    std::vector<std::unique_ptr<FuncFParamAST>> params;
    void Dump(std::ostream& os, int indent = 0) const override
    {
        for (auto& p : params) p->Dump(os, indent);
    }
    std::string EmitKoopa(std::vector<std::string>&, SymbolTable&) const override { return ""; }
    void SemanticCheck(SymbolTable&) override {}
};

class FuncDefAST : public BaseAST
{
public:
    std::string ret_type;
    std::string ident;
    std::unique_ptr<FuncFParamsAST> params;
    std::unique_ptr<BaseAST> block;

    void Dump(std::ostream& os, int indent = 0) const override
    {
        os << make_indent(indent) << "FuncDefAST " << ret_type << " " << ident << "\n";
        if (params) params->Dump(os, indent+1);
        if (block) block->Dump(os, indent+1);
    }

    std::string EmitKoopa(std::vector<std::string>& code, SymbolTable& global_tab) const override
    {
        std::vector<std::string> param_names, param_types;
        if (params)
        {
            for (auto& p : params->params)
            {
                param_names.push_back(p->name);
                param_types.push_back(p->type);
            }
        }
        global_tab.add(ident, FUNCTION, 0, "", ret_type, param_names, param_types);

        koopa_tmp_id = 0;
        code.clear();

        std::string params_str;
        for (size_t i = 0; i < param_names.size(); ++i)
        {
            params_str += "@" + param_names[i] + ": ";
            if (param_types[i] == "int")
                params_str += "i32";
            else if (param_types[i] == "int[]")
                params_str += "*i32";
            // 可扩展更多类型
            else
            {
                std::cerr << "Warning: 未知参数类型 " << param_types[i] << "\n";
                params_str += "unknown";
            }
            params_str += ", ";
        }
        if (!params_str.empty())
        {
            params_str = params_str.substr(0, params_str.size() - 2);
        }

        std::string func_head = "fun @" + ident + "(" + params_str + ")";
        if (ret_type == "int")
        {
            func_head += ": i32";
        }
        func_head += "{\n";
        code.push_back("%entry:");
        SymbolTable local_tab(&global_tab);
        for (size_t i = 0; i < param_names.size(); ++i)
        {
            std::string var_name = "%" + param_names[i];
            code.push_back(var_name + " = alloc i32");
            local_tab.add(param_names[i], VAR, 0, var_name);
            code.push_back("store @" + param_names[i] + ", " + var_name);
        }

        block->EmitKoopa(code, local_tab);

         // void 函数自动添加 ret
        if (ret_type == "void") 
        {
            bool found_ret = false;
            for (auto it = code.rbegin(); it != code.rend(); it++) 
            {
                std::string line = *it;
                line.erase(0, line.find_first_not_of(" \t"));
                if (line.substr(0, 3) == "ret") 
                {
                    found_ret = true;
                    break;
                }
                if (!line.empty()) break;
            }
            if (!found_ret) 
            {
                code.push_back("ret");
            }
        }

        std::string koopa;
        koopa += func_head;
        koopa += EmitKoopaWithDCE(code);
        koopa += "}\n";
        return koopa;
    }

    void SemanticCheck(SymbolTable& global_tab) override
    {
        std::vector<std::string> param_names, param_types;
        if (params)
        {
            for (auto& p : params->params)
            {
                param_names.push_back(p->name);
                param_types.push_back(p->type);
            }
        }
        if (!global_tab.add(ident, FUNCTION, 0, "", ret_type, param_names, param_types))
        {
            throw std::runtime_error("重复定义函数: " + ident);
        }
        block->SemanticCheck(global_tab);
    }
};

