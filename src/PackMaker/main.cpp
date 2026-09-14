#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <limits>
#include <string_view>
#include <type_traits>

#include <zstd.h>
#include <argparse.hpp>
#include <sodium.h>

#include "PackLib/config.h"

template <typename Target, typename Source>
static bool CheckedUnsignedCast(Source value, Target& result)
{
	static_assert(std::is_unsigned_v<Target> && std::is_unsigned_v<Source>);
	if constexpr ((std::numeric_limits<Source>::max)() > (std::numeric_limits<Target>::max)()) {
		if (value > static_cast<Source>((std::numeric_limits<Target>::max)()))
			return false;
	}

	result = static_cast<Target>(value);
	return true;
}

static void EncryptData(uint8_t* data, size_t len, const uint8_t* nonce)
{
	crypto_stream_xchacha20_xor(data, data, len, nonce, PACK_KEY.data());
}

int main(int argc, char* argv[])
{
	std::setlocale(LC_ALL, "en_US.UTF-8");

	if (sodium_init() < 0) {
		std::cerr << "Failed to initialize libsodium" << std::endl;
		return EXIT_FAILURE;
	}

	argparse::ArgumentParser program("PackMaker");

	program.add_argument("--input")
		.required()
		.help("Input folder to pack");

	program.add_argument("--output")
		.default_value("")
		.help("Output path to place newly created pack file");

	try {
		program.parse_args(argc, argv);
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << std::endl;
		std::cerr << program;
		std::exit(EXIT_FAILURE);
	}

	std::filesystem::path input = program.get<std::string>("--input"), output = program.get<std::string>("--output");

	// we just normalize it here, because if it has a trailing slash, filename() will be empty
	// otherwise it returns the last part of the path
	if (input.filename().empty()) {
		input = input.parent_path();
	}

	output /= input.filename().replace_extension(".pck");

	std::ofstream ofs(output, std::ios::binary);
	if (!ofs.is_open()) {
		std::cerr << "Failed to open output file: " << output << std::endl;
		return EXIT_FAILURE;
	}

	std::map<std::filesystem::path, TPackFileEntry> entries;

	for (auto entry : std::filesystem::recursive_directory_iterator(input)) {
		if (!entry.is_regular_file())
			continue;

		std::filesystem::path relative_path = std::filesystem::relative(entry.path(), input);

		TPackFileEntry& file_entry = entries[relative_path];
		std::memset(&file_entry, 0, sizeof(file_entry));
		const uintmax_t source_file_size = entry.file_size();
		if (!CheckedUnsignedCast(source_file_size, file_entry.file_size)) {
			std::cerr << "Input file is too large for the pack format: " << entry.path() << std::endl;
			return EXIT_FAILURE;
		}

		constexpr std::string_view ymir_work_prefix = "ymir work/";
		std::string rp_str = relative_path.generic_string();
		if (rp_str.compare(0, ymir_work_prefix.size(), ymir_work_prefix) == 0) {
			rp_str = (std::filesystem::path("d:/ymir work/") / rp_str.substr(ymir_work_prefix.size())).generic_string();
		}

		std::transform(rp_str.begin(), rp_str.end(), rp_str.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});

		if (rp_str.size() >= PACK_FILENAME_CAPACITY) {
			std::cerr << "Pack path exceeds " << (PACK_FILENAME_CAPACITY - 1)
				<< " bytes: " << rp_str << std::endl;
			return EXIT_FAILURE;
		}

		std::memcpy(file_entry.file_name, rp_str.data(), rp_str.size());
	}

	TPackFileHeader header;
	std::memset(&header, 0, sizeof(header));
	if (!CheckedUnsignedCast(entries.size(), header.entry_num)) {
		std::cerr << "Too many files for the pack format" << std::endl;
		return EXIT_FAILURE;
	}
	if (header.entry_num > ((std::numeric_limits<uint64_t>::max)() - sizeof(TPackFileHeader)) / sizeof(TPackFileEntry)) {
		std::cerr << "Pack index size exceeds the format limit" << std::endl;
		return EXIT_FAILURE;
	}
	header.data_begin = sizeof(TPackFileHeader) + sizeof(TPackFileEntry) * header.entry_num;
	if (header.data_begin > static_cast<uint64_t>((std::numeric_limits<std::streamoff>::max)())) {
		std::cerr << "Pack index exceeds the output stream limit" << std::endl;
		return EXIT_FAILURE;
	}

	randombytes_buf(header.nonce, sizeof(header.nonce));

	ofs.write(reinterpret_cast<const char*>(&header), static_cast<std::streamsize>(sizeof(header)));
	if (!ofs) {
		std::cerr << "Failed to write pack header" << std::endl;
		return EXIT_FAILURE;
	}
	ofs.seekp(static_cast<std::streamoff>(header.data_begin), std::ios::beg);
	if (!ofs) {
		std::cerr << "Failed to seek to pack data" << std::endl;
		return EXIT_FAILURE;
	}

	uint64_t offset = 0;
	for (auto& [path, entry] : entries) {
		std::ifstream ifs(input / path, std::ios::binary);
		if (!ifs.is_open()) {
			std::cerr << "Failed to open input file: " << (input / path) << std::endl;
			return EXIT_FAILURE;
		}

		size_t file_size = 0;
		if (!CheckedUnsignedCast(entry.file_size, file_size) ||
			entry.file_size > static_cast<uint64_t>((std::numeric_limits<std::streamsize>::max)())) {
			std::cerr << "Input file exceeds process or stream limits: " << (input / path) << std::endl;
			return EXIT_FAILURE;
		}

		static std::vector<char> buffer;
		if (file_size > buffer.max_size()) {
			std::cerr << "Input file exceeds buffer limits: " << (input / path) << std::endl;
			return EXIT_FAILURE;
		}
		buffer.resize(file_size);

		if (file_size != 0 && !ifs.read(buffer.data(), static_cast<std::streamsize>(file_size))) {
			std::cerr << "Failed to read input file: " << (input / path) << std::endl;
			return EXIT_FAILURE;
		}

		size_t compress_bound = ZSTD_compressBound(file_size);
		static std::vector<char> compressed_buffer;
		if (ZSTD_isError(compress_bound) || compress_bound > compressed_buffer.max_size()) {
			std::cerr << "Input file exceeds compression limits: " << (input / path) << std::endl;
			return EXIT_FAILURE;
		}
		compressed_buffer.resize(compress_bound);

		const char empty_input = 0;
		const void* const input_data = file_size == 0 ? static_cast<const void*>(&empty_input) : buffer.data();
		const size_t compressed_size = ZSTD_compress(compressed_buffer.data(), compress_bound, input_data, file_size, 17);
		if(ZSTD_isError(compressed_size)) {
			std::cerr << "Failed to compress input file: " << (input / path) << " error: " << ZSTD_getErrorName(compressed_size) << std::endl;
			return EXIT_FAILURE;
		}
		if (static_cast<uintmax_t>(compressed_size) > static_cast<uintmax_t>((std::numeric_limits<std::streamsize>::max)()) ||
			compressed_size > (std::numeric_limits<uint64_t>::max)() - offset ||
			offset > static_cast<uint64_t>((std::numeric_limits<std::streamoff>::max)()) - header.data_begin ||
			compressed_size > static_cast<uint64_t>((std::numeric_limits<std::streamoff>::max)()) - header.data_begin - offset) {
			std::cerr << "Compressed data exceeds pack or stream limits: " << (input / path) << std::endl;
			return EXIT_FAILURE;
		}

		entry.offset = offset;
		entry.compressed_size = static_cast<uint64_t>(compressed_size);

		entry.encryption = 0;
		if (path.has_extension() && path.extension() == ".py") {
			entry.encryption = 1;

			randombytes_buf(entry.nonce, sizeof(entry.nonce));
			EncryptData(reinterpret_cast<uint8_t*>(compressed_buffer.data()), compressed_size, entry.nonce);
		}

		ofs.write(compressed_buffer.data(), static_cast<std::streamsize>(compressed_size));
		if (!ofs) {
			std::cerr << "Failed to write compressed data: " << (input / path) << std::endl;
			return EXIT_FAILURE;
		}
		offset += entry.compressed_size;
	}

	ofs.seekp(sizeof(TPackFileHeader), std::ios::beg);
	if (!ofs) {
		std::cerr << "Failed to seek to pack index" << std::endl;
		return EXIT_FAILURE;
	}

	for (auto& [path, entry] : entries) {
		TPackFileEntry tmp = entry;
		EncryptData(reinterpret_cast<uint8_t*>(&tmp), sizeof(tmp), header.nonce);
		ofs.write(reinterpret_cast<const char*>(&tmp), static_cast<std::streamsize>(sizeof(tmp)));
		if (!ofs) {
			std::cerr << "Failed to write pack index" << std::endl;
			return EXIT_FAILURE;
		}
	}
	ofs.flush();
	if (!ofs) {
		std::cerr << "Failed to flush pack output" << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
