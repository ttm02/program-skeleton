#include "logging.hpp"

namespace logging {

void declare_function(Module& M, LLVMContext& Ctx, const std::string& func_name, FunctionType* func_Ty) {
    if (M.getFunction(func_name)) return;

    FunctionCallee FC = M.getOrInsertFunction(func_name, func_Ty);
    if (auto* F = llvm::dyn_cast<llvm::Function>(FC.getCallee())) {
        F->setLinkage(GlobalValue::ExternalLinkage);

        F->addFnAttr(Attribute::NoInline);
        F->addFnAttr(Attribute::OptimizeNone);
    }

    llvm::errs() << "[AllocTrackerLTOPass] Declared function " << func_name << "\n";
}

void declare_logging_functions(Module& M, LLVMContext& Ctx) {
    // Declare malloc logging function
    declare_function(M, Ctx,
        // function name
        "alloc_tracker_log_malloc_call",
        FunctionType::get(
            // function return type
            PointerType::getUnqual(Ctx), 
            // function parameter types
            {   
                Type::getInt64Ty(Ctx),          // size
                Type::getInt1Ty(Ctx),           // is_constant_size
                PointerType::getUnqual(Ctx),    // parent_func_name
                Type::getInt1Ty(Ctx),           // is_valid
                Type::getInt1Ty(Ctx),           // is_required_to_allocate
            },
            false
        )
    );

    // Declare calloc logging function
    declare_function(M, Ctx,
        "alloc_tracker_log_calloc_call",
        FunctionType::get(
            PointerType::getUnqual(Ctx),
            {
                Type::getInt64Ty(Ctx),          // num
                Type::getInt64Ty(Ctx),          // size
                Type::getInt1Ty(Ctx),           // is_constant_num
                Type::getInt1Ty(Ctx),           // is_constant_size
                PointerType::getUnqual(Ctx),    // parent_func_name
                Type::getInt1Ty(Ctx),           // is_valid
                Type::getInt1Ty(Ctx),           // is_required_to_allocate
            },
            false
        )
    );

    // Declare realloc logging function
    declare_function(M, Ctx,
        "alloc_tracker_log_realloc_call",
        FunctionType::get(
            PointerType::getUnqual(Ctx),
            {
                Type::getInt64Ty(Ctx),          // size
                Type::getInt1Ty(Ctx),           // is_constant_size
                PointerType::getUnqual(Ctx),    // parent_func_name
                Type::getInt1Ty(Ctx),           // is_valid
                Type::getInt1Ty(Ctx),           // is_required_to_allocate
            },
            false
        )
    );

    // Declare aligned_alloc logging function
    declare_function(M, Ctx,
        "alloc_tracker_log_aligned_alloc_call",
        FunctionType::get(
            PointerType::getUnqual(Ctx),
            {
                Type::getInt64Ty(Ctx),          // size
                Type::getInt1Ty(Ctx),           // is_constant_size
                PointerType::getUnqual(Ctx),    // parent_func_name
                Type::getInt1Ty(Ctx),           // is_valid
                Type::getInt1Ty(Ctx),           // is_required_to_allocate
            },
            false
        )
    );
}


GlobalVariable* get_or_create_global_string_constant(Module& M, LLVMContext& Ctx, std::string str) {
    // Name of the global constant string variable
    std::string global_var_name = "instruction_str";

    // Check if the string already exists as a global string constant
    for (auto& GV : M.getGlobalList()) {
        if (GV.isConstant() && GV.hasInitializer()) {
            if (auto* CDA = dyn_cast<ConstantDataArray>(GV.getInitializer())) {
                if (CDA->isString() && CDA->getAsString().rtrim('\0') == str) {
                    return &GV;
                }
            }
        }
    }

    // Create global string constant
    Constant* instruction_string_global_C = ConstantDataArray::getString(Ctx, str);
    return new GlobalVariable(
        M,
        instruction_string_global_C->getType(),
        true, // isConstant
        GlobalValue::PrivateLinkage,
        instruction_string_global_C,
        global_var_name
    );
}

GlobalVariable* get_parent_function_name_V(Module& M, LLVMContext& Ctx, Function& F) {
    StringRef parent_func_name = F.getName();

    // Check if the global variable already exists
    std::string global_var_name = "func_name_" + parent_func_name.str();

    for (auto& GV : M.getGlobalList()) {
        if (GV.getName().startswith(global_var_name)) {
            if (GV.isConstant() && GV.hasInitializer()) {
                if (auto* CDA = dyn_cast<ConstantDataArray>(GV.getInitializer())) {
                    if (CDA->isString() && CDA->getAsString().rtrim('\0') == parent_func_name) {
                        return &GV;
                    }
                }
            }
        }
    }

    Constant* parent_func_name_array = ConstantDataArray::getString(Ctx, parent_func_name);
    return new GlobalVariable(
        M,
        parent_func_name_array->getType(),
        true, // isConstant
        GlobalValue::PrivateLinkage,
        parent_func_name_array,
        "func_name_" + parent_func_name
    );
}

bool has_call_I_further_uses(CallInst* call_I) {
    return !call_I->getType()->isVoidTy() && !call_I->use_empty();
}

void instrument_start_of_program(Module& M, IRBuilder<>& builder) {
    LLVMContext& Ctx = M.getContext();

    logging::declare_function(M, Ctx, "alloc_tracker_program_start", FunctionType::get(Type::getVoidTy(Ctx), {}, false));
    Function* program_start_instrumentation_F = M.getFunction("alloc_tracker_program_start");
    builder.CreateCall(program_start_instrumentation_F, {});
}

void instrument_end_of_program(Module& M, IRBuilder<>& builder) {
    LLVMContext& Ctx = M.getContext();
    
    logging::declare_function(M, Ctx, "alloc_tracker_program_end", FunctionType::get(Type::getVoidTy(Ctx), {}, false));
    Function* program_end_instrumentation_F = M.getFunction("alloc_tracker_program_end");

    builder.CreateCall(program_end_instrumentation_F, {});
}


} // namespace: logging