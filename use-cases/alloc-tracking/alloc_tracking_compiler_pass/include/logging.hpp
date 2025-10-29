#pragma once

#include "llvm/IR/Module.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"

#include <string>


namespace logging {
    using namespace llvm;

    void declare_function(Module& M, LLVMContext& Ctx, const std::string& func_name, FunctionType* func_Ty);
    void declare_logging_functions(Module& M, LLVMContext& Ctx);

    GlobalVariable* get_parent_function_name_V(Module& M, LLVMContext& Ctx, Function& F);
    GlobalVariable* get_or_create_global_string_constant(Module& M, LLVMContext& Ctx, std::string str);

    bool has_call_I_further_uses(CallInst* call_I);

    void instrument_start_of_program(Module& M, IRBuilder<>& builder);
    void instrument_end_of_program(Module& M, IRBuilder<>& builder);
    
}; // namespace: logging