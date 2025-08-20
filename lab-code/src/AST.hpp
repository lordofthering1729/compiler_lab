#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <vector>
#include <map>
#include <cassert>
#include <stdexcept>

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

    SymbolTable(SymbolTable *parent = nullptr)
    {
        this->parent = parent;
    }

    bool add(const std::string &name, SymbolType type, int value, const std::string &koopa_name = "")
    {
        if (table.count(name))
        {
            return false;
        }
        table[name] = {type, value, koopa_name};
        return true;
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
    virtual void Dump() const = 0;
    virtual std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const = 0;
    virtual int ConstEval(SymbolTable &symtab) const { throw std::runtime_error("Not a const expr"); }
    virtual void SemanticCheck(SymbolTable &symtab) = 0;
};

inline int koopa_tmp_id = 0;

class NumberAST : public BaseAST
{
public:
    int value;

    void Dump() const override
    {
        std::cout << value;
    }

    std::string EmitKoopa(std::vector<std::string> &, SymbolTable &) const override
    {
        return std::to_string(value);
    }

    int ConstEval(SymbolTable &) const override
    {
        return value;
    }

    void SemanticCheck(SymbolTable &) override
    {
    }
};

class PrimaryExpAST : public BaseAST
{
public:
    bool is_number = false;
    std::unique_ptr<BaseAST> exp;
    int number_value = 0;

    void Dump() const override
    {
        if (is_number)
        {
            std::cout << number_value;
        }
        else
        {
            std::cout << "(";
            exp->Dump();
            std::cout << ")";
        }
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        if (is_number)
        {
            return std::to_string(number_value);
        }
        else
        {
            return exp->EmitKoopa(code, symtab);
        }
    }

    int ConstEval(SymbolTable &symtab) const override
    {
        if (is_number)
            return number_value;
        return exp->ConstEval(symtab);
    }

    void SemanticCheck(SymbolTable &) override
    {
    }
};

class IdentAST : public BaseAST
{
public:
    std::string name;

    void Dump() const override
    {
        std::cout << name;
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        auto *info = symtab.lookup(name);
        if (!info)
        {
            throw std::runtime_error("未定义标识符: " + name + " (EmitKoopa)");
        }
        if (info->type == CONSTANT)
        {
            return std::to_string(info->value);
        }
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
        {
            throw std::runtime_error("ConstEval要求常量: " + name);
        }
        return info->value;
    }

    void SemanticCheck(SymbolTable &symtab) override
    {
        auto *info = symtab.lookup(name);
        if (!info)
        {
            throw std::runtime_error("未定义标识符: " + name + " (SemanticCheck)");
        }
    }
};

class UnaryExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> exp;

    void Dump() const override
    {
        std::cout << op << " ";
        exp->Dump();
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        std::string val = exp->EmitKoopa(code, symtab);
        if (op == "+")
        {
            return val;
        }
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

    void SemanticCheck(SymbolTable &) override
    {
    }
};

class BinaryExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> lhs;
    std::unique_ptr<BaseAST> rhs;

    void Dump() const override
    {
        lhs->Dump();
        std::cout << " " << op << " ";
        rhs->Dump();
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        std::string l = lhs->EmitKoopa(code, symtab);
        std::string r = rhs->EmitKoopa(code, symtab);

        if (op == "&&")
        {
            std::string l_cmp = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(l_cmp + " = ne " + l + ", 0");
            std::string r_cmp = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(r_cmp + " = ne " + r + ", 0");
            std::string res = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(res + " = and " + l_cmp + ", " + r_cmp);
            return res;
        }
        else if (op == "||")
        {
            std::string l_cmp = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(l_cmp + " = ne " + l + ", 0");
            std::string r_cmp = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(r_cmp + " = ne " + r + ", 0");
            std::string res = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(res + " = or " + l_cmp + ", " + r_cmp);
            return res;
        }

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

    void Dump() const override
    {
        lor_exp->Dump();
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        return lor_exp->EmitKoopa(code, symtab);
    }

    int ConstEval(SymbolTable &symtab) const override
    {
        return lor_exp->ConstEval(symtab);
    }

    void SemanticCheck(SymbolTable &) override
    {
    }
};

class StmtAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> stmt;

    void Dump() const override
    {
        std::cout << "return ";
        stmt->Dump();
        std::cout << ";" << std::endl;
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        std::string val = stmt->EmitKoopa(code, symtab);
        code.push_back("ret " + val);
        return "";
    }

    void SemanticCheck(SymbolTable &) override
    {
    }
};

class ConstDeclAST : public BaseAST
{
public:
    struct Def
    {
        std::string name;
        std::unique_ptr<BaseAST> val;
    };
    std::vector<Def> defs;

    void Dump() const override
    {
        std::cout << "const int ";
        for (size_t i = 0; i < defs.size(); ++i)
        {
            std::cout << defs[i].name << " = ";
            defs[i].val->Dump();
            if (i + 1 < defs.size())
            {
                std::cout << ", ";
            }
        }
        std::cout << ";" << std::endl;
    }

    // 关键：EmitKoopa时也插入符号表
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
            {
                throw std::runtime_error("重复定义: " + def.name);
            }
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

    void Dump() const override
    {
        std::cout << "int ";
        for (size_t i = 0; i < defs.size(); ++i)
        {
            std::cout << defs[i].name;
            if (defs[i].has_init)
            {
                std::cout << " = ";
                defs[i].val->Dump();
            }
            if (i + 1 < defs.size())
            {
                std::cout << ", ";
            }
        }
        std::cout << ";" << std::endl;
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        for (const auto &def : defs)
        {
            std::string alloc_name = "%" + std::to_string(koopa_tmp_id++);
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
            {
                def.val->SemanticCheck(symtab);
            }
            if (!symtab.add(def.name, VAR, 0))
            {
                throw std::runtime_error("重复定义: " + def.name);
            }
        }
    }
};

class AssignAST : public BaseAST
{
public:
    std::string lval;
    std::unique_ptr<BaseAST> exp;

    void Dump() const override
    {
        std::cout << lval << " = ";
        exp->Dump();
        std::cout << ";" << std::endl;
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        auto *info = symtab.lookup(lval);
        if (!info || info->type != VAR)
        {
            throw std::runtime_error("赋值语句左值必须为变量: " + lval);
        }
        std::string v = exp->EmitKoopa(code, symtab);
        code.push_back("store " + v + ", " + info->koopa_name);
        return "";
    }

    void SemanticCheck(SymbolTable &symtab) override
    {
        auto *info = symtab.lookup(lval);
        if (!info)
        {
            throw std::runtime_error("变量未定义: " + lval);
        }
        if (info->type != VAR)
        {
            throw std::runtime_error("不能给常量赋值: " + lval);
        }
        exp->SemanticCheck(symtab);
    }
};

class ReturnAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> exp;

    void Dump() const override
    {
        std::cout << "return ";
        exp->Dump();
        std::cout << ";" << std::endl;
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        try
        {
            int v = exp->ConstEval(symtab);
            code.push_back("ret " + std::to_string(v));
            return "";
        }
        catch (...)
        {
            std::string v = exp->EmitKoopa(code, symtab);
            code.push_back("ret " + v);
            return "";
        }
    }

    void SemanticCheck(SymbolTable &symtab) override
    {
        exp->SemanticCheck(symtab);
    }
};

class BlockAST : public BaseAST
{
public:
    std::vector<std::unique_ptr<BaseAST>> items;

    void Dump() const override
    {
        std::cout << "{" << std::endl;
        for (auto &item : items)
        {
            item->Dump();
        }
        std::cout << "}" << std::endl;
    }

    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &parent_tab) const override
    {
        code.push_back("%entry:");
        SymbolTable local_tab(&parent_tab);
        for (auto &item : items)
        {
            item->EmitKoopa(code, local_tab);
        }
        return "";
    }

    void SemanticCheck(SymbolTable &parent_tab) override
    {
        SymbolTable local_tab(&parent_tab);
        for (auto &item : items)
        {
            item->SemanticCheck(local_tab);
        }
    }
};

class FuncTypeAST : public BaseAST
{
public:
    std::string type;
    void Dump() const override
    {
        std::cout << type << " ";
    }
    std::string EmitKoopa(std::vector<std::string> &, SymbolTable &) const override
    {
        return "i32 ";
    }
    void SemanticCheck(SymbolTable &) override
    {
    }
};

class FuncDefAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;
    void Dump() const override
    {
        func_type->Dump();
        std::cout << ident << "() ";
        block->Dump();
    }
    std::string EmitKoopa(std::vector<std::string> &code, SymbolTable &symtab) const override
    {
        koopa_tmp_id = 0;
        code.clear();
        std::string koopa;
        koopa += "fun @" + ident + "(): " + func_type->EmitKoopa(code, symtab) + "{\n";
        block->EmitKoopa(code, symtab);
        for (auto &line : code)
        {
            koopa += "  " + line + "\n";
        }
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
    void Dump() const override
    {
        func_def->Dump();
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