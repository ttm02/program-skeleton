#pragma once

#include <chrono>


namespace timer {

    class CustomTimer {
    public:
        void start() {
            start_time_point = std::chrono::high_resolution_clock::now();
        }

        double stop() {
            auto end_time_point = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end_time_point - start_time_point;
            return duration.count();
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> start_time_point;
    };

};  // namespace: timer
