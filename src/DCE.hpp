// lab-code/src/DCE.hpp
// This file implements dead code elimination (DCE) for Koopa IR.
#pragma once

#include <vector>
#include <string>
#include <set>
#include <map>
#include <memory>
#include <cassert>
#include "AST.hpp"

// 一个基本块的数据结构
struct DCEBasicBlock
{
    std::string label;
    std::vector<std::string> ir; // Koopa IR指令
    std::vector<std::string> successors; // 后继块label
    bool reachable = false;
};

// 构建控制流图
inline std::vector<DCEBasicBlock> BuildCFG(const std::vector<std::string>& code)
{
    std::vector<DCEBasicBlock> blocks;
    std::map<std::string, int> label2idx;
    DCEBasicBlock* curr = nullptr;
    for (size_t i = 0; i < code.size(); ++i)
    {
        const std::string& line = code[i];
        // 新块起始
        if (!line.empty() && line.back() == ':' && line[0] == '%')
        {
            std::string label = line.substr(0, line.size() - 1);
            blocks.push_back(DCEBasicBlock{label, {}, {}, false});
            label2idx[label] = blocks.size() - 1;
            curr = &blocks.back();
            continue;
        }
        // 指令归入当前块
        if (curr)
        {
            curr->ir.push_back(line);
            // 收集分支指令的后继
            if (line.find("jump ") == 0)
            {
                std::string tgt = line.substr(5);
                tgt.erase(0, tgt.find_first_not_of(" \t"));
                curr->successors.push_back(tgt);
            }
            else if (line.find("br ") == 0)
            {
                // br cond, %true, %false
                size_t pos1 = line.find(',');
                size_t pos2 = line.find(',', pos1 + 1);
                std::string tbb = line.substr(pos1 + 1, pos2 - pos1 - 1);
                std::string fbb = line.substr(pos2 + 1);
                tbb.erase(0, tbb.find_first_not_of(" \t"));
                fbb.erase(0, fbb.find_first_not_of(" \t"));
                curr->successors.push_back(tbb);
                curr->successors.push_back(fbb);
            }
            // ret指令无后继
        }
    }
    return blocks;
}

// 可达性分析，入口为 %entry
inline void MarkReachable(std::vector<DCEBasicBlock>& blocks)
{
    std::map<std::string, DCEBasicBlock*> label2blk;
    for (auto& blk : blocks)
    {
        label2blk[blk.label] = &blk;
    }
    if (label2blk.count("%entry") == 0)
        return;
    std::vector<DCEBasicBlock*> worklist;
    worklist.push_back(label2blk["%entry"]);
    label2blk["%entry"]->reachable = true;
    while (!worklist.empty())
    {
        DCEBasicBlock* bb = worklist.back();
        worklist.pop_back();
        for (auto& succ : bb->successors)
        {
            auto it = label2blk.find(succ);
            if (it != label2blk.end() && !it->second->reachable)
            {
                it->second->reachable = true;
                worklist.push_back(it->second);
            }
        }
    }
}

// 死代码消除主流程
inline std::string EmitKoopaWithDCE(const std::vector<std::string>& code)
{
    auto blocks = BuildCFG(code);
    MarkReachable(blocks);
    std::string result;
    for (const auto& block : blocks)
    {
        if (!block.reachable) continue;
        result += block.label + ":\n";
        for (const auto& inst : block.ir)
        {
            result += "  " + inst + "\n";
            // ret后立即终止本块输出
            if (inst.find("ret") == 0) break;
        }
    }
    return result;
}