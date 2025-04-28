#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

/**
 * Example: Concurrent event processing
 */

int main() {
    std::cout << "Running: Concurrent event processing" << std::endl;
    
    // Initialize subsystems
    std::vector<std::thread> threads;
    
    // Create worker threads
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([]() {
            for (int j = 0; j < 100; ++j) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Example completed successfully" << std::endl;
    return 0;
}
