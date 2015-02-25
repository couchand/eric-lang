// codegen

#ifndef _CODEGEN_H
#define _CODEGEN_H

void InitializeCodegen(const char *filename);
void CreateMainFunction(std::vector<Function *> expressions);
void DumpAllCode();

#endif
