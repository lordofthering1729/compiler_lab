#include <fstream>
#include "koopa.h"
#include <map>
#include <string>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

static const char *regs[] = { "t0", "t1", "t2", "t3", "t4", "t5", "t6" };

struct StackInfo
{
    std::map<koopa_raw_value_t, int> value_offset;
    std::map<koopa_raw_value_t, int> alloc_offset;
    int arg_bytes = 0;
    bool need_save_ra = false;
    int total_bytes = 0;
    std::vector<koopa_raw_value_t> values;

    void Dump(std::ostream &os = std::cout) const
    {
        os << "=== StackInfo Dump ===\n";
        os << "alloc_offset:\n";
        for (const auto &kv : alloc_offset)
        {
            std::string name = kv.first->name ? kv.first->name : "<unnamed>";
            os << "  " << name << " @ offset " << kv.second << "\n";
        }
        os << "value_offset:\n";
        for (const auto &kv : value_offset)
        {
            std::string name = kv.first->name ? kv.first->name : "<unnamed>";
            os << "  " << name << " @ offset " << kv.second << "\n";
        }
        os << "arg_bytes: " << arg_bytes << "\n";
        os << "need_save_ra: " << (need_save_ra ? "true" : "false") << "\n";
        os << "total_bytes: " << total_bytes << "\n";
        os << "values: ";
        for (const auto &v : values)
        {
            std::string name = v->name ? v->name : "<unnamed>";
            os << name << ", ";
        }
        os << "\n=== End StackInfo ===\n";
    }
};

// 全局变量信息结构体
struct GlobalVarInfo {
    std::string name;
    int init_value;
    bool zeroinit;
};

std::map<const koopa_raw_value_t, std::string> reg_map;
std::map<std::string, koopa_raw_value_t> param_to_alloc;
int reg_cnt = 0;
// 收集全局变量
std::vector<GlobalVarInfo> global_vars;

// 解析全局变量
void CollectGlobalVars(const koopa_raw_program_t& program) 
{
    for (size_t i = 0; i < program.values.len; ++i) 
    {
        auto val = reinterpret_cast<koopa_raw_value_t>(program.values.buffer[i]);
        if (val->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) 
        {
            std::string name = val->name ? std::string(val->name) : "";
            int init_value = 0;
            bool zeroinit = false;
            const auto& init = val->kind.data.global_alloc.init;
            if (init->kind.tag == KOOPA_RVT_INTEGER) 
            {
                init_value = init->kind.data.integer.value;
            } 
            else 
            {
                zeroinit = true;
            }
            global_vars.push_back({name, init_value, zeroinit});
        }
    }
}

bool IsGlobalVar(const std::string& var_name)
{
    // var_name 已经去掉前导 @
    for (const auto& gv : global_vars)
    {
        std::string name = gv.name;
        if (!name.empty() && name[0] == '@')
        {
            name = name.substr(1);
        }
        if (name == var_name)
        {
            return true;
        }
    }
    return false;
}

// 输出 .data 段
void EmitGlobalVars(std::ofstream& riscv_out) {
    if (global_vars.empty()) return;
    riscv_out << "  .data\n";
    for (const auto& gv : global_vars) {
        riscv_out << "  .globl " << gv.name.substr(1) << "\n"; // 去掉@
        riscv_out << gv.name.substr(1) << ":\n";
        if (gv.zeroinit) {
            riscv_out << "  .zero 4\n";
        } else {
            riscv_out << "  .word " << gv.init_value << "\n";
        }
    }
}


int Align16(int size)
{
    return (size + 15) / 16 * 16;
}

void AnalyzeCalls(const koopa_raw_function_t &func, int &max_args, bool &has_call)
{
    max_args = 0;
    has_call = false;
    for (size_t i = 0; i < func->bbs.len; ++i)
    {
        auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
        for (size_t j = 0; j < bb->insts.len; ++j)
        {
            auto inst = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);
            if (inst->kind.tag == KOOPA_RVT_CALL)
            {
                has_call = true;
                int argc = inst->kind.data.call.args.len;
                if (argc > max_args)
                {
                    max_args = argc;
                }
            }
        }
    }
}

// sp指向最低地址，参数区从sp开始，局部变量从sp+arg_bytes开始，ra在最高（sp+arg_bytes+total_bytes）
void AnalyzeStack(const koopa_raw_function_t &func, StackInfo &stack_info)
{
    int offset = 0;
    for (size_t i = 0; i < func->bbs.len; ++i)
    {
        auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
        for (size_t j = 0; j < bb->insts.len; ++j)
        {
            auto inst = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);
            if (inst->ty->tag == KOOPA_RTT_UNIT)
            {
                continue;
            }
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

// 局部变量起始偏移
int GetVarBaseOffset(const StackInfo &stack_info)
{
    return stack_info.arg_bytes;
}

// ra保存偏移
int GetRAOffset(const StackInfo &stack_info)
{
    return stack_info.arg_bytes + stack_info.total_bytes;
}

void Debug_koopa_raw_value(const koopa_raw_value_t value)
{
    std::cout << "EmitCall: value address = " << value << std::endl;
    std::cout << "EmitCall: value->kind.tag = " << value->kind.tag << std::endl;
    if (value->ty != nullptr)
    {
        std::cout << "EmitCall: value->ty->tag = " << value->ty->tag << std::endl;
    }
    if (value->kind.tag == KOOPA_RVT_INTEGER)
    {
        std::cout << "EmitCall: integer value = " << value->kind.data.integer.value << std::endl;
    }
    if (value->kind.tag == KOOPA_RVT_CALL)
    {
        auto &call = value->kind.data.call;
        std::cout << "EmitCall: call argc = " << call.args.len << std::endl;
        if (call.callee && call.callee->name)
        {
            std::cout << "EmitCall: call callee = " << call.callee->name << std::endl;
        }
    }
}

void EmitPrologue(std::ofstream &riscv_out, int stack_size, bool need_save_ra, const StackInfo& stack_info)
{
    int aligned = Align16(stack_size);
    if (aligned == 0)
    {
        return;
    }
    riscv_out << "  addi sp, sp, -" << aligned << "\n";
    if (need_save_ra)
    {
        riscv_out << "  sw ra, " << GetRAOffset(stack_info) << "(sp)\n";
    }
}

void EmitEpilogue(std::ofstream &riscv_out, int stack_size, bool need_save_ra, const StackInfo &stack_info)
{
    int aligned = Align16(stack_size);
    if (need_save_ra)
    {
        riscv_out << "  lw ra, " << GetRAOffset(stack_info) << "(sp)\n";
    }
    riscv_out << "  addi sp, sp, " << aligned << "\n";
}

int GetValueOffset(const StackInfo &stack_info, const koopa_raw_value_t value)
{
    auto it = stack_info.value_offset.find(value);
    if (it != stack_info.value_offset.end())
    {
        return GetVarBaseOffset(stack_info) + it->second;
    }
    auto it2 = stack_info.alloc_offset.find(value);
    if (it2 != stack_info.alloc_offset.end())
    {
        return GetVarBaseOffset(stack_info) + it2->second;
    }
    return 0;
}

int GetAllocOffset(const StackInfo &stack_info, const koopa_raw_value_t value, const std::map<std::string, koopa_raw_value_t> &param_to_alloc)
{
    std::string name = value->name ? value->name : "";
    if (!name.empty() && name[0] == '@')
    {
        auto it = param_to_alloc.find(name);
        if (it != param_to_alloc.end())
        {
            return GetValueOffset(stack_info, it->second);
        }
    }
    return GetValueOffset(stack_info, value);
}

void Visit(const koopa_raw_program_t &program, std::ofstream &riscv_out);
void Visit(const koopa_raw_slice_t &slice, std::ofstream &riscv_out, StackInfo &stack_info);
void Visit(const koopa_raw_function_t &func, std::ofstream &riscv_out);
void Visit(const koopa_raw_basic_block_t &bb, std::ofstream &riscv_out, StackInfo &stack_info);
void Visit(const koopa_raw_value_t value, std::ofstream &riscv_out, StackInfo &stack_info);

void EmitBlockLabel(const koopa_raw_basic_block_t &bb, std::ofstream &riscv_out)
{
    if (bb->name)
    {
        std::string label(bb->name);
        if (!label.empty() && label[0] == '%')
        {
            label = label.substr(1);
        }
        if (label == "entry")
        {
            return;
        }
        riscv_out << label << ":\n";
    }
}

void Visit(const koopa_raw_program_t &program, std::ofstream &riscv_out)
{
    CollectGlobalVars(program);
    EmitGlobalVars(riscv_out);
    for (size_t i = 0; i < program.funcs.len; ++i)
    {
        auto func = reinterpret_cast<koopa_raw_function_t>(program.funcs.buffer[i]);
        if (func->bbs.len == 0)
        {
            continue;
        }
        Visit(func, riscv_out);
    }
}

void Visit(const koopa_raw_slice_t &slice, std::ofstream &riscv_out, StackInfo &stack_info)
{
    for (size_t i = 0; i < slice.len; ++i)
    {
        auto ptr = slice.buffer[i];
        switch (slice.kind)
        {
            case KOOPA_RSIK_FUNCTION:
            {
                Visit(reinterpret_cast<koopa_raw_function_t>(ptr), riscv_out);
                break;
            }
            case KOOPA_RSIK_BASIC_BLOCK:
            {
                Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr), riscv_out, stack_info);
                break;
            }
            case KOOPA_RSIK_VALUE:
            {
                Visit(reinterpret_cast<koopa_raw_value_t>(ptr), riscv_out, stack_info);
                break;
            }
            default:
            {
                assert(false);
            }
        }
    }
}

void Visit(const koopa_raw_function_t &func, std::ofstream &riscv_out)
{
    reg_map.clear();
    param_to_alloc.clear();
    reg_cnt = 0;
    int max_args = 0;
    bool need_save_ra = false;
    AnalyzeCalls(func, max_args, need_save_ra);

    StackInfo stack_info;
    stack_info.arg_bytes = (std::max(0, max_args - 8)) * 4;
    stack_info.need_save_ra = need_save_ra;
    AnalyzeStack(func, stack_info);

    for (const auto &kv : stack_info.alloc_offset)
    {
        std::string alloc_name = kv.first->name ? kv.first->name : "";
        if (alloc_name.size() > 1 && alloc_name[0] == '%')
        {
            std::string param_name = "@" + alloc_name.substr(1);
            param_to_alloc[param_name] = kv.first;
        }
    }

    int stack_frame_bytes = stack_info.arg_bytes + stack_info.total_bytes + (need_save_ra ? 4 : 0);
    int aligned = Align16(stack_frame_bytes);

    riscv_out << "  .text\n";
    std::string func_name = func->name;
    if (!func_name.empty() && func_name[0] == '@')
    {
        func_name = func_name.substr(1);
    }
    riscv_out << "  .globl " << func_name << "\n";
    riscv_out << func_name << ":\n";
    // 这里传递dummy stack_info用于EmitPrologue中的GetRAOffset
    StackInfo dummy;
    dummy.arg_bytes = stack_info.arg_bytes;
    dummy.total_bytes = stack_info.total_bytes;
    EmitPrologue(riscv_out, stack_frame_bytes, need_save_ra, stack_info);

    for (size_t i = 0; i < func->params.len; ++i)
    {
        auto param = reinterpret_cast<koopa_raw_value_t>(func->params.buffer[i]);
        std::string param_name = param->name ? std::string(param->name) : ("param" + std::to_string(i));
        std::string alloc_name = "%" + param_name.substr(1);
        int var_offset = 0;
        for (auto &kv : stack_info.alloc_offset)
        {
            auto alloc_val = kv.first;
            if (alloc_val->name && std::string(alloc_val->name) == alloc_name)
            {
                var_offset = GetValueOffset(stack_info, alloc_val);
                break;
            }
        }
        if (i < 8)
        {
            riscv_out << "  sw a" << i << ", " << var_offset << "(sp)\n";
        }
        else
        {
            int arg_offset = (i - 8) * 4;
            riscv_out << "  lw t0, " << aligned + arg_offset << "(sp)\n";
            riscv_out << "  sw t0, " << var_offset << "(sp)\n";
        }
    }
    for (size_t i = 0; i < func->bbs.len; ++i)
    {
        auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
        EmitBlockLabel(bb, riscv_out);
        Visit(bb->insts, riscv_out, stack_info);
    }
}

void EmitCall(const koopa_raw_value_t value, std::ofstream &riscv_out, StackInfo &stack_info)
{
    auto &call = value->kind.data.call;
    int argc = call.args.len;
    int stack_frame_bytes = stack_info.arg_bytes + stack_info.total_bytes + (stack_info.need_save_ra ? 4 : 0);
    //int aligned = Align16(stack_frame_bytes);

    //stack_info.Dump();

    for (int i = 0; i < argc; ++i)
    {
        auto arg = reinterpret_cast<koopa_raw_value_t>(call.args.buffer[i]);
        std::string reg;
        if (arg->kind.tag == KOOPA_RVT_INTEGER)
        {
            reg = regs[(reg_cnt++) % 7];
            riscv_out << "  li " << reg << ", " << arg->kind.data.integer.value << "\n";
        }
        else
        {
            int arg_offset = GetAllocOffset(stack_info, arg, param_to_alloc);
            reg = regs[(reg_cnt++) % 7];
            riscv_out << "  lw " << reg << ", " << arg_offset << "(sp)\n";
        }
        if (i < 8)
        {
            riscv_out << "  mv a" << i << ", " << reg << "\n";
        }
        else
        {
            int param_offset = (i - 8) * 4;
            riscv_out << "  sw " << reg << ", " << param_offset << "(sp)\n";
        }
    }
    std::string callee = call.callee->name ? std::string(call.callee->name) : "";
    if (!callee.empty() && callee[0] == '@')
    {
        callee = callee.substr(1);
    }
    riscv_out << "  call " << callee << "\n";
    if (value->ty->tag == KOOPA_RTT_INT32)
    {
        int dst_offset = GetAllocOffset(stack_info, value, param_to_alloc);
        riscv_out << "  sw a0, " << dst_offset << "(sp)\n";
    }
}

void Visit(const koopa_raw_basic_block_t &bb, std::ofstream &riscv_out, StackInfo &stack_info)
{
    EmitBlockLabel(bb, riscv_out);
    Visit(bb->insts, riscv_out, stack_info);
}

void Visit(const koopa_raw_value_t value, std::ofstream &riscv_out, StackInfo &stack_info)
{
    const auto &kind = value->kind;
    switch (kind.tag)
    {
        case KOOPA_RVT_ALLOC:
        {
            break;
        }
        case KOOPA_RVT_INTEGER:
        {
            break;
        }
        case KOOPA_RVT_LOAD:
        {
            auto src = kind.data.load.src;
            std::string src_name = src->name ? std::string(src->name) : "";
            if (!src_name.empty() && src_name[0] == '@')
            {
                src_name = src_name.substr(1);
            }
            if (IsGlobalVar(src_name))
            {
                std::string rd = regs[(reg_cnt++) % 7];
                riscv_out << "  la " << rd << ", " << src_name << "\n";
                riscv_out << "  lw " << rd << ", 0(" << rd << ")\n";
                int dst_offset = GetAllocOffset(stack_info, value, param_to_alloc);
                riscv_out << "  sw " << rd << ", " << dst_offset << "(sp)\n";
            }
            else
            {
                int src_offset = GetAllocOffset(stack_info, src, param_to_alloc);
                std::string rd = regs[(reg_cnt++) % 7];
                reg_map[value] = rd;
                riscv_out << "  lw " << rd << ", " << src_offset << "(sp)\n";
                int dst_offset = GetAllocOffset(stack_info, value, param_to_alloc);
                riscv_out << "  sw " << rd << ", " << dst_offset << "(sp)\n";
            }
            break;
        }
        case KOOPA_RVT_STORE:
        {
            auto dest = kind.data.store.dest;
            auto src = kind.data.store.value;
            std::string dest_name = dest->name ? std::string(dest->name) : "";
            if (!dest_name.empty() && dest_name[0] == '@')
            {
                dest_name = dest_name.substr(1);
            }
            if (IsGlobalVar(dest_name))
            {
                std::string rs = regs[(reg_cnt++) % 7];
                if (src->kind.tag == KOOPA_RVT_INTEGER)
                {
                    riscv_out << "  li " << rs << ", " << src->kind.data.integer.value << "\n";
                }
                else
                {
                    int src_offset = GetAllocOffset(stack_info, src, param_to_alloc);
                    riscv_out << "  lw " << rs << ", " << src_offset << "(sp)\n";
                }
                std::string rd = regs[(reg_cnt++) % 7];
                riscv_out << "  la " << rd << ", " << dest_name << "\n";
                riscv_out << "  sw " << rs << ", 0(" << rd << ")\n";
            }
            else
            {
                int src_offset = GetAllocOffset(stack_info, src, param_to_alloc);
                int dest_offset = GetAllocOffset(stack_info, dest, param_to_alloc);
                std::string rs = "";
                if (src->kind.tag == KOOPA_RVT_INTEGER)
                {
                    rs = regs[(reg_cnt++) % 7];
                    riscv_out << "  li " << rs << ", " << src->kind.data.integer.value << "\n";
                    riscv_out << "  sw " << rs << ", " << dest_offset << "(sp)\n";
                }
                else
                {
                    rs = regs[(reg_cnt++) % 7];
                    if (src_offset != dest_offset)
                    {
                        riscv_out << "  lw " << rs << ", " << src_offset << "(sp)\n";
                        riscv_out << "  sw " << rs << ", " << dest_offset << "(sp)\n";
                    }
                }
            }
            break;
        }
        case KOOPA_RVT_BINARY:
        {
            auto &bin = kind.data.binary;
            std::string lhs, rhs;
            if (bin.lhs->kind.tag == KOOPA_RVT_INTEGER)
            {
                lhs = regs[(reg_cnt++) % 7];
                riscv_out << "  li " << lhs << ", " << bin.lhs->kind.data.integer.value << "\n";
            }
            else
            {
                int lhs_offset = GetAllocOffset(stack_info, bin.lhs, param_to_alloc);
                lhs = regs[(reg_cnt++) % 7];
                riscv_out << "  lw " << lhs << ", " << lhs_offset << "(sp)\n";
            }
            if (bin.rhs->kind.tag == KOOPA_RVT_INTEGER)
            {
                rhs = regs[(reg_cnt++) % 7];
                riscv_out << "  li " << rhs << ", " << bin.rhs->kind.data.integer.value << "\n";
            }
            else
            {
                int rhs_offset = GetAllocOffset(stack_info, bin.rhs, param_to_alloc);
                rhs = regs[(reg_cnt++) % 7];
                riscv_out << "  lw " << rhs << ", " << rhs_offset << "(sp)\n";
            }
            std::string rd = regs[(reg_cnt++) % 7];
            reg_map[value] = rd;
            switch (bin.op)
            {
                case KOOPA_RBO_ADD:
                {
                    riscv_out << "  add " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_SUB:
                {
                    riscv_out << "  sub " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_MUL:
                {
                    riscv_out << "  mul " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_DIV:
                {
                    riscv_out << "  div " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_MOD:
                {
                    riscv_out << "  rem " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
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
                {
                    riscv_out << "  slt " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_GT:
                {
                    riscv_out << "  sgt " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_GE:
                {
                    riscv_out << "  slt " << rd << ", " << lhs << ", " << rhs << "\n";
                    riscv_out << "  xori " << rd << ", " << rd << ", 1\n";
                    break;
                }
                case KOOPA_RBO_LE:
                {
                    riscv_out << "  sgt " << rd << ", " << lhs << ", " << rhs << "\n";
                    riscv_out << "  xori " << rd << ", " << rd << ", 1\n";
                    break;
                }
                case KOOPA_RBO_AND:
                {
                    riscv_out << "  and " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_OR:
                {
                    riscv_out << "  or " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_XOR:
                {
                    riscv_out << "  xor " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_SHL:
                {
                    riscv_out << "  sll " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_SHR:
                {
                    riscv_out << "  srl " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                case KOOPA_RBO_SAR:
                {
                    riscv_out << "  sra " << rd << ", " << lhs << ", " << rhs << "\n";
                    break;
                }
                default:
                {
                    break;
                }
            }
            int dst_offset = GetAllocOffset(stack_info, value, param_to_alloc);
            riscv_out << "  sw " << rd << ", " << dst_offset << "(sp)\n";
            break;
        }
        case KOOPA_RVT_CALL:
        {
            EmitCall(value, riscv_out, stack_info);
            break;
        }
        case KOOPA_RVT_BRANCH:
        {
            auto &br = kind.data.branch;
            auto cond = br.cond;
            std::string cond_reg;
            if (cond->kind.tag == KOOPA_RVT_INTEGER)
            {
                cond_reg = regs[(reg_cnt++) % 7];
                riscv_out << "  li " << cond_reg << ", " << cond->kind.data.integer.value << "\n";
            }
            else
            {
                int cond_offset = GetAllocOffset(stack_info, cond, param_to_alloc);
                cond_reg = regs[(reg_cnt++) % 7];
                riscv_out << "  lw " << cond_reg << ", " << cond_offset << "(sp)\n";
            }
            std::string label_true = br.true_bb->name ? std::string(br.true_bb->name) : "";
            if (!label_true.empty() && label_true[0] == '%')
            {
                label_true = label_true.substr(1);
            }
            std::string label_false = br.false_bb->name ? std::string(br.false_bb->name) : "";
            if (!label_false.empty() && label_false[0] == '%')
            {
                label_false = label_false.substr(1);
            }
            riscv_out << "  bnez " << cond_reg << ", " << label_true << "\n";
            riscv_out << "  j " << label_false << "\n";
            break;
        }
        case KOOPA_RVT_JUMP:
        {
            auto &jump = kind.data.jump;
            std::string label = jump.target->name ? std::string(jump.target->name) : "";
            if (!label.empty() && label[0] == '%')
            {
                label = label.substr(1);
            }
            riscv_out << "  j " << label << "\n";
            break;
        }
        case KOOPA_RVT_RETURN:
        {
            auto &ret = kind.data.ret;
            if (ret.value != nullptr)
            {
                if (ret.value->kind.tag == KOOPA_RVT_INTEGER)
                {
                    riscv_out << "  li a0, " << ret.value->kind.data.integer.value << "\n";
                }
                else
                {
                    int src_offset = GetAllocOffset(stack_info, ret.value, param_to_alloc);
                    riscv_out << "  lw a0, " << src_offset << "(sp)\n";
                }
            }
            EmitEpilogue(riscv_out, stack_info.arg_bytes + stack_info.total_bytes + (stack_info.need_save_ra ? 4 : 0), stack_info.need_save_ra, stack_info);
            riscv_out << "  ret\n\n";
            break;
        }
        default:
        {
            break;
        }
    }
}

void deal_koopa(const char *str, const char *fn)
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