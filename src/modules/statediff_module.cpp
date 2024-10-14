#include "statediff_module.hpp"

statediff_module_t::statediff_module_t(const config_t &c) : cfg(c) {
    active = cfg.get_bool("statediff", false);
    if (active && !check_dir(cfg.get("reference"))) {
        ERROR("Reference directory "
              << cfg.get("reference")
              << " inaccessible. Statediff deactivated!");
        active = false;
    }
    fuzzy_hash = cfg.get_bool("diff_fuzzy", true);
    cfg.get_optional("diff_error", error_tolerance);
    cfg.get_optional("diff_start", start_level);
    cfg.get_optional("diff_chunksize", chunk_size);
    cfg.get_optional("diff_dtype", data_type);
    INFO("Reproducibility analysis active: " << active);
    if (!Kokkos::is_initialized()) {
        Kokkos::initialize();
        DBG("Kokkos Initialized");
    }
}

statediff_module_t::~statediff_module_t() {
    delete local_reader;
    delete local_client;
    delete prev_reader;
    delete prev_client;
    if (!Kokkos::is_finalized()) {
        Kokkos::finalize();
        DBG("Kokkos Finalized");
    }
}

size_t
read_chkpt(const std::string &filename, std::vector<uint8_t> &buffer) {
    std::ifstream basefile;
    basefile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    basefile.open(filename, std::ifstream::in | std::ifstream::binary);

    int id;
    size_t num_regions, region_size, expected_size = 0;
    std::map<int, size_t> region_info;

    basefile.read(reinterpret_cast<char *>(&num_regions), sizeof(size_t));
    for (uint32_t i = 0; i < num_regions; i++) {
        basefile.read(reinterpret_cast<char *>(&id), sizeof(int));
        basefile.read(reinterpret_cast<char *>(&region_size), sizeof(size_t));
        expected_size += region_size;
    }
    size_t header_size = basefile.tellg();
    basefile.seekg(0, basefile.end);
    size_t file_size = static_cast<size_t>(basefile.tellg()) - header_size;
    if (file_size != expected_size) {
        std::cerr << "File size " << file_size
                  << " does not match expected size " << expected_size
                  << std::endl;
    }
    buffer.resize(expected_size);
    basefile.seekg(header_size);
    basefile.read(reinterpret_cast<char *>(buffer.data()), expected_size);
    basefile.close();
    return expected_size;
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

    switch (c.command) {
    case command_t::CHECKPOINT: {
        std::string current_file = cfg.get("scratch");
        std::string local = c.filename(current_file);
        local_reader =
            new io_uring_stream_t<float>(local, chunk_size / sizeof(float));
        std::vector<uint8_t> local_data;
        TIMER_START(local_loader);
        size_t data_size = read_chkpt(local, local_data);
        TIMER_STOP(local_loader, "loaded " << local << " to host memory");

        // Initialize client, create tree for checkpoint
        // INFO("Statediff: Processing file " << local);
        local_client = new state_diff::client_t<float, io_uring_stream_t>(
            0, *local_reader, data_size, error_tolerance, data_type[0],
            chunk_size, start_level, fuzzy_hash);

        TIMER_START(tree_creation);
        local_client->create(local_data.data());
        TIMER_STOP(tree_creation, "metadata created");

        // Serialize tree for checkpoint
        std::string local_meta = c.state_filename(cfg.get("persistent"));
        // INFO("Statediff: Serializing metadata to " << local_meta);
        TIMER_START(tree_ser);
        std::ofstream ofs(local_meta, std::ios::binary);
        cereal::BinaryOutputArchive oa(ofs);
        oa(*local_client);
        ofs.close();
        TIMER_STOP(tree_ser, "metadata serialized to " << local_meta);

        // If statediff params is set, proceed to comparison
        if (active) {
            // Initialize client for previous run
            std::string prev_file = cfg.get("reference");
            std::string prev_data = c.filename(prev_file);
            std::string prev_meta = c.state_filename(prev_file);

            INFO("Statediff: Comparing " << local << " with " << prev_data);
            prev_reader = new io_uring_stream_t<float>(
                prev_data, chunk_size / sizeof(float));
            prev_client = new state_diff::client_t<float, io_uring_stream_t>(
                1, *prev_reader, data_size, error_tolerance, data_type[0],
                chunk_size, start_level, fuzzy_hash);

            // Deserialize tree of previous run
            TIMER_START(tree_deser);
            std::ifstream ifs(prev_meta, std::ios::binary);
            cereal::BinaryInputArchive ia(ifs);
            ia(*prev_client);
            ifs.close();
            TIMER_STOP(tree_deser, "metadata deserialized from " << prev_meta);

            // Compare both runs checkpoints
            INFO("Statediff: Info (" << local_client->get_client_info()
                                     << ") VS ("
                                     << prev_client->get_client_info() << ")");
            TIMER_START(compare);
            local_client->compare_with(*prev_client);
            TIMER_STOP(compare, "checkpoint comparison completed");
            INFO("Statediff: Reproducibility analysis completed with "
                 << local_client->get_num_changes() << " changes.");
        }
        return VELOC_SUCCESS;
    }
    case command_t::RESTART:
        return VELOC_IGNORED;

    default:
        return VELOC_IGNORED;
    }
}
