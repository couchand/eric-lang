// type data implementation

#include "types.h"

// static methods

std::map<std::string, TypeData *> TypeData::types;

TypeData *TypeData::getType(std::string name) {
  return TypeData::types[name];
}

void TypeData::registerType(TypeData *type) {
  TypeData::types[type->getName()] = type;
}

// basic type methods

bool BasicTypeData::canConvertTo(TypeData *other) {
  return 0 < conversions.count(other);
}

llvm::Value *BasicTypeData::convertTo(llvm::IRBuilder<> builder, TypeData *other, llvm::Value *value) {
  if (!canConvertTo(other)) {
    return 0;
  }

  ConversionFunction converter = conversions[other];
  return converter(builder, value);
}

TypeData *BasicTypeData::getConverterType(TypeData *other) {
  if (!canConvertTo(other)) {
    return 0;
  }

  std::vector<TypeData *> ps;
  ps.push_back(this);
  return new FunctionTypeData(other, ps);
}

// function type methods

std::string FunctionTypeData::getName() {
  std::string name = "(";
  if (parameterTypes.size() > 0) {
    name += parameterTypes[0]->getName();

    for (unsigned i = 1, e = parameterTypes.size(); i < e; i++) {
      name += "," + parameterTypes[i]->getName();
    }
  }
  name += ")" + returnType->getName();

  return name;
}

llvm::Type *FunctionTypeData::getLLVMType() {
  llvm::Type *returns = returnType->getLLVMType();

  std::vector<llvm::Type *> takes;
  for (unsigned i = 0, e = parameterTypes.size(); i < e; i++) {
    takes.push_back(parameterTypes[i]->getLLVMType());
  }

  return llvm::FunctionType::get(returns, takes, false);
}

llvm::DIType FunctionTypeData::getDIType(llvm::DIFile *where, llvm::DIBuilder *diBuilder) {
  llvm::SmallVector<llvm::Value *, 8> paramTypes;

  paramTypes.push_back(returnType->getDIType(where, diBuilder));
  for (unsigned i = 0, e = parameterTypes.size(); i < e; i++) {
    paramTypes.push_back(parameterTypes[i]->getDIType(where, diBuilder));
  }

  llvm::DIArray paramTypeArray = diBuilder->getOrCreateArray(paramTypes);
  return diBuilder->createSubroutineType(*where, paramTypeArray);
}

// basic types

llvm::Value *convertBooleanToInteger(llvm::IRBuilder<> irBuilder, llvm::Value *value) {
  return irBuilder.CreateZExt(value, TypeData::getType("integer")->getLLVMType(), "casttmp");
}

llvm::Value *convertBooleanToNumber(llvm::IRBuilder<> irBuilder, llvm::Value *value) {
  return irBuilder.CreateUIToFP(value, TypeData::getType("number")->getLLVMType(), "casttmp");
}

llvm::Value *convertIntegerToBoolean(llvm::IRBuilder<> irBuilder, llvm::Value *value) {
  return irBuilder.CreateTrunc(value, TypeData::getType("boolean")->getLLVMType(), "casttmp");
}

llvm::Value *convertIntegerToNumber(llvm::IRBuilder<> irBuilder, llvm::Value *value) {
  return irBuilder.CreateSIToFP(value, TypeData::getType("number")->getLLVMType(), "casttmp");
}

llvm::Value *convertNumberToBoolean(llvm::IRBuilder<> irBuilder, llvm::Value *value) {
  llvm::Value *zero = llvm::ConstantFP::get(TypeData::getType("number")->getLLVMType(), 0);
  return irBuilder.CreateFCmpUNE(value, zero, "casttmp");
}

llvm::Value *convertNumberToInteger(llvm::IRBuilder<> irBuilder, llvm::Value *value) {
  return irBuilder.CreateFPToSI(value, TypeData::getType("integer")->getLLVMType(), "casttmp");
}

void InitializeBasicTypes(llvm::LLVMContext &context, llvm::DIBuilder *builder) {

  BasicTypeData *booleanType = new BasicTypeData(
    "boolean",
    llvm::TypeBuilder<llvm::types::i<1>, true>::get(context),
    builder->createBasicType("boolean", 1, 1, llvm::dwarf::DW_ATE_boolean)
  );

  BasicTypeData *integerType = new BasicTypeData(
    "integer",
    llvm::TypeBuilder<llvm::types::i<64>, true>::get(context),
    builder->createBasicType("integer", 64, 64, llvm::dwarf::DW_ATE_signed)
  );

  BasicTypeData *numberType = new BasicTypeData(
    "number",
    llvm::TypeBuilder<llvm::types::ieee_double, true>::get(context),
    builder->createBasicType("number", 64, 64, llvm::dwarf::DW_ATE_float)
  );

  booleanType->addConversion(integerType, convertBooleanToInteger);
  booleanType->addConversion(numberType,  convertBooleanToNumber );
  integerType->addConversion(booleanType, convertIntegerToBoolean);
  integerType->addConversion(numberType,  convertIntegerToNumber );
  numberType ->addConversion(booleanType, convertNumberToBoolean );
  numberType ->addConversion(integerType, convertNumberToInteger );

  TypeData::registerType(booleanType);
  TypeData::registerType(integerType);
  TypeData::registerType(numberType);

  TypeData::registerType(booleanType->getConverterType(integerType));
  TypeData::registerType(booleanType->getConverterType(numberType));
  TypeData::registerType(integerType->getConverterType(booleanType));
  TypeData::registerType(integerType->getConverterType(numberType));
  TypeData::registerType(numberType->getConverterType(booleanType));
  TypeData::registerType(numberType->getConverterType(integerType));

}
