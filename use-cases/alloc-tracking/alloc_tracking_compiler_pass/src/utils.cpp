#include "utils.hpp"

namespace utils {

bool is_allocation_call(llvm::StringRef name) {
    if (name == "malloc") return true;
    if (name == "calloc") return true;
    if (name == "realloc") return true;
    if (name == "aligned_alloc") return true;

    return false;
}

bool verify_module(Module& M) {
    std::string error_msg;
    llvm::raw_string_ostream error_stream(error_msg);
    
    if (llvm::verifyModule(M, &error_stream)) {
        llvm::errs() << "[AllocTrackerLTOPass] IR VERIFICATION FAILED:\n" << error_msg << "\n";
        return false;
    }

    llvm::errs() << "[AllocTrackerLTOPass] IR is verified!\n";
    return true;
}

std::string get_string_from_V(llvm::Value* V) {
    std::string I_str;
    llvm::raw_string_ostream rso(I_str);
    V->print(rso);
    rso.flush();

    return I_str;
}

std::string trim_V_str(const std::string& str) {
    // trim away leading whitespaces and everything after the closing paren after arguments
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos) return "";

    size_t open_paren = str.find('(', start);
    if (open_paren == std::string::npos) return str.substr(start);

    size_t close_paren = str.find(')', open_paren);
    if (close_paren == std::string::npos) return str.substr(start);

    return str.substr(start, close_paren - start + 1);
}


} // namespace: utils