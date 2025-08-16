#include <fstream>
#include "koopa.h"
#include <map>
#include <string>
#include <cassert>

// 寄存器列表
static const char* regs[] = {"t0", "t1", "t2", "t3", "t4", "t5", "t6"};

void Visit(const koopa_raw_program_t &program, std::ofstream &riscv_out);
void Visit(const koopa_raw_slice_t &slice, std::ofstream &riscv_out,
           std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt);
void Visit(const koopa_raw_function_t &func, std::ofstream &riscv_out);
void Visit(const koopa_raw_basic_block_t &bb, std::ofstream &riscv_out,
          std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt);
void Visit(const koopa_raw_value_t value, std::ofstream &riscv_out,
            std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt);

void Visit(const koopa_raw_program_t &program, std::ofstream &riscv_out) {
  std::map<const koopa_raw_value_t, std::string> reg_map;
  int reg_cnt = 0;
  Visit(program.funcs, riscv_out, reg_map, reg_cnt);
}

void Visit(const koopa_raw_slice_t &slice, std::ofstream &riscv_out,
           std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt) {
  for (size_t i = 0; i < slice.len; ++i) {
    auto ptr = slice.buffer[i];
    switch (slice.kind) {
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

void Visit(const koopa_raw_function_t &func, std::ofstream &riscv_out) {
  std::map<const koopa_raw_value_t, std::string> reg_map;
  int reg_cnt = 0;
  riscv_out << "  .text\n";
  std::string func_name = func->name;
  if (!func_name.empty() && func_name[0] == '@') {
    func_name = func_name.substr(1);
  }
  riscv_out << "  .globl " << func_name << "\n";
  riscv_out << func_name << ":\n";
  Visit(func->bbs, riscv_out, reg_map, reg_cnt);
}

void Visit(const koopa_raw_basic_block_t &bb, std::ofstream &riscv_out,
           std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt) {
  Visit(bb->insts, riscv_out, reg_map, reg_cnt);
}

void Visit(const koopa_raw_value_t value, std::ofstream &riscv_out,
           std::map<const koopa_raw_value_t, std::string> &reg_map, int &reg_cnt) {
  const auto &kind = value->kind;
  switch (kind.tag) {
    case KOOPA_RVT_INTEGER: {
      break;
    }
    case KOOPA_RVT_BINARY: {
      std::string rd = regs[reg_cnt++];
      reg_map[value] = rd;
      auto &bin = kind.data.binary;
      std::string lhs, rhs;
      if (bin.lhs->kind.tag == KOOPA_RVT_INTEGER)
        lhs = std::to_string(bin.lhs->kind.data.integer.value);
      else
        lhs = reg_map[bin.lhs];
      if (bin.rhs->kind.tag == KOOPA_RVT_INTEGER)
        rhs = std::to_string(bin.rhs->kind.data.integer.value);
      else
        rhs = reg_map[bin.rhs];
      switch (bin.op) {
        case KOOPA_RBO_EQ:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  xor   " << rd << ", " << rd << ", " << rhs << "\n";
          riscv_out << "  seqz  " << rd << ", " << rd << "\n";
          break;
        case KOOPA_RBO_GT:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  sgt   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_LT:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  slt   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_GE:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  sge   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_LE:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  sle   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_ADD:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  add   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_SUB:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  sub   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_MUL:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  mul   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_DIV:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  div   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_MOD:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  rem   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_AND:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  and   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_OR:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  or    " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_XOR:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  xor   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_SHL:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  sll   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_SHR:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  srl   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        case KOOPA_RBO_SAR:
          riscv_out << "  li    " << rd << ", " << lhs << "\n";
          riscv_out << "  sra   " << rd << ", " << rd << ", " << rhs << "\n";
          break;
        default:
          break;
      }
      break;
    }
    case KOOPA_RVT_RETURN: {
      auto &ret = kind.data.ret;
      std::string src;
      if (ret.value->kind.tag == KOOPA_RVT_INTEGER)
        src = std::to_string(ret.value->kind.data.integer.value);
      else
        src = reg_map[ret.value];
      riscv_out << "  li    a0, " << src << "\n";
      riscv_out << "  ret\n";
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