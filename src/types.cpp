// type data implementation

#include "types.h"

static std::string dataName(void *d) {
  return ((TypeData *)d)->getName();
}

static std::string specName(void *s) {
  return ((TypeSpecifier *)s)->getName();
}

typedef std::string (*nameFn)(void *);

static std::string functionTypeName(std::vector<void *> parameterTypes, void *returnType, nameFn getName) {
  std::string name = "(";
  if (parameterTypes.size() > 0) {
    name += getName(parameterTypes[0]);

    for (unsigned i = 1, e = parameterTypes.size(); i < e; i++) {
      name += "," + getName(parameterTypes[i]);
    }
  }
  name += ")" + getName(returnType);

  return name;
}

static std::string arrayTypeName(void *elementType, nameFn getName) {
  std::string name = "[";
  name += getName(elementType);
  name += "]";
  return name;
}

// type specifiers

std::string FunctionTypeSpecifier::getName() {
  std::vector<void *> pts;
  for (unsigned i = 0, e = parameterTypes.size(); i < e; i++) {
    pts.push_back(parameterTypes[i]);
  }
  return functionTypeName(pts, returnType, specName);
}

std::string ArrayTypeSpecifier::getName() {
  return arrayTypeName(elementType, specName);
}

// types

// static methods

std::map<std::string, TypeData *> TypeData::types;
std::map<std::string, FunctionTypeData *> FunctionTypeData::functionTypes;

TypeData *TypeData::getType(std::string name) {
  TypeSpecifier *t = new BasicTypeSpecifier(name);
  return getType(t);
}

TypeData *TypeData::getType(TypeSpecifier *specifier) {
  std::string name = specifier->getName();
  return TypeData::types[name];
}

void TypeData::registerType(TypeData *type) {
  TypeData::types[type->getName()] = type;
}

FunctionTypeData *FunctionTypeData::getFunctionType(std::string name) {
  return FunctionTypeData::functionTypes[name];
}

void FunctionTypeData::registerFunctionType(std::string name, FunctionTypeData *type) {
  FunctionTypeData::functionTypes[name] = type;
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

llvm::DIType FunctionTypeData::getDIType(DebugContext *context) {
  llvm::SmallVector<llvm::Value *, 8> paramTypes;

  paramTypes.push_back(returnType->getDIType(context));
  for (unsigned i = 0, e = parameterTypes.size(); i < e; i++) {
    paramTypes.push_back(parameterTypes[i]->getDIType(context));
  }

  llvm::DIArray paramTypeArray = context->getBuilder()->getOrCreateArray(paramTypes);
  return context->getBuilder()->createSubroutineType(context->getFile(), paramTypeArray);
}

// struct type methods

llvm::Type *StructTypeData::getLLVMType() {
  // already made one
  if (llvmType) return llvmType;

  llvm::SmallVector<llvm::Type *, 8> fTypes;

  //fprintf(stdout, "getting llvm type for %s\n", name.c_str());

  for (unsigned i = 0, e = fieldTypes.size(); i < e; i++) {
    std::string field = fieldNames[i];
    TypeData *fieldType = fieldTypes[i];
    if (!fieldType) return 0;

    fTypes.push_back(fieldType->getLLVMType());
    if (!fTypes.back()) return 0;
  }

  llvmType = llvm::StructType::create(fTypes, name);
  return llvmType;
}

llvm::DIType StructTypeData::getDIType(DebugContext *context) {
  if (hasDIType) {
    return diType;
  }

  llvm::SmallVector<llvm::Value *, 8> fields;
  for (unsigned i = 0, e = fieldTypes.size(); i < e; i++) {
    std::string fieldName = fieldNames[i];
    TypeData *fieldType = fieldTypes[i];
    if (!fieldType) return llvm::DIType();

    llvm::Type *fieldLLVMType = fieldType->getLLVMType();
    if (!fieldLLVMType) return llvm::DIType();

    uint64_t elsize = context->getDataLayout()->getTypeSizeInBits(fieldLLVMType);
    uint64_t elalign = context->getDataLayout()->getABITypeAlignment(fieldLLVMType);
    uint64_t eloffset = i * elsize;
    llvm::DIType t = fieldType->getDIType(context);

    fields.push_back(context->getBuilder()->createMemberType(llvm::DIDescriptor(), fieldName, context->getFile(), 0, elsize, elalign, eloffset, llvm::dwarf::DW_ACCESS_public, t));
    if (!fields.back()) return llvm::DIType();
  }

  llvm::Type *llvmType = getLLVMType();

  uint64_t size = context->getDataLayout()->getTypeSizeInBits(llvmType);
  uint64_t align = context->getDataLayout()->getABITypeAlignment(llvmType);
  uint64_t offset = 0;

  llvm::DIArray elements = context->getBuilder()->getOrCreateArray(fields);

  diType = context->getBuilder()->createStructType(llvm::DIDescriptor(), name, context->getFile(), 0, size, align, offset, llvm::DIType(), elements);
  hasDIType = true;

  return diType;
}

// array type

std::string ArrayTypeData::getName() {
  std::string name = "[";
  name += MemberType->getName();
  name += "]";
  return name;
}

llvm::Type *ArrayTypeData::getLLVMType() {
  llvm::SmallVector<llvm::Type *, 8> fTypes;

  TypeData *integerType = TypeData::getType("integer");
  fTypes.push_back(integerType->getLLVMType());

  llvm::Type *elType = MemberType->getLLVMType();
  fTypes.push_back(llvm::ArrayType::get(elType, 0));

  llvm::Type *dataStruct = llvm::StructType::get(llvm::getGlobalContext(), fTypes);

  return llvm::PointerType::get(dataStruct, 0);
}

llvm::DIType ArrayTypeData::getDIType(DebugContext *context) {
  return context->getBuilder()->createBasicType("integer", 64, 64, llvm::dwarf::DW_ATE_signed);
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

  BasicTypeData *voidType = new BasicTypeData(
    "void",
    llvm::Type::getVoidTy(context),
    llvm::DIType()
  );

  BasicTypeData *booleanType = new BasicTypeData(
    "boolean",
    llvm::TypeBuilder<llvm::types::i<1>, true>::get(context),
    builder->createBasicType("boolean", 1, 1, llvm::dwarf::DW_ATE_boolean)
  );

  BasicTypeData *byteType = new BasicTypeData(
    "byte",
    llvm::TypeBuilder<llvm::types::i<8>, true>::get(context),
    builder->createBasicType("byte", 8, 8, llvm::dwarf::DW_ATE_unsigned)
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

  TypeData::registerType(voidType);
  TypeData::registerType(booleanType);
  TypeData::registerType(byteType);
  TypeData::registerType(integerType);
  TypeData::registerType(numberType);

  TypeData::registerType(booleanType->getConverterType(integerType));
  TypeData::registerType(booleanType->getConverterType(numberType));
  TypeData::registerType(integerType->getConverterType(booleanType));
  TypeData::registerType(integerType->getConverterType(numberType));
  TypeData::registerType(numberType->getConverterType(booleanType));
  TypeData::registerType(numberType->getConverterType(integerType));

  ArrayTypeData *booleanArrayType = new ArrayTypeData(booleanType);
  ArrayTypeData *byteArrayType = new ArrayTypeData(byteType);
  ArrayTypeData *integerArrayType = new ArrayTypeData(integerType);
  ArrayTypeData *numberArrayType = new ArrayTypeData(numberType);

  TypeData::registerType(booleanArrayType);
  TypeData::registerType(byteArrayType);
  TypeData::registerType(integerArrayType);
  TypeData::registerType(numberArrayType);

}
