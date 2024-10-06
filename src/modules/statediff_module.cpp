#include "statediff_module.hpp"

statediff_module_t::statediff_module_t(const config_t &c) : cfg(c) {
    active = cfg.get_bool("statediff", false);
    if (active && !check_dir(cfg.get("persistent"))) {
        ERROR("Reference directory "
              << cfg.get("persistent")
              << " inaccessible. Statediff deactivated!");
        active = false;
    }
    fuzzy_hash = cfg.get_bool("diff_fuzzy", true);
    cfg.get_optional("diff_error", error_tolerance);
    cfg.get_optional("diff_start", start_level);
    cfg.get_optional("diff_chunksize", chunk_size);
    cfg.get_optional("diff_dtype", data_type);
    INFO("Reproducibility analysis active: " << active);
}

static void
load_data(const std::string &filename, std::vector<uint8_t> &buffer,
          size_t data_size) {
    int fd = open(filename.c_str(), O_RDONLY, 0644);
    if (fd == -1) {
        FATAL("cannot open " << filename << ", error = " << strerror(errno));
    }
    size_t transferred = 0, remaining = data_size;
    while (remaining > 0) {
        auto ret = read(fd, buffer.data() + transferred, remaining);
        remaining -= ret;
        transferred += ret;
    }
    fsync(fd);
    close(fd);
}

int
get_file_size(const std::string &filename, off_t *size) {
    struct stat st;

    if (stat(filename.c_str(), &st) < 0)
        return -1;
    if (S_ISREG(st.st_mode)) {
        *size = st.st_size;
        return 0;
    }
    return -1;
}

int
statediff_module_t::process_command(const command_t &c) {
    if (!active) {
        INFO("Statediff is inactive");
        return VELOC_IGNORED;
    }

    switch (c.command) {
    case command_t::CHECKPOINT: {
        std::string reference = c.prev_filename(cfg.get("persistent")),
                    local = c.filename(cfg.get("scratch"));
        io_uring_stream_t<float> reader_prev(reference,
                                             chunk_size / sizeof(float));
        io_uring_stream_t<float> reader_curr(local, chunk_size / sizeof(float));
        // To-Do:
        // 1- Make sure we properly handle floating-point and integer data
        // 2- Stream data to the GPU for hashing
        INFO("Reference file " << reference);
        INFO("Current file " << local);
        off_t filesize;
        std::vector<uint8_t> data_prev, data_curr;
        get_file_size(local, &filesize);
        size_t data_size = static_cast<size_t>(filesize);
        // read_file(reference, unsigned char *buffer, ssize_t size)
        // read_file(const std::string &source, unsigned char *buffer, ssize_t size)
        load_data(reference, data_prev, data_size);
        load_data(local, data_curr, data_size);

        state_diff::client_t<float, io_uring_stream_t> client_prev(
            0, reader_prev, data_size, error_tolerance, data_type[0],
            chunk_size, start_level, fuzzy_hash);
        state_diff::client_t<float, io_uring_stream_t> client_curr(
            1, reader_curr, data_size, error_tolerance, data_type[0],
            chunk_size, start_level, fuzzy_hash);
        client_prev.create(data_prev.data());
        client_curr.create(data_curr.data());
        client_curr.compare_with(client_prev);
        INFO("Statediff reproducibility analysis completed with "
             << client_curr.get_num_changes() << " changes.");
        return VELOC_SUCCESS;
    }
    case command_t::RESTART:
        return VELOC_IGNORED;

    default:
        return VELOC_IGNORED;
    }
}
