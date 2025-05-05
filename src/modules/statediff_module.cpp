#include "statediff_module.hpp"

statediff_module_t::statediff_module_t(const config_t &c) : cfg(c) {
    compare = cfg.get_bool("statediff", false);
    debug_log.open("statediff.out", std::ios::out | std::ios::app);
    if (compare && !check_dir(cfg.get("reference"))) {
        ERROR("Reference directory "
              << cfg.get("reference")
              << " inaccessible. Statediff deactivated!");
        compare = false;
        debug_log << "[ERROR] Unable to verify that reference file exists"
                  << std::endl;
    }
    fuzzy_hash = cfg.get_bool("diff_fuzzy", true);
    cfg.get_optional("diff_error", error_tolerance);
    cfg.get_optional("diff_start", start_level);
    cfg.get_optional("diff_chunksize", chunk_size);
    cfg.get_optional("diff_dtype", data_type);
    debug_log << "[INFO] Reproducibility comparison active: " << compare
              << std::endl;
    if (!Kokkos::is_initialized()) {
        uint32_t num_threads = std::thread::hardware_concurrency();
        Kokkos::initialize(
            Kokkos::InitializationSettings().set_num_threads(num_threads));
        DBG("Kokkos Initialized");
    }
}

statediff_module_t::~statediff_module_t() {
    if (!Kokkos::is_finalized()) {
        Kokkos::finalize();
        DBG("Kokkos Finalized");
    }
}

int
statediff_module_t::process_command(const command_t &c) {
    if (c.command == command_t::CHECKPOINT) {
        std::string scratch_dir = cfg.get("scratch");
        std::string current_ckpt = c.filename(scratch_dir);
        std::string prev_ckpt = "";
        size_t diff_count = 0;

        std::map<int, size_t> region_info;
        size_t data_size = static_cast<size_t>(
            file_size(current_ckpt) - read_header(current_ckpt,
            region_info));
        local_client.initialize(0, data_size, error_tolerance, data_type[0],
                                chunk_size, start_level, fuzzy_hash);
        liburing_io_reader_t local_reader(current_ckpt);
        local_client.create(local_reader);
        {
            std::string local_meta = c.state_filename(cfg.get("persistent"));
            std::ofstream ofs(local_meta, std::ios::binary);
            cereal::BinaryOutputArchive oa(ofs);
            oa(local_client);
            ofs.close();
        }
        debug_log << "[INFO] Current client tree created!" << std::endl;

        if (compare) {
            std::string reference_dir = cfg.get("reference");
            prev_ckpt = c.filename(reference_dir);
            debug_log << "[INFO] Comparing " << current_ckpt << " with "
                      << prev_ckpt << std::endl;
            {
                std::string prev_meta = c.state_filename(reference_dir);
                std::ifstream ifs(prev_meta, std::ios::binary);
                cereal::BinaryInputArchive ia(ifs);
                ia(prev_client);
                ifs.close();
            }
            liburing_io_reader_t prev_reader(prev_ckpt);
            debug_log << "[INFO] Ready for comparisons!" << std::endl;
            local_client.compare_with(0, local_reader, prev_client,
                                      prev_reader);
            diff_count = local_client.get_num_changes();
            debug_log << "[INFO] Reproducibility analysis completed with "
                      << diff_count << " changes." << std::endl;
        }
        // Write logs
        std::string csv_filename = "diff_log.csv";
        bool file_exists = std::filesystem::exists(csv_filename);
        std::ofstream csv_out(csv_filename, std::ios::app);
        if (csv_out.is_open()) {
            if (!file_exists) {
                csv_out << "prev_ckpt,current_ckpt,num_changes\n";
            }
            csv_out << prev_ckpt << "," << current_ckpt << "," << diff_count
                    << "\n";
            csv_out.close();
        } else {
            debug_log << "[ERROR] Failed to open diff_log.csv for writing."
                      << std::endl;
        }
        return VELOC_SUCCESS;
    } else {
        return VELOC_IGNORED;
    }
}

// int
// statediff_module_t::process_command(const command_t &c) {
//     if (c.command == command_t::CHECKPOINT) {
//             std::string scratch_dir = cfg.get("scratch");
//             std::string current_ckpt = c.filename(scratch_dir);
//             std::string prev_ckpt = "";
//             size_t diff_count = 0;

//             std::map<int, size_t> region_info;
//             size_t data_size = static_cast<size_t>(
//                 file_size(current_ckpt) - read_header(current_ckpt, region_info));
//             local_client->initialize(0, data_size, error_tolerance, data_type[0],
//                                     chunk_size, start_level, fuzzy_hash);
//             liburing_io_reader_t local_reader_(current_ckpt);
//             local_client->create(local_reader_);
//             {
//                 std::string local_meta = c.state_filename(cfg.get("persistent"));
//                 std::ofstream ofs(local_meta, std::ios::binary);
//                 cereal::BinaryOutputArchive oa(ofs);
//                 oa(*local_client);
//                 ofs.close();
//             }
//             debug_log << "[INFO] Current client tree created!" << std::endl;

//             if (compare) {
//                 std::string reference_dir = cfg.get("reference");
//                 prev_ckpt = c.filename(reference_dir);
//                 debug_log << "[INFO] Comparing " << current_ckpt << " with "
//                         << prev_ckpt << std::endl;
//                 {
//                     std::string prev_meta = c.state_filename(reference_dir);
//                     std::ifstream ifs(prev_meta, std::ios::binary);
//                     cereal::BinaryInputArchive ia(ifs);
//                     ia(*prev_client);
//                     ifs.close();
//                 }
//                 liburing_io_reader_t prev_reader_(prev_ckpt);
//                 debug_log << "[INFO] Ready for comparisons!" << std::endl;
//                 local_client->compare_with(0, local_reader_, *prev_client,
//                                         prev_reader_);
//                 diff_count = local_client->get_num_changes();
//                 debug_log << "[INFO] Reproducibility analysis completed with "
//                         << diff_count << " changes." << std::endl;
//             }
//             // Write logs
//             std::string csv_filename = "diff_log.csv";
//             bool file_exists = std::filesystem::exists(csv_filename);
//             std::ofstream csv_out(csv_filename, std::ios::app);
//             if (csv_out.is_open()) {
//                 if (!file_exists) {
//                     csv_out << "prev_ckpt,current_ckpt,num_changes\n";
//                 }
//                 csv_out << prev_ckpt << "," << current_ckpt << "," << diff_count
//                         << "\n";
//                 csv_out.close();
//             } else {
//                 debug_log << "[ERROR] Failed to open diff_log.csv for writing."
//                         << std::endl;
//             }
//             return VELOC_SUCCESS;
//         }
//         else {
//             return VELOC_IGNORED;
//         }
// }
