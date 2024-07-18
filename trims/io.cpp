#include "include/io.h"


namespace io {

std::string get_error_message(error code) {
    if (code == io::error::file_doesnt_exist)
        return "File doesnt exist";
    if (code == io::error::couldnt_open_file)
        return "Couldnt open file";
    if (code == error::couldnt_read_file)
        return "Failed to read contents of a file";
    if (code == error::couldnt_write_file)
        return "Failed to write to a file";
    if (code == error::couldnt_get_file_size)
        return "Failed to get size of a file";
    if (code == error::couldnt_navigate_file)
        return "Failed to navigate a file";
    return "Undocumented error";
}

std::string get_error_message(error code, const fs::path& file) {
    if (code == io::error::file_doesnt_exist)
        return "File " + file.string() + " doesnt exist";
    if (code == io::error::couldnt_open_file)
        return "Couldnt open file " + file.string();
    if (code == error::couldnt_read_file)
        return "Failed to read contents of " + file.string();
    if (code == error::couldnt_write_file)
        return "Failed to write to file " + file.string();
    if (code == error::couldnt_get_file_size)
        return "Failed to get size of " + file.string();
    if (code == error::couldnt_navigate_file)
        return "Failed to navigate, when reading file " + file.string();
    return "Undocumented error";
}

std::expected<std::ifstream, error> open_file_rb(const fs::path& file, bool ate) noexcept {
    if (!fs::exists(file))
        return std::unexpected(error::file_doesnt_exist);
    std::ifstream in(file, ate ? std::ios_base::binary | std::ios_base::ate : std::ios_base::binary);
    if (in.fail())
        return std::unexpected(error::couldnt_open_file);
    return std::move(in);
}

std::expected<void, io::error> return_to_begin(std::istream& stream) noexcept {
    return_to_begin_unsafe(stream);
    if (!stream.good())
        return std::unexpected(io::error::couldnt_navigate_file);
    return {};
}

std::expected<uint64_t, error> 
get_file_size(std::istream& stream, bool ate, std::function<void(std::istream&)> transform) noexcept {
    uint64_t filesize;
    if (!ate) stream.seekg(0, std::ios_base::end);
    filesize = stream.tellg();
    transform(stream);
    if (!stream.good())
        return std::unexpected(error::couldnt_get_file_size);
    return filesize;
}

std::vector<uint8_t> read_bytes_unsafe(std::istream& in, uint64_t filesize, size_t max_bytes) noexcept {
    size_t n = std::min<size_t>(filesize - in.tellg(), max_bytes);
    std::vector<uint8_t> bytes(n);
    in.read(reinterpret_cast<char*>(&bytes[0]), n);
    return bytes;
}

size_t read_bytes_unsafe(std::istream& in, uint64_t filesize, uint8_t* buf, size_t max_bytes) noexcept {
    size_t n = std::min<size_t>(filesize - in.tellg(), max_bytes);
    in.read(reinterpret_cast<char*>(buf), n);
    return n;
}

std::expected<std::vector<uint8_t>, error> 
read_bytes(std::istream& in, uint64_t filesize, size_t max_bytes) noexcept {
    auto read = read_bytes_unsafe(in, filesize, max_bytes);
    if (!in.good())
        return std::unexpected(error::couldnt_read_file);
    return std::move(read);
}

std::expected<size_t, error> 
read_bytes(std::istream& in, uint64_t filesize, uint8_t* buf, size_t max_bytes) noexcept {
    auto read = read_bytes_unsafe(in, filesize, buf, max_bytes);
    if (!in.good())
        return std::unexpected(error::couldnt_read_file);
    return read;
}


std::expected<std::ofstream, error> open_file_wb(const fs::path& file) noexcept {
    std::ofstream out(file, std::ios_base::binary | std::ios_base::trunc);
    if (out.fail())
        return std::unexpected(error::couldnt_open_file);
    return std::move(out);
}

size_t write_bytes_unsafe(std::ostream& out, const uint8_t* buf, size_t n_bytes) noexcept {
    out.write(reinterpret_cast<const char*>(buf), n_bytes);
    return n_bytes;
}

size_t write_bytes_unsafe(std::ostream& out, std::vector<uint8_t>& buf) noexcept {
    size_t n;
    out.write(reinterpret_cast<char*>(&buf[0]), n = buf.size());
    return buf.clear(), n;
}

size_t write_bytes_unsafe(std::ostream& out, std::vector<uint8_t>&& buf) noexcept {
    size_t n;
    out.write(reinterpret_cast<const char*>(&buf[0]), n = buf.size());
    return n;
}

std::expected<size_t, error> write_bytes(std::ostream& out, const uint8_t* buf, size_t n_bytes) noexcept {
    auto write = write_bytes_unsafe(out, buf, n_bytes);
    if (!out.good())
        return std::unexpected(error::couldnt_write_file);
    return write;
}

std::expected<size_t, error> write_bytes(std::ostream& out, std::vector<uint8_t>& buf) noexcept {
    auto write = write_bytes_unsafe(out, buf);
    if (!out.good())
        return std::unexpected(error::couldnt_write_file);
    return write;
}

std::expected<size_t, error> write_bytes(std::ostream& out, std::vector<uint8_t>&& buf) noexcept {
    auto write = write_bytes_unsafe(out, std::move(buf));
    if (!out.good())
        return std::unexpected(error::couldnt_write_file);
    return write;
}

}
