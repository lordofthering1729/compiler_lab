#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <vector>
#include <map>
#include <cassert>
#include <stdexcept>
#include "DCE.hpp"

// ------ Symbol Table and BaseAST ------

enum SymbolType
{
    CONSTANT, VAR
};

struct SymbolInfo
{
    SymbolType type;
    int value;
    std::string koopa_name;
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
        if (parent)
        {
            scope_id = parent->scope_id + 1;
        }
        else
        {
            scope_id = 0;
        }
        var_cnt = 0;
    }

    bool add(const std::string &name, SymbolType type, int value, std::string koopa_name = "")
    {
        if (table.count(name))
        {
            return false;
        }
        table[name] = {type, value, koopa_name};
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
};

class BaseAST
{
public:
    virtual ~BaseAST() = default;
    virtual void Dump(std::ostream& os, int indent = 0) const = 0;
    virtual std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const = 0;
    virtual int ConstEval(SymbolTable &symtab) const { throw std::runtime_error("Not a const expr"); }
    virtual void SemanticCheck(SymbolTable &symtab) = 0;
};

// 工具函数：生成缩进
inline std::string make_indent(int indent) {
    return std::string(indent * 2, ' ');
}

inline int koopa_tmp_id = 0;

// 判断最后一条指令是否是 ret 指令
inline bool ends_with_ret(const std::vector<std::string>& code) {
    for (auto it = code.rbegin(); it != code.rend(); ++it) {
        if (it->empty()) continue;
        std::string line = *it;
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.substr(0, 3) == "ret") return true;
        break;
    }
    return false;
}

// ------ Expression ASTs ------

class NumberAST : public BaseAST
{
public:
    int value;

    void Dump(std::ostream& os, int indent = 0) const override {
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

    void Dump(std::ostream& os, int indent = 0) const override {
        os << make_indent(indent) << "PrimaryExpAST { ";
        if (is_number)
            os << number_value;
        else {
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

    void Dump(std::ostream& os, int indent = 0) const override {
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
            std::string tmp = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(tmp + " = load " + info->koopa_name);
            return tmp;
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

    void Dump(std::ostream& os, int indent = 0) const override {
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

    void Dump(std::ostream& os, int indent = 0) const override {
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
        BLOCK
    } kind;
    std::string lval;
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> block;
    bool has_exp = false;

    StmtAST() : kind(EXPR), has_exp(false) {}

    void Dump(std::ostream& os, int indent = 0) const override {
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
        }
    }
};

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
        if (!ends_with_ret(code))
            code.push_back("jump " + end_bb);

        // ELSE 分支
        if (has_else && else_stmt)
        {
            code.push_back(else_bb + ":");
            else_stmt->EmitKoopa(code, symtab);
            if (!ends_with_ret(code))
                code.push_back("jump " + end_bb);
        }

        code.push_back(end_bb + ":");
        return "";
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
        for (const auto &def : defs)
        {
            int v = def.val->ConstEval(symtab);
            symtab.add(def.name, CONSTANT, v);
        }
        return "";
    }

    void SemanticCheck(SymbolTable &symtab) override
    {
        for (const auto &def : defs)
        {
            int v = def.val->ConstEval(symtab);
            if (!symtab.add(def.name, CONSTANT, v))
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

    void SemanticCheck(SymbolTable &symtab) override
    {
        for (const auto &def : defs)
        {
            if (def.has_init)
                def.val->SemanticCheck(symtab);
            if (!symtab.add(def.name, VAR, 0))
                throw std::runtime_error("重复定义: " + def.name);
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
            item->EmitKoopa(code, local_tab);
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

class FuncTypeAST : public BaseAST
{
public:
    std::string type;
    void Dump(std::ostream& os, int indent = 0) const override {
        os << make_indent(indent) << "FuncTypeAST { " << type << " }\n";
    }
    std::string EmitKoopa(std::vector<std::string> &, SymbolTable &) const override
    {
        return "i32 ";
    }
    void SemanticCheck(SymbolTable &) override {}
};

class FuncDefAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;
    void Dump(std::ostream& os, int indent = 0) const override {
        os << make_indent(indent) << "FuncDefAST {\n";
        if (func_type) func_type->Dump(os, indent+1);
        os << make_indent(indent+1) << "name: " << ident << "\n";
        if (block) block->Dump(os, indent+1);
        os << make_indent(indent) << "}\n";
    }
    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        koopa_tmp_id = 0;
        code.clear();
        symtab.scope_id = 1;
        symtab.var_cnt = 0;
        code.push_back("%entry:");
        block->EmitKoopa(code, symtab);
        std::string koopa;
        koopa += "fun @" + ident + "(): " + func_type->EmitKoopa(code, symtab) + "{\n";
        koopa += EmitKoopaWithDCE(code);
        koopa += "}\n";
        return koopa;
    }
    void SemanticCheck(SymbolTable &symtab) override
    {
        block->SemanticCheck(symtab);
    }
};

class CompUnitAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_def;
    void Dump(std::ostream& os, int indent = 0) const override {
        os << make_indent(indent) << "CompUnitAST {\n";
        if (func_def) func_def->Dump(os, indent+1);
        os << make_indent(indent) << "}\n";
    }
    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        return func_def->EmitKoopa(code, symtab);
    }
    void SemanticCheck(SymbolTable &symtab) override
    {
        func_def->SemanticCheck(symtab);
    }
};