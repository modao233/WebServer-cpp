#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include "../buffer/buffer.h"
#include "../log/log.h"

#include <string>
#include <unordered_map>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string &src_dir, std::string &path, 
            bool is_keep_alive=false, int code = -1);
    void MakeResponse(Buffer &buff);
    void UnmapFile();
    char *File();
    size_t FileLen() const;
    void ErrorContent(Buffer &buff, std::string message);
    int Code() const {
        return code_;
    }
private:
    void AddStateLine(Buffer &buff);
    void AddHeader(Buffer &buff);
    void AddContent(Buffer &buff);

    void ErrorHtml();
    std::string GetFileType();

    int code_;
    bool is_keep_alive_;

    std::string path_;
    std::string src_dir_;

    char *mm_file_;
    struct stat mm_file_stat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
};
#endif