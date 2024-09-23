#ifndef __STATEDIFF_MODULE_HPP
#define __STATEDIFF_MODULE_HPP

#include "common/command.hpp"
#include "common/config.hpp"
#include "common/file_util.hpp"
#include "common/status.hpp"
#include "statediff.hpp"
#include "readers/io_uring_stream.hpp"
#include <fcntl.h>
#include <openssl/sha.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define __DEBUG
#include "common/debug.hpp"

class statediff_module_t {
    const config_t &cfg;
    bool active;
    int start_level = 13;
    double error_tolerance = 1e-3;
    int chunk_size = 4096;
    bool fuzzy_hash = true;
    string data_type = "float";

  public:
    statediff_module_t(const config_t &c);
    ~statediff_module_t() {}
    int process_command(const command_t &c);
};

#endif   //__STATEDIFF_MODULE_HPP
