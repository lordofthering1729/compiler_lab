#include <fstream>
#include "koopa.h"
#include <map>
#include <string>
#include <algorithm>
#include <cassert>
#include <iostream>

// 寄存器列表
static const char* regs[] = 
{
    "t0", "t1", "t2", "t3", "t4", "t5", "t6"
};

void Visit(const koopa_raw_program_t &program, std::ofstream &riscv_out);
void Visit(const koopa_raw_slice_t &slice, std::ofstream &riscv_out,
           std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt);
void Visit(const koopa_raw_function_t &func, std::ofstream &riscv_out);
void Visit(const koopa_raw_basic_block_t &bb, std::ofstream &riscv_out,
           std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt);
void Visit(const koopa_raw_value_t value, std::ofstream &riscv_out,
           std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt);

void Visit(const koopa_raw_program_t &program, std::ofstream &riscv_out) 
{
    std::map<const koopa_raw_value_t, std::string> reg_map;
    int reg_cnt = 0;
    Visit(program.funcs, riscv_out, reg_map, reg_cnt);
}

inline bool is_number(const std::string& s) 
{
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

void Visit(const koopa_raw_slice_t &slice, std::ofstream &riscv_out,
           std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt) 
{
    for (size_t i = 0; i < slice.len; ++i) 
    {
        auto ptr = slice.buffer[i];
        switch (slice.kind) 
        {
            case KOOPA_RSIK_FUNCTION:
                Visit(reinterpret_cast<koopa_raw_function_t>(ptr), riscv_out);
                break;
            case KOOPA_RSIK_BASIC_BLOCK:
                Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr), riscv_out, reg_map, reg_cnt);
                break;
            case KOOPA_RSIK_VALUE:
                Visit(reinterpret_cast<koopa_raw_value_t>(ptr), riscv_out, reg_map, reg_cnt);
                break;
            default:
                assert(false);
        }
    }
}

void Visit(const koopa_raw_function_t &func, std::ofstream &riscv_out) 
{
    std::map<const koopa_raw_value_t, std::string> reg_map;
    int reg_cnt = 0;
    riscv_out << "  .text\n";
    std::string func_name = func->name;
    if (!func_name.empty() && func_name[0] == '@') 
    {
        func_name = func_name.substr(1);
    }
    riscv_out << "  .globl " << func_name << "\n";
    riscv_out << func_name << ":\n";
    Visit(func->bbs, riscv_out, reg_map, reg_cnt);
}

void Visit(const koopa_raw_basic_block_t &bb, std::ofstream &riscv_out,
           std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt) 
{
    Visit(bb->insts, riscv_out, reg_map, reg_cnt);
}

//对于加减乘除，取模，sle，sge，统一把两个操作数弄到寄存器中再操作（如果原本就是寄存器则无所谓）
//寄存器的获取方式为regs[(reg_cnt++)%7];以避免重复
void Visit(const koopa_raw_value_t value, std::ofstream &riscv_out,
           std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt) 
{
    const auto &kind = value->kind;
    int lhs_is_a_number = 0;
    int rhs_is_a_number = 0;
    switch (kind.tag) 
    {
        case KOOPA_RVT_INTEGER: 
        {
            break;
        }
        case KOOPA_RVT_BINARY: 
        {
            std::string rd = regs[(reg_cnt++)%7];
            reg_map[value] = rd;
            auto &bin = kind.data.binary;
            std::string lhs, rhs;
            if (bin.lhs->kind.tag == KOOPA_RVT_INTEGER)
            {
                lhs = std::to_string(bin.lhs->kind.data.integer.value);
                lhs_is_a_number = 1;
            }
            else
                lhs = reg_map[bin.lhs];
            if (bin.rhs->kind.tag == KOOPA_RVT_INTEGER)
            {
                rhs = std::to_string(bin.rhs->kind.data.integer.value);
                rhs_is_a_number = 1;
            }
            else
                rhs = reg_map[bin.rhs];
            switch (bin.op) 
            {
                case KOOPA_RBO_EQ:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    riscv_out << "  xor   " << rd << ", " << rd << ", " << rhs << "\n";
                    riscv_out << "  seqz  " << rd << ", " << rd << "\n";
                    break;
                case KOOPA_RBO_NOT_EQ:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    riscv_out << "  xor   " << rd << ", " << rd << ", " << rhs << "\n";
                    riscv_out << "  snez  " << rd << ", " << rd << "\n";
                    break;
                case KOOPA_RBO_GT:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    if (is_number(rhs)) 
                    {
                        std::string rs1 = regs[(reg_cnt++)%7];
                        riscv_out << "  li    " << rs1 << ", " << rhs << "\n";
                        riscv_out << "  sgt   " << rd << ", " << rd << ", " << rs1 << "\n";
                    } 
                    else 
                    {
                        riscv_out << "  sgt   " << rd << ", " << rd << ", " << rhs << "\n";
                    }
                    break;
                case KOOPA_RBO_LT:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    if (is_number(rhs)) 
                    {
                        std::string rs1 = regs[(reg_cnt++)%7];
                        riscv_out << "  li    " << rs1 << ", " << rhs << "\n";
                        riscv_out << "  slt   " << rd << ", " << rd << ", " << rs1 << "\n";
                    } 
                    else 
                    {
                        riscv_out << "  slt   " << rd << ", " << rd << ", " << rhs << "\n";
                    }
                    break;
                case KOOPA_RBO_GE:
                    //大于等于就是小于的反面，但是操作数不用反过来
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    if (is_number(rhs)) 
                    {
                        std::string rs1 = regs[(reg_cnt++)%7];
                        riscv_out << "  li    " << rs1 << ", " << rhs << "\n";
                        riscv_out << "  slt   " << rd << ", " << rd << ", " << rs1 << "\n";
                    } 
                    else 
                    {
                        riscv_out << "  slt   " << rd << ", " << rd << ", " << rhs << "\n";
                    }
                    riscv_out << "  xori  " << rd << ", " << rd << ", 1\n";
                    break;
                case KOOPA_RBO_LE:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    if (is_number(rhs)) 
                    {
                        std::string rs1 = regs[(reg_cnt++)%7];
                        riscv_out << "  li    " << rs1 << ", " << rhs << "\n";
                        riscv_out << "  sgt   " << rd << ", " << rd << ", " << rs1 << "\n";
                    } 
                    else 
                    {
                        riscv_out << "  sgt   " << rd << ", " << rd << ", " << rhs << "\n";
                    }
                    riscv_out << "  xori  " << rd << ", " << rd << ", 1\n";
                    break;
                case KOOPA_RBO_ADD:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    if (is_number(rhs)) 
                    {
                        std::string rs1 = regs[(reg_cnt++)%7];
                        riscv_out << "  li    " << rs1 << ", " << rhs << "\n";
                        riscv_out << "  add   " << rd << ", " << rd << ", " << rs1 << "\n";
                    } 
                    else 
                    {
                        riscv_out << "  add   " << rd << ", " << rd << ", " << rhs << "\n";
                    }
                    break;
                case KOOPA_RBO_SUB:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    if (is_number(rhs)) 
                    {
                        std::string rs1 = regs[(reg_cnt++)%7];
                        riscv_out << "  li    " << rs1 << ", " << rhs << "\n";
                        riscv_out << "  sub   " << rd << ", " << rd << ", " << rs1 << "\n";
                    } 
                    else 
                    {
                        riscv_out << "  sub   " << rd << ", " << rd << ", " << rhs << "\n";
                    }
                    break;
                case KOOPA_RBO_MUL:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    if (is_number(rhs)) 
                    {
                        std::string rs1 = regs[(reg_cnt++)%7];
                        riscv_out << "  li    " << rs1 << ", " << rhs << "\n";
                        riscv_out << "  mul   " << rd << ", " << rd << ", " << rs1 << "\n";
                    } 
                    else 
                    {
                        riscv_out << "  mul   " << rd << ", " << rd << ", " << rhs << "\n";
                    }
                    break;
                case KOOPA_RBO_DIV:
                    // 如果左操作数是数字，直接加载到寄存器
                    // 否则从寄存器中移动
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    // 如果右操作数是数字，直接加载到临时寄存器
                    // 否则从寄存器中移动
                    // 注意：除法操作需要处理除数为0的情况，暂时不考虑
                    if (is_number(rhs)) 
                    {
                        std::string rs1 = regs[(reg_cnt++)%7];
                        riscv_out << "  li    " << rs1 << ", " << rhs << "\n";
                        riscv_out << "  div   " << rd << ", " << rd << ", " << rs1 << "\n";
                    }
                    else
                    {
                        riscv_out << "  div   " << rd << ", " << rd << ", " << rhs << "\n";
                    } 
                    break;
                case KOOPA_RBO_MOD:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    if (is_number(rhs)) 
                    {
                        std::string rs1 = regs[(reg_cnt++)%7];
                        riscv_out << "  li    " << rs1 << ", " << rhs << "\n";
                        riscv_out << "  rem   " << rd << ", " << rd << ", " << rs1 << "\n";
                    } 
                    else 
                    {
                        riscv_out << "  rem   " << rd << ", " << rd << ", " << rhs << "\n";
                    }
                    break;
                case KOOPA_RBO_AND:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    riscv_out << "  and   " << rd << ", " << rd << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_OR:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    riscv_out << "  or    " << rd << ", " << rd << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_XOR:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    riscv_out << "  xor   " << rd << ", " << rd << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_SHL:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    riscv_out << "  sll   " << rd << ", " << rd << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_SHR:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    riscv_out << "  srl   " << rd << ", " << rd << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_SAR:
                    if(lhs_is_a_number) 
                    {
                        riscv_out << "  li    " << rd << ", " << lhs << "\n";
                    }
                    else
                    {
                        riscv_out << "  mv    " << rd << ", " << lhs << "\n";
                    }
                    riscv_out << "  sra   " << rd << ", " << rd << ", " << rhs << "\n";
                    break;
                default:
                    break;
            }
            break;
        }
        case KOOPA_RVT_RETURN: 
        {
            auto &ret = kind.data.ret;
            std::string src;
            if (ret.value->kind.tag == KOOPA_RVT_INTEGER)
            {
                src = std::to_string(ret.value->kind.data.integer.value);
                riscv_out << "  li    a0, " << src << "\n";
                riscv_out << "  ret\n";
            }
            else
            {
                src = reg_map[ret.value];
                riscv_out << "  mv    a0, " << src << "\n";
                riscv_out << "  ret\n";
            }
            break;
        }
        default:
            break;
    }
}

void deal_koopa(const char* str,const char* fn) 
{
    koopa_program_t program;
    koopa_error_code_t ret = koopa_parse_from_string(str, &program);
    assert(ret == KOOPA_EC_SUCCESS);

    koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
    koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
    koopa_delete_program(program);

    std::ofstream riscv_output(fn, std::ios::out | std::ios::trunc);
    Visit(raw, riscv_output);
    riscv_output.close();

    koopa_delete_raw_program_builder(builder);
}