#include "io.hpp"

namespace io {

void write_module_dump_to_file(Module& M, const std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream out_file_stream(filename, EC, llvm::sys::fs::OF_Text);
    if (EC) {
        llvm::errs() << "[AllocTrackerLTOPass] Error opening file: " << EC.message() << "\n";
        return;
    }

    llvm::errs() << "[AllocTrackerLTOPass] Writing module dump to file " << filename << " ...\n";
    M.print(out_file_stream, nullptr);
}

void cleanup_previously_generated_files() {
    llvm::sys::fs::remove("slicing_information.txt");
    llvm::sys::fs::remove("try_information.txt");
}

void log_time_for(const std::string& timed_for, double time) {
    std::error_code EC;
    llvm::raw_fd_ostream out_file_stream("slicing_information.txt", EC, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
    if (!EC) out_file_stream << timed_for << " duration: " << time << "\n";        
}


} // namespace: io