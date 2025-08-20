#include <fstream>
#include "koopa.h"
#include <map>
#include <string>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

// RISC-V 临时寄存器列表
static const char* regs[] = 
{
    "t0", "t1", "t2", "t3", "t4", "t5", "t6"
};

struct StackInfo
{
    // 指令返回值在栈上的偏移量
    std::map<koopa_raw_value_t, int> value_offset;
    // alloc 指令的返回值在栈上的偏移量
    std::map<koopa_raw_value_t, int> alloc_offset;
    // 当前分配到的总字节数
    int total_bytes = 0;
    // 保存所有 alloc 指令和需要保存的指令
    std::vector<koopa_raw_value_t> values;
};

int Align16(int size)
{
    return (size + 15) / 16 * 16;
}

// 扫描所有指令，分配偏移，返回对齐后的总字节数
void AnalyzeStack(const koopa_raw_function_t &func, StackInfo &stack_info)
{
    int offset = 0;
    // 对所有 basic block 的指令分配空间
    for (size_t i = 0; i < func->bbs.len; ++i)
    {
        auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
        for (size_t j = 0; j < bb->insts.len; ++j)
        {
            auto inst = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);
            // unit 类型不分配空间
            if (inst->ty->tag == KOOPA_RTT_UNIT)
                continue;

            // alloc 另做记录
            if (inst->kind.tag == KOOPA_RVT_ALLOC)
            {
                stack_info.alloc_offset[inst] = offset;
            }
            else
            {
                stack_info.value_offset[inst] = offset;
            }
            stack_info.values.push_back(inst);
            offset += 4;
        }
    }
    stack_info.total_bytes = offset;
}

void EmitPrologue(std::ofstream &riscv_out, int stack_size)
{
    int aligned = Align16(stack_size);
    if (aligned == 0)
        return;
    // 范围在[-2048,2047]直接用 addi
    if (aligned >= -2048 && aligned <= 2047)
    {
        riscv_out << "  addi sp, sp, -" << aligned << "\n";
    }
    else
    {
        riscv_out << "  li t0, -" << aligned << "\n";
        riscv_out << "  add sp, sp, t0\n";
    }
}

void EmitEpilogue(std::ofstream &riscv_out, int stack_size)
{
    int aligned = Align16(stack_size);
    if (aligned == 0)
        return;
    if (aligned >= -2048 && aligned <= 2047)
    {
        riscv_out << "  addi sp, sp, " << aligned << "\n";
    }
    else
    {
        riscv_out << "  li t0, " << aligned << "\n";
        riscv_out << "  add sp, sp, t0\n";
    }
}

// 获取一个指令的栈偏移
int GetValueOffset(const StackInfo &stack_info, const koopa_raw_value_t value)
{
    auto it = stack_info.value_offset.find(value);
    if (it != stack_info.value_offset.end())
        return it->second;
    auto it2 = stack_info.alloc_offset.find(value);
    if (it2 != stack_info.alloc_offset.end())
        return it2->second;
    // 没找到，可能是参数（本题不涉及），或者直接返回0
    return 0;
}

// 记录每条指令已分配的寄存器名
std::map<const koopa_raw_value_t, std::string> reg_map;
int reg_cnt = 0;

void Visit(const koopa_raw_program_t &program, std::ofstream &riscv_out);
void Visit(const koopa_raw_slice_t &slice, std::ofstream &riscv_out,
           StackInfo &stack_info);
void Visit(const koopa_raw_function_t &func, std::ofstream &riscv_out);
void Visit(const koopa_raw_basic_block_t &bb, std::ofstream &riscv_out,
           StackInfo &stack_info);
void Visit(const koopa_raw_value_t value, std::ofstream &riscv_out,
           StackInfo &stack_info);

void Visit(const koopa_raw_program_t &program, std::ofstream &riscv_out)
{
    for (size_t i = 0; i < program.funcs.len; ++i)
    {
        auto func = reinterpret_cast<koopa_raw_function_t>(program.funcs.buffer[i]);
        Visit(func, riscv_out);
    }
}

void Visit(const koopa_raw_slice_t &slice, std::ofstream &riscv_out,StackInfo &stack_info)
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
                Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr), riscv_out, stack_info);
                break;
            case KOOPA_RSIK_VALUE:
                Visit(reinterpret_cast<koopa_raw_value_t>(ptr), riscv_out, stack_info);
                break;
            default:
                assert(false);
        }
    }
}

void Visit(const koopa_raw_function_t &func, std::ofstream &riscv_out)
{
    reg_map.clear();
    reg_cnt = 0;
    StackInfo stack_info;
    AnalyzeStack(func, stack_info);

    riscv_out << "  .text\n";
    std::string func_name = func->name;
    if (!func_name.empty() && func_name[0] == '@')
    {
        func_name = func_name.substr(1);
    }
    riscv_out << "  .globl " << func_name << "\n";
    riscv_out << func_name << ":\n";
    EmitPrologue(riscv_out, stack_info.total_bytes);
    Visit(func->bbs, riscv_out, stack_info);
    EmitEpilogue(riscv_out, stack_info.total_bytes);
}

void Visit(const koopa_raw_basic_block_t &bb, std::ofstream &riscv_out,
           StackInfo &stack_info)
{
    Visit(bb->insts, riscv_out, stack_info);
}

void Visit(const koopa_raw_value_t value, std::ofstream &riscv_out,StackInfo &stack_info)
{
    const auto &kind = value->kind;
    switch (kind.tag)
    {
        case KOOPA_RVT_ALLOC:
        {
            // alloc 指令分配空间，无需输出代码
            // 变量的初值 store 指令会处理
            break;
        }
        case KOOPA_RVT_INTEGER:
        {
            // 立即数只在被用到时由其它指令处理
            break;
        }
        case KOOPA_RVT_LOAD:
        {
            // load 指令，寄存器保存结果
            auto src = kind.data.load.src;
            int src_offset = GetValueOffset(stack_info, src);
            std::string rd = regs[(reg_cnt++) % 7];
            reg_map[value] = rd;
            riscv_out << "  lw " << rd << ", " << src_offset << "(sp)\n";
            // 把返回值存入栈帧
            int dst_offset = GetValueOffset(stack_info, value);
            riscv_out << "  sw " << rd << ", " << dst_offset << "(sp)\n";
            break;
        }
        case KOOPA_RVT_STORE:
        {
            // store 指令没有返回值
            auto src = kind.data.store.value;
            auto dest = kind.data.store.dest;
            std::string rs = "";
            if (src->kind.tag == KOOPA_RVT_INTEGER)
            {
                rs = regs[(reg_cnt++) % 7];
                riscv_out << "  li " << rs << ", " << src->kind.data.integer.value << "\n";
            }
            else
            {
                int src_offset = GetValueOffset(stack_info, src);
                rs = regs[(reg_cnt++) % 7];
                riscv_out << "  lw " << rs << ", " << src_offset << "(sp)\n";
            }
            int dest_offset = GetValueOffset(stack_info, dest);
            riscv_out << "  sw " << rs << ", " << dest_offset << "(sp)\n";
            break;
        }
        case KOOPA_RVT_BINARY:
        {
            auto &bin = kind.data.binary;
            std::string lhs, rhs;
            // 左操作数
            if (bin.lhs->kind.tag == KOOPA_RVT_INTEGER)
            {
                lhs = regs[(reg_cnt++) % 7];
                riscv_out << "  li " << lhs << ", " << bin.lhs->kind.data.integer.value << "\n";
            }
            else
            {
                int lhs_offset = GetValueOffset(stack_info, bin.lhs);
                lhs = regs[(reg_cnt++) % 7];
                riscv_out << "  lw " << lhs << ", " << lhs_offset << "(sp)\n";
            }
            // 右操作数
            if (bin.rhs->kind.tag == KOOPA_RVT_INTEGER)
            {
                rhs = regs[(reg_cnt++) % 7];
                riscv_out << "  li " << rhs << ", " << bin.rhs->kind.data.integer.value << "\n";
            }
            else
            {
                int rhs_offset = GetValueOffset(stack_info, bin.rhs);
                rhs = regs[(reg_cnt++) % 7];
                riscv_out << "  lw " << rhs << ", " << rhs_offset << "(sp)\n";
            }
            std::string rd = regs[(reg_cnt++) % 7];
            reg_map[value] = rd;
            switch (bin.op)
            {
                case KOOPA_RBO_ADD:
                    riscv_out << "  add " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_SUB:
                    riscv_out << "  sub " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_MUL:
                    riscv_out << "  mul " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_DIV:
                    riscv_out << "  div " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_MOD:
                    riscv_out << "  rem " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_EQ:
                {
                    riscv_out << "  xor " << rd << ", " << lhs << ", " << rhs << "\n";
                    riscv_out << "  seqz " << rd << ", " << rd << "\n";
                    break;
                }
                case KOOPA_RBO_NOT_EQ:
                {
                    riscv_out << "  xor " << rd << ", " << lhs << ", " << rhs << "\n";
                    riscv_out << "  snez " << rd << ", " << rd << "\n";
                    break;
                }
                case KOOPA_RBO_LT:
                    riscv_out << "  slt " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_GT:
                    riscv_out << "  sgt " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_GE:
                    riscv_out << "  slt " << rd << ", " << lhs << ", " << rhs << "\n";
                    riscv_out << "  xori " << rd << ", " << rd << ", 1\n";
                    break;
                case KOOPA_RBO_LE:
                    riscv_out << "  sgt " << rd << ", " << lhs << ", " << rhs << "\n";
                    riscv_out << "  xori " << rd << ", " << rd << ", 1\n";
                    break;
                case KOOPA_RBO_AND:
                    riscv_out << "  and " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_OR:
                    riscv_out << "  or " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_XOR:
                    riscv_out << "  xor " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_SHL:
                    riscv_out << "  sll " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_SHR:
                    riscv_out << "  srl " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                case KOOPA_RBO_SAR:
                    riscv_out << "  sra " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                default:
                    break;
            }
            // 把返回值存入栈帧
            int dst_offset = GetValueOffset(stack_info, value);
            riscv_out << "  sw " << rd << ", " << dst_offset << "(sp)\n";
            break;
        }
        case KOOPA_RVT_RETURN:
        {
            auto &ret = kind.data.ret;
            std::string src;
            if (ret.value->kind.tag == KOOPA_RVT_INTEGER)
            {
                riscv_out << "  li a0, " << ret.value->kind.data.integer.value << "\n";
            }
            else
            {
                int src_offset = GetValueOffset(stack_info, ret.value);
                riscv_out << "  lw a0, " << src_offset << "(sp)\n";
            }
            riscv_out << "  ret\n";
            break;
        }
        default:
            break;
    }
}

void deal_koopa(const char* str, const char* fn)
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