#include <string>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <mpi.h>

#include <cstdint>
#include <cstdio>
#include <stdbool.h>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>


extern "C" {
    void* alloc_tracker_log_malloc_call(uint64_t, bool, const char*, bool, bool);
    void* alloc_tracker_log_calloc_call(uint64_t, uint64_t, bool, bool, const char*, bool, bool);
    void* alloc_tracker_log_realloc_call(uint64_t, bool, const char*, bool, bool);
    void* alloc_tracker_log_aligned_alloc_call(uint64_t, bool, const char*, bool, bool);

    void alloc_tracker_mpi_init_called();
    void alloc_tracker_mpi_finalize_called();

    void alloc_tracker_program_start();
    void alloc_tracker_program_end();
}
