// type support

#ifndef _TYPES_H
#define _TYPES_H

#include <map>
#include <string>
#include <vector>

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DIBuilder.h"

typedef llvm::Value *(*ConversionFunction)(llvm::IRBuilder<>, llvm::Value *);

class TypeData {
  static std::map<std::string, TypeData *> types;

public:
  virtual std::string  getName()     = 0;
  virtual llvm::Type  *getLLVMType() = 0;
  virtual llvm::DIType getDIType(llvm::DIFile *where, llvm::DIBuilder *diBuilder)   = 0;

  virtual bool canConvertTo(TypeData *other) { return false; }
  virtual llvm::Value *convertTo(llvm::IRBuilder<> builder, TypeData *other, llvm::Value *value) { return 0; }
  virtual TypeData *getConverterType(TypeData *other) { return 0; }

  static TypeData *getType(std::string name);
  static void registerType(TypeData *type);

};

class FunctionTypeData : public TypeData {
  TypeData *returnType;
  std::vector<TypeData *> parameterTypes;

public:
  FunctionTypeData(TypeData *returns, std::vector<TypeData *> takes)
  : returnType(returns), parameterTypes(takes) {}

  virtual std::string getName();
  virtual llvm::Type *getLLVMType();
  virtual llvm::DIType getDIType(llvm::DIFile *where, llvm::DIBuilder *diBuilder);

  unsigned getNumParameters() { return parameterTypes.size(); }
  TypeData *getParameterType(unsigned i) { return parameterTypes[i]; }
  TypeData *getReturnType() { return returnType; }
};

class BasicTypeData : public TypeData {
  std::string name;
  llvm::Type   *llvmType;
  llvm::DIType  diType;

  std::map<TypeData *, ConversionFunction> conversions;

public:
  BasicTypeData(std::string n, llvm::Type *lt, llvm::DIType dt)
    : name(n), llvmType(lt), diType(dt) {}

  virtual std::string getName() { return name; }
  virtual llvm::Type  *getLLVMType() { return llvmType; }
  virtual llvm::DIType getDIType(llvm::DIFile *where, llvm::DIBuilder *diBuilder) { return diType; }

  void addConversion(TypeData *other, ConversionFunction converter) {
    conversions[other] = converter;
  }

  virtual bool canConvertTo(TypeData *other);
  virtual llvm::Value *convertTo(llvm::IRBuilder<> builder, TypeData *other, llvm::Value *value);
  virtual TypeData *getConverterType(TypeData *other);
};

void InitializeBasicTypes(llvm::LLVMContext &context, llvm::DIBuilder *builder);

#endif
