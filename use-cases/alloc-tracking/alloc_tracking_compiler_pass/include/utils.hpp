#pragma once

#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ADT/StringRef.h"

#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>


namespace utils {
    using namespace llvm;

    bool is_allocation_call(llvm::StringRef name);

    bool verify_module(Module& M);

    std::string get_string_from_V(Value* V);
    std::string trim_V_str(const std::string& str);


    template <typename T>
    void remove_duplicates_from_vector(std::vector<T>& v) {
        std::unordered_set<T> us;
        auto it = std::remove_if(v.begin(), v.end(), 
            [&us](const T& item) {
                return !us.insert(item).second;
            });
        v.erase(it, v.end());
    }

} // namespace: utils

