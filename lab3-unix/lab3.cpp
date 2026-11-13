#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;

size_t file_hash(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return std::hash<std::string>{}(ss.str());
}

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;

    std::vector<std::string> files;
    for (const auto& e : fs::recursive_directory_iterator(argv[1]))
        if (e.is_regular_file() && !e.is_symlink())
            files.push_back(e.path().string());

    std::map<size_t, std::string> kept;
    for (const auto& path : files) {
        size_t h = file_hash(path);
        auto it = kept.find(h);
        if (it == kept.end()) {
            kept[h] = path;
            continue;
        }
        unlink(path.c_str());
        link(it->second.c_str(), path.c_str());
    }
}
