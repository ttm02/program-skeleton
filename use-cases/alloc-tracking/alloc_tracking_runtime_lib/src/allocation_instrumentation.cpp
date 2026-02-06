#include "allocation_instrumentation.hpp"


static bool mpi_is_initialized = false;
static int cached_mpi_rank = -1;


static time_t& get_time() {
    static time_t time_now = std::time(nullptr);
    return time_now;
}

static int safe_get_mpi_rank() {
    // Return rank -1 when mpi is not yet initialized
    if (!mpi_is_initialized) return -1;
    
    if (cached_mpi_rank == -1) {
        MPI_Comm_rank(MPI_COMM_WORLD, &cached_mpi_rank);
    }
    
    return cached_mpi_rank;
}


extern "C" void* alloc_tracker_log_malloc_call(
    uint64_t size,
    bool is_constant_size,
    const char* parent_func_name,
    bool is_valid,
    bool is_required_to_allocate
) {
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    std::stringstream ss;
    ss << "malloc: "
       << "timestamp " << seconds << ", "
       << "in function " << parent_func_name << ", "
       << "allocation size " << size << ", "
       << "constant size " << (is_constant_size ? "true" : "false") << ", "
       << "rank " << safe_get_mpi_rank() << ", "
       << "valid " << (is_valid ? "true" : "false") << ", "
       << "required to allocate " << (is_required_to_allocate ? "true" : "false")
       << "\n";
    
    std::stringstream file_ss;
    file_ss << get_time() << "_allocation_log_pid_" << getpid() << ".txt";

    std::ofstream file(file_ss.str(), std::ios_base::app); // if file does not exist yet, it will be created here
    file << ss.str();

    return NULL;
}

extern "C" void* alloc_tracker_log_calloc_call(
    uint64_t num,
    uint64_t size,
    bool is_constant_num,
    bool is_constant_size,
    const char* parent_func_name,
    bool is_valid,
    bool is_required_to_allocate
) {
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    uint64_t prod = num * size;

    std::stringstream ss;
    ss << "calloc: "
       << "timestamp " << seconds << ", "
       << "in function " << parent_func_name << ", "
       << "allocation size " << prod << "( " << num << " * " << size << " ), "
       << "constant num " << (is_constant_num ? "true" : "false") << ", "
       << "constant size " << (is_constant_size ? "true" : "false") << ", "
       << "rank " << safe_get_mpi_rank() << ", "
       << "valid " << (is_valid ? "true" : "false") << ", "
       << "required to allocate " << (is_required_to_allocate ? "true" : "false")
       << "\n";
    
    std::stringstream file_ss;
    file_ss << get_time() << "_allocation_log_pid_" << getpid() << ".txt";

    std::ofstream file(file_ss.str(), std::ios_base::app); // if file does not exist yet, it will be created here
    file << ss.str();

    return NULL;
}

extern "C" void* alloc_tracker_log_realloc_call(
    uint64_t size,
    bool is_constant_size,
    const char* parent_func_name,
    bool is_valid,
    bool is_required_to_allocate
) {
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    std::stringstream ss;
    ss << "realloc: "
       << "timestamp " << seconds << ", "
       << "in function " << parent_func_name << ", "
       << "allocation size " << size << ", "
       << "constant size " << (is_constant_size ? "true" : "false") << ", "
       << "rank " << safe_get_mpi_rank() << ", "
       << "valid " << (is_valid ? "true" : "false") << ", "
       << "required to allocate " << (is_required_to_allocate ? "true" : "false")
       << "\n";
    
    std::stringstream file_ss;
    file_ss << get_time() << "_allocation_log_pid_" << getpid() << ".txt";

    std::ofstream file(file_ss.str(), std::ios_base::app); // if file does not exist yet, it will be created here
    file << ss.str();

    return NULL;
}

extern "C" void* alloc_tracker_log_aligned_alloc_call(
    uint64_t size,
    bool is_constant_size,
    const char* parent_func_name,
    bool is_valid,
    bool is_required_to_allocate
) {
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    std::stringstream ss;
    ss << "aligned_alloc: "
       << "timestamp " << seconds << ", "
       << "in function " << parent_func_name << ", "
       << "allocation size " << size << ", "
       << "constant size " << (is_constant_size ? "true" : "false") << ", "
       << "rank " << safe_get_mpi_rank() << ", "
       << "valid " << (is_valid ? "true" : "false") << ", "
       << "required to allocate " << (is_required_to_allocate ? "true" : "false")
       << "\n";
    
    std::stringstream file_ss;
    file_ss << get_time() << "_allocation_log_pid_" << getpid() << ".txt";

    std::ofstream file(file_ss.str(), std::ios_base::app); // if file does not exist yet, it will be created here
    file << ss.str();
    
    return NULL;
}

extern "C" void alloc_tracker_mpi_init_called() {
    mpi_is_initialized = true;
    cached_mpi_rank = -1;
}
    
extern "C" void alloc_tracker_mpi_finalize_called() {
    mpi_is_initialized = false;
    cached_mpi_rank = -1;
}

extern "C" void alloc_tracker_program_start() {
    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    
    std::stringstream file_ss;
    file_ss << get_time() << "_allocation_log_pid_" << getpid() << ".txt";
    
    std::stringstream ss;
    ss << "program start: " << ns << "\n";
    
    std::ofstream file(file_ss.str(), std::ios_base::app);
    file << ss.str();
}
    
extern "C" void alloc_tracker_program_end() {
    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    std::stringstream file_ss;
    file_ss << get_time() << "_allocation_log_pid_" << getpid() << ".txt";

    std::stringstream ss;
    ss << "program end: " << ns << "\n";

    std::ofstream file(file_ss.str(), std::ios_base::app);
    file << ss.str();
}
