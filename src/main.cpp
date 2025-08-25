#include <cassert>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <memory>
#include "AST.hpp"
#include <string>
#include <vector>
#include <map>
using namespace std;

extern FILE *yyin;
extern int yyparse(std::vector<std::unique_ptr<BaseAST>>& ast_items);
extern void deal_koopa(const char* str, const char* fn);
extern void register_sysy_lib(SymbolTable &symtab);
extern std::string koopa_sysy_lib_decls();

int main(int argc, const char *argv[])
{
    assert(argc == 5);
    auto mode = argv[1];
    auto input = argv[2];
    auto output = argv[4];

    yyin = fopen(input, "r");
    assert(yyin);

    std::vector<std::unique_ptr<BaseAST>> ast_items;
    auto ret = yyparse(ast_items);
    assert(!ret);

    SymbolTable root_tab;
    register_sysy_lib(root_tab);

    std::vector<std::string> code;
    std::string koopa_ir;
    koopa_ir += koopa_sysy_lib_decls();

    //std::cerr << "Warning: Input does not have a single CompUnitAST root. Processing each AST node individually.\n";
    // 向前兼容：每个AST节点单独处理
    for (auto& item : ast_items) 
    {
        item->SemanticCheck(root_tab);
    }
    root_tab.Print();
    for (auto& item : ast_items) 
    {
        koopa_ir += item->EmitKoopa(code, root_tab);
        //std::cerr << "After emitting Koopa IR for an AST node, koopa_ir = " << std::endl << koopa_ir << std::endl;
    }

    if (mode[1] == 'k') {
        std::ofstream ofs(output, std::ios::out | std::ios::trunc);
        ofs << koopa_ir;
        ofs.close();
    }
    else if (mode[1] == 'r') {
        deal_koopa(koopa_ir.c_str(), output);
    }
}