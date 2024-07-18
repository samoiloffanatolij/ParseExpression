#pragma once

#include <fstream>
#include <expected>
#include <filesystem>
#include <vector>
#include <functional>


namespace io {

namespace fs = std::filesystem;

enum class error {
    file_doesnt_exist, couldnt_open_file,
    couldnt_read_file, couldnt_write_file,
    couldnt_get_file_size, couldnt_navigate_file,
};

std::string get_error_message(error code);
std::string get_error_message(error code, const fs::path& file);

std::expected<std::ifstream, error> open_file_rb(const fs::path& file, bool ate = false) noexcept;

inline void return_to_begin_unsafe(std::istream& stream) noexcept {
    stream.seekg(0);
}

std::expected<void, io::error> return_to_begin(std::istream& stream) noexcept;

std::expected<uint64_t, error> 
get_file_size(std::istream& stream, bool ate = false, 
              std::function<void(std::istream&)> transform = return_to_begin_unsafe) noexcept;

std::vector<uint8_t> read_bytes_unsafe(std::istream& in, uint64_t filesize, size_t max_bytes = 1024) noexcept;

size_t read_bytes_unsafe(std::istream& in, uint64_t filesize, uint8_t* buf, size_t max_bytes = 1024) noexcept;

std::expected<std::vector<uint8_t>, error> 
read_bytes(std::istream& in, uint64_t filesize, size_t max_bytes = 1024) noexcept;

std::expected<size_t, error> 
read_bytes(std::istream& in, uint64_t filesize, uint8_t* buf, size_t max_bytes = 1024) noexcept;


std::expected<std::ofstream, error> open_file_wb(const fs::path& file) noexcept;

size_t write_bytes_unsafe(std::ostream& out, const uint8_t* buf, size_t n_bytes) noexcept;

size_t write_bytes_unsafe(std::ostream& out, std::vector<uint8_t>& buf) noexcept;

size_t write_bytes_unsafe(std::ostream& out, std::vector<uint8_t>&& buf) noexcept;

std::expected<size_t, error> write_bytes(std::ostream& out, const uint8_t* buf, size_t n_bytes) noexcept;

std::expected<size_t, error> write_bytes(std::ostream& out, std::vector<uint8_t>& buf) noexcept;

std::expected<size_t, error> write_bytes(std::ostream& out, std::vector<uint8_t>&& buf) noexcept;

}
