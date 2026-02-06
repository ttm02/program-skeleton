#pragma once

#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"


namespace io {
    using namespace llvm;

    void write_module_dump_to_file(Module& M, const std::string& filename);
    void cleanup_previously_generated_files();
    void log_time_for(const std::string& timed_for, double time);

} // namespace: io