#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <vector>

// 所有 AST 的基类
class BaseAST 
{
 public:
  virtual ~BaseAST() = default;
  virtual void Dump() const = 0;
  virtual std::string EmitKoopa(std::vector<std::string>& code) const = 0;
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
    std::string EmitKoopa(std::vector<std::string>& code) const override 
    {
        return std::to_string(value);
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
    std::string EmitKoopa(std::vector<std::string>& code) const override 
    {
        if (is_number) 
        {
            return std::to_string(number_value);
        } 
        else 
        {
            return exp->EmitKoopa(code);
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
    std::string EmitKoopa(std::vector<std::string>& code) const override 
    {
        std::string val = exp->EmitKoopa(code);
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
    std::string EmitKoopa(std::vector<std::string>& code) const override 
    {
        std::string l = lhs->EmitKoopa(code);
        std::string r = rhs->EmitKoopa(code);

        // 逻辑与/或特殊处理：拆为比较+按位与或
        if (op == "&&") 
        {
            std::string l_cmp = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(l_cmp + " = ne " + l + ", 0");
            std::string r_cmp = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(r_cmp + " = ne " + r + ", 0");
            // 按位与
            std::string res = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(res + " = and " + l_cmp + ", " + r_cmp);
            return res;
        } 
        else if (op == "||") 
        {
            // l != 0
            std::string l_cmp = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(l_cmp + " = ne " + l + ", 0");
            // r != 0
            std::string r_cmp = "%" + std::to_string(koopa_tmp_id++);
            code.push_back(r_cmp + " = ne " + r + ", 0");
            // 按位或
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
};

class ExpAST : public BaseAST 
{
public:
    std::unique_ptr<BaseAST> lor_exp;
    void Dump() const override 
    {
        lor_exp->Dump();
    }
    std::string EmitKoopa(std::vector<std::string>& code) const override 
    {
        return lor_exp->EmitKoopa(code);
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
    std::string EmitKoopa(std::vector<std::string>& code) const override 
    {
        std::string val = stmt->EmitKoopa(code);
        code.push_back("ret " + val);
        return "";
    }
};

class BlockAST : public BaseAST 
{
public:
    std::unique_ptr<BaseAST> stmt;
    void Dump() const override 
    {
        std::cout << "{ ";
        stmt->Dump();
        std::cout << " }" << std::endl;
    }
    std::string EmitKoopa(std::vector<std::string>& code) const override 
    {
        code.push_back("%entry:");
        stmt->EmitKoopa(code);
        return "";
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
    std::string EmitKoopa(std::vector<std::string>& code) const override 
    {
        return "i32 ";
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
    std::string EmitKoopa(std::vector<std::string>& code) const override 
    {
        koopa_tmp_id = 0;
        code.clear();
        std::string koopa;
        koopa += "fun @" + ident + "(): " + func_type->EmitKoopa(code) + "{\n";
        block->EmitKoopa(code);
        for (auto& line : code) 
        {
            koopa += "  " + line + "\n";
        }
        koopa += "}\n";
        return koopa;
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
    std::string EmitKoopa(std::vector<std::string>& code) const override 
    {
        return func_def->EmitKoopa(code);
    }
};