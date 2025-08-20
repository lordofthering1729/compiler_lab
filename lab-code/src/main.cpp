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
extern int yyparse(unique_ptr<BaseAST>& ast);
extern void deal_koopa(const char* str,const char* fn);

int main(int argc, const char *argv[]) 
{
    std::cout<< "Starting main function...\n";
    assert(argc == 5);
    auto mode = argv[1];
    auto input = argv[2];
    auto output = argv[4];

    yyin = fopen(input, "r");
    assert(yyin);

    unique_ptr<BaseAST> ast;
    auto ret = yyparse(ast);
    assert(!ret);

    SymbolTable root_tab;
    ast->SemanticCheck(root_tab); // 语义检查

    std::cout << "Semantic check passed.\n";
    if(mode[1] == 'k')
    {
        ast->Dump();
        std::vector<std::string> code;
        std::string koopa_ir = ast->EmitKoopa(code, root_tab);
        std::ofstream ofs(output, std::ios::out | std::ios::trunc);
        ofs << koopa_ir;
        ofs.close();
    }
    else if(mode[1] == 'r')
    {
        std::vector<std::string> code;
        std::string koopa_ir = ast->EmitKoopa(code, root_tab);
        deal_koopa(koopa_ir.c_str(), output);
    }
    std::cout<< "Ending main function...\n";
}