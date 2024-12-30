#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <chrono>
#include <numeric>
#include <algorithm>

/**
 * Performance profiling tool
 * Usage: telltale-profile <log-file> [options]
 */

class LogAnalyzer {
public:
    void load_and_analyze(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Cannot open: " << filename << std::endl;
            return;
        }
        
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        
        analyze_data(data);
    }
    
    void analyze_data(const std::vector<uint8_t>& data) {
        if (data.empty()) {
            std::cout << "Empty file" << std::endl;
            return;
        }
        
        std::map<uint8_t, uint64_t> histogram;
        for (uint8_t byte : data) {
            histogram[byte]++;
        }
        
        std::cout << "File size: " << data.size() << " bytes" << std::endl;
        std::cout << "Unique byte values: " << histogram.size() << std::endl;
        
        uint64_t max_count = 0;
        uint8_t max_byte = 0;
        for (const auto& pair : histogram) {
            if (pair.second > max_count) {
                max_count = pair.second;
                max_byte = pair.first;
            }
        }
        
        std::cout << "Most common byte: 0x" << std::hex << (int)max_byte 
                  << " (appears " << std::dec << max_count << " times)" << std::endl;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <log-file>" << std::endl;
        return 1;
    }
    
    LogAnalyzer analyzer;
    analyzer.load_and_analyze(argv[1]);
    
    return 0;
}
