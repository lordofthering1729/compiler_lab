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

    if(mode[1] == 'k') 
    {
        ast->Dump();
        std::vector<std::string> code;
        std::string koopa_ir = ast->EmitKoopa(code);
        std::ofstream ofs(output, std::ios::out | std::ios::trunc);
        ofs << koopa_ir;
        ofs.close();
    }
    else if(mode[1] == 'r') 
    {
        std::vector<std::string> code;
        std::string koopa_ir = ast->EmitKoopa(code);
        deal_koopa(koopa_ir.c_str(), output);
    }
    return 0;
}