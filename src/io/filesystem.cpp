#include <io/filesystem.h>
#include <misc/debug.h>
#include <fstream>
#include <unistd.h>
using namespace IO;

std::vector<char> FileSystem::ReadFile(const std::string &path) {
    debug("File requested: " + path);
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
        throw fs_error("File could not be opened");
    std::vector<char> fileContents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    return fileContents;
}

std::string FileSystem::GetCurrentDirectory() {
    char *cwd = NULL;
    cwd = getcwd(0, 0);
    std::string cur_dir(cwd);
    free(cwd);

    return cur_dir;
}
