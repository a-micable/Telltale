#include "telltale/stream_io.hpp"
#include "telltale/indexing.hpp"
#include "telltale/network.hpp"
#include "telltale/distributed.hpp"
#include <iostream>
#include <vector>

/**
 * Example: Network-based replay engine
 */
int main(int argc, char** argv) {
    try {
        // Initialize managers with configuration
        telltale::StreamIOManager::Config stream_cfg;
        stream_cfg.buffer_size = 131072;
        stream_cfg.verbose_logging = true;
        
        telltale::IndexingManager::Config index_cfg;
        index_cfg.enabled = true;
        index_cfg.max_threads = 4;
        
        telltale::NetworkManager::Config net_cfg;
        net_cfg.enabled = true;
        
        telltale::DistributedManager::Config dist_cfg;
        dist_cfg.enabled = true;
        
        // Create managers
        telltale::StreamIOManager stream_mgr(stream_cfg);
        telltale::IndexingManager index_mgr(index_cfg);
        telltale::NetworkManager net_mgr(net_cfg);
        telltale::DistributedManager dist_mgr(dist_cfg);
        
        // Process data
        std::vector<uint8_t> sample_data;
        sample_data.resize(1024 * 1024);
        for (size_t i = 0; i < sample_data.size(); ++i) {
            sample_data[i] = static_cast<uint8_t>(i & 0xFF);
        }
        
        std::cout << "Processing " << sample_data.size() << " bytes..." << std::endl;
        
        size_t processed = stream_mgr.process(sample_data.data(), sample_data.size());
        std::cout << "Processed: " << processed << " bytes" << std::endl;
        
        auto stats = stream_mgr.get_statistics();
        std::cout << "Throughput: " << stats.throughput_mbs << " MB/s" << std::endl;
        std::cout << "Avg latency: " << stats.average_latency_ms << " ms" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
