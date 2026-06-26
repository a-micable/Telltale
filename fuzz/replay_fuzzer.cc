#include "telltale/dispatcher.hpp"
#include <cstddef>
#include <cstdint>
#include <ostream>

namespace {

class NullStreamBuf : public std::streambuf {
protected:
    int overflow(int ch) override {
        return ch == EOF ? 0 : ch;
    }

    std::streamsize xsputn(const char*, std::streamsize n) override {
        return n;
    }
};

NullStreamBuf g_null_buf;
std::ostream g_null_stream(&g_null_buf);

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    try {
        telltale::Dispatcher dispatcher;
        dispatcher.set_output_stream(&g_null_stream);
        dispatcher.set_verbose(false);
        (void)dispatcher.replay_buffer(data, size, telltale::ReplayMode::Execute);
    } catch (...) {
    }
    return 0;
}
