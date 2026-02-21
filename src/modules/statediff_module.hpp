#ifndef __STATEDIFF_MODULE_HPP
#define __STATEDIFF_MODULE_HPP

#include "common/ckpt_util.hpp"
#include "common/command.hpp"
#include "common/config.hpp"
#include "common/file_util.hpp"
#include "common/status.hpp"
#include "liburing_reader.hpp"
#include "statediff.hpp"
#include <fcntl.h>
#include <openssl/sha.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "debug.hpp"
#include <fstream>
#include <filesystem>

using namespace state_diff;

class statediff_module_t {
    const config_t &cfg;
    bool compare;
    bool fuzzy_hash;
    int start_level = 13;
    double error_tolerance = 1e-3;
    int chunk_size = 4096;
    std::string data_type = "float";
    std::ofstream debug_log;

    state_diff::client_t<float> local_client;
    state_diff::client_t<float> prev_client;

  public:
    statediff_module_t(const config_t &c);
    int process_command(const command_t &c);
};

#endif   //__STATEDIFF_MODULE_HPP
