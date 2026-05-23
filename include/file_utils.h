#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

struct FileChunk {
    int index;           // chunk number (0, 1, 2...)
    std::vector<char> data;  // chunk data
};

// Split a file into N chunks
std::vector<FileChunk> split_file(const std::string& filepath, int num_chunks) {
    std::ifstream infile(filepath, std::ios::binary);
    if (!infile) {
        std::cerr << "Error opening file: " << filepath << "\n";
        return {};
    }

    // Get file size
    infile.seekg(0, std::ios::end);
    long file_size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    long chunk_size = (file_size + num_chunks - 1) / num_chunks; // ceiling division

    std::vector<FileChunk> chunks;
    for (int i = 0; i < num_chunks; i++) {
        FileChunk chunk;
        chunk.index = i;

        long remaining = file_size - (i * chunk_size);
        long this_chunk_size = std::min(chunk_size, remaining);
        if (this_chunk_size <= 0) break;

        chunk.data.resize(this_chunk_size);
        infile.read(chunk.data.data(), this_chunk_size);
        chunks.push_back(chunk);
    }

    infile.close();
    return chunks;
}

// Reassemble chunks into a file
void reassemble_file(const std::string& output_path, 
                     std::vector<FileChunk>& chunks) {
    // Sort by index to ensure correct order
    std::sort(chunks.begin(), chunks.end(), 
              [](const FileChunk& a, const FileChunk& b) {
                  return a.index < b.index;
              });

    std::ofstream outfile(output_path, std::ios::binary);
    for (auto& chunk : chunks) {
        outfile.write(chunk.data.data(), chunk.data.size());
    }
    outfile.close();
    std::cout << "File reassembled at: " << output_path << "\n";
}