// compiler context

#ifndef _CONTEXT_H
#define _CONTEXT_H

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DIBuilder.h"

class DebugContext {
  llvm::DIFile      *where;
  llvm::DIBuilder   *diBuilder;
  llvm::DataLayout  *dataLayout;

public:
  DebugContext()
    : where(0), diBuilder(0) {}
  DebugContext(llvm::DIFile *where, llvm::DIBuilder *diBuilder, llvm::DataLayout *dataLayout)
    : where(where), diBuilder(diBuilder), dataLayout(dataLayout) {}

  llvm::DIFile getFile() { return where ? *where : llvm::DIFile(); }
  llvm::DIBuilder *getBuilder() { return diBuilder; }
  llvm::DataLayout *getDataLayout() { return dataLayout; }
};

#endif
