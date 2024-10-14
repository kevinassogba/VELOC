#ifndef __STATEDIFF_MODULE_HPP
#define __STATEDIFF_MODULE_HPP

#include "common/command.hpp"
#include "common/config.hpp"
#include "common/file_util.hpp"
#include "common/status.hpp"
#include "io_uring_stream.hpp"
#include "statediff.hpp"
#include <fcntl.h>
#include <openssl/sha.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define __DEBUG

using namespace state_diff;

class statediff_module_t {
    const config_t &cfg;
    bool active;
    bool fuzzy_hash;
    int start_level = 13;
    double error_tolerance = 1e-3;
    int chunk_size = 4096;
    std::string data_type = "float";

    io_uring_stream_t<float> *prev_reader = NULL;
    io_uring_stream_t<float> *local_reader = NULL;
    state_diff::client_t<float, io_uring_stream_t> *prev_client = NULL;
    state_diff::client_t<float, io_uring_stream_t> *local_client = NULL;

  public:
    statediff_module_t(const config_t &c);
    ~statediff_module_t();
    int process_command(const command_t &c);
};

#endif   //__STATEDIFF_MODULE_HPP
