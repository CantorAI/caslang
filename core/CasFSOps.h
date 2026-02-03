#pragma once
#include "CasOps.h"
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

#if (WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace CasLang {

#include <stdexcept>

// Helper function: UTF-8 to UTF-16
inline std::wstring UTF8ToWString(const std::string& utf8) {
#if (WIN32)
    if (utf8.empty()) return std::wstring();
    int wstrLength = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wstrLength <= 0) return std::wstring();
    std::wstring wstr(wstrLength - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], wstrLength);
    return wstr;
#else
    return std::wstring(utf8.begin(), utf8.end());
#endif
}

// Helper function to open input stream with Unicode support
inline std::ifstream open_input_stream(const std::string& path, std::ios::openmode mode = std::ios::in) {
#if (WIN32)
    std::wstring widePath = UTF8ToWString(path);
    return std::ifstream(widePath.c_str(), mode);
#else
    return std::ifstream(path.c_str(), mode);
#endif
}

// Helper function to open output stream with Unicode support
inline std::ofstream open_output_stream(const std::string& path, std::ios::openmode mode = std::ios::out) {
#if (WIN32)
    std::wstring widePath = UTF8ToWString(path);
    return std::ofstream(widePath.c_str(), mode);
#else
    return std::ofstream(path.c_str(), mode);
#endif
}

// Helper function to create filesystem path with Unicode support
inline fs::path create_fs_path(const std::string& path) {
#if (WIN32)
    std::wstring widePath = UTF8ToWString(path);
    return fs::path(widePath);
#else
    return fs::path(path);
#endif
}

// Simple wildcard match (* and ?)
inline bool fs_matches_pattern(const std::string& name, const std::string& pattern) {
    size_t n = name.size(), p = pattern.size();
    size_t i = 0, j = 0, star = std::string::npos, match = 0;
    while (i < n) {
        if (j < p && (pattern[j] == '?' || pattern[j] == name[i])) { ++i; ++j; }
        else if (j < p && pattern[j] == '*') { star = j++; match = i; }
        else if (star != std::string::npos) { j = star + 1; i = ++match; }
        else return false;
    }
    while (j < p && pattern[j] == '*') ++j;
    return j == p;
}

inline std::string json_escape(const std::string& s) {
    std::string o; o.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '\\': o += "\\\\"; break;
        case '"':  o += "\\\""; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:   o.push_back(c); break;
        }
    }
    return o;
}

inline std::string to_json_array(const std::vector<std::string>& v) {
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ',';
        out += '"';
        out += json_escape(v[i]);
        out += '"';
    }
    out += ']';
    return out;
}

inline std::string to_json_stat(const std::string& pathStr, std::error_code& ec) {
    fs::path p = create_fs_path(pathStr);
    bool exists = fs::exists(p, ec);
    if (ec) return "{}";

    bool is_dir = false, is_file = false;
    uintmax_t size = 0;
    if (exists) {
        is_dir = fs::is_directory(p, ec);
        is_file = fs::is_regular_file(p, ec);
        if (is_file) size = fs::file_size(p, ec);
    }

    std::string out = "{";
    out += "\"path\":\"" + json_escape(pathStr) + "\",";
    out += std::string("\"exists\":") + (exists ? "true" : "false") + ",";
    out += std::string("\"is_dir\":") + (is_dir ? "true" : "false") + ",";
    out += std::string("\"is_file\":") + (is_file ? "true" : "false") + ",";
    out += "\"size\":" + std::to_string(size);
    out += "}";
    return out;
}

class CasFSOps : public CasOps {
public:
    const std::string& Namespace() const override {
        static std::string k = "fs";
        return k;
    }

    // Execute stateless, large-grain FS ops (no handles)
    X::Value Execute(const std::vector<std::string>& ns_parts,
        const std::string& command,
        std::unordered_map<std::string, X::Value>& args,
        CasContext& ctx,
        std::vector<std::string>& errs) override
    {
        (void)ns_parts;

        auto S = [&](const char* k, const std::string& def = "")->std::string {
            auto it = args.find(k);
            return (it != args.end() && it->second.isString()) ? it->second.asString() : def;
            };
        auto B = [&](const char* k, bool def = false)->bool {
            auto it = args.find(k);
            return (it != args.end() && it->second.isBool()) ? it->second.asBool() : def;
            };
        auto I64 = [&](const char* k, int64_t def = 0)->int64_t {
            auto it = args.find(k);
            return (it != args.end() && it->second.isNumber()) ? static_cast<int64_t>(it->second.asNumber()) : def;
            };

        // Common control keys (already handled by runner for as/return)
        (void)ctx;

        if (command == "read_file") {
            std::string path = S("path");
            if (path.empty()) { errs.push_back("fs.read_file: missing 'path'"); return X::Value(); }
            int64_t max_bytes = I64("max_bytes", -1);
            int64_t offset = I64("offset", 0);

            // Use Unicode-aware file opening
            std::ifstream f = open_input_stream(path, std::ios::binary);
            if (!f) { errs.push_back("fs.read_file: cannot open: " + path); return X::Value(); }
            if (offset > 0) f.seekg(offset, std::ios::beg);

            std::string data;
            if (max_bytes >= 0) {
                data.resize((size_t)max_bytes);
                f.read(&data[0], (std::streamsize)max_bytes);
                data.resize((size_t)f.gcount());
            }
            else {
                f.seekg(0, std::ios::end);
                std::streampos end = f.tellg();
                std::streampos start = offset > 0 ? std::streampos(offset) : std::streampos(0);
                if (end < start) { errs.push_back("fs.read_file: offset beyond EOF"); return X::Value(); }
                std::streamsize len = end - start;
                data.resize((size_t)len);
                f.seekg(start, std::ios::beg);
                f.read(&data[0], len);
            }
            return X::Value(data);
        }

        if (command == "write_file") {
            std::string path = S("path");
            std::string data = S("data");
            bool append = B("append", false);
            if (path.empty()) { errs.push_back("fs.write_file: missing 'path'"); return X::Value(); }

            std::ios::openmode flags = std::ios::binary | std::ios::out | (append ? std::ios::app : std::ios::trunc);
            // Use Unicode-aware file opening
            std::ofstream f = open_output_stream(path, flags);
            if (!f) { errs.push_back("fs.write_file: cannot open: " + path); return X::Value(); }
            f.write(data.data(), (std::streamsize)data.size());
            if (!f) { errs.push_back("fs.write_file: write failed"); return X::Value(); }
            return X::Value((int64_t)data.size());
        }

        if (command == "list") {
            std::string dir = S("dir");
            std::string pattern = S("pattern", "*");
            bool recursive = B("recursive", false);
            bool include_dirs = B("include_dirs", false);

            if (dir.empty()) { errs.push_back("fs.list: missing 'dir'"); return X::Value("[]"); }

            std::error_code ec;
            std::vector<std::string> out;

            // Use Unicode-aware path creation
            fs::path dirPath = create_fs_path(dir);

            if (!recursive) {
                if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) {
                    errs.push_back("fs.list: not a directory: " + dir);
                    return X::Value("[]");
                }
                for (auto& e : fs::directory_iterator(dirPath, ec)) {
                    if (ec) break;
                    auto name = e.path().filename().u8string(); // Use u8string() for UTF-8 output
                    bool isdir = fs::is_directory(e.path(), ec);
                    if (!include_dirs && isdir) continue;
                    if (fs_matches_pattern(name, pattern)) {
                        out.push_back((dirPath / name).u8string());  // full path
                    }
                }
            }
            else {
                for (auto& e : fs::recursive_directory_iterator(dirPath, ec)) {
                    if (ec) break;
                    auto rel = fs::relative(e.path(), dirPath, ec).u8string(); // Use u8string() for UTF-8 output
                    bool isdir = fs::is_directory(e.path(), ec);
                    if (!include_dirs && isdir) continue;
                    if (fs_matches_pattern(rel, pattern)) {
                        out.push_back(e.path().u8string());  // full absolute path
                    }
                }
            }
            return X::Value(to_json_array(out));
        }

        if (command == "delete") {
            std::string path = S("path");
            bool recursive = B("recursive", false);
            if (path.empty()) { errs.push_back("fs.delete: missing 'path'"); return X::Value(); }
            std::error_code ec;

            // Use Unicode-aware path creation
            fs::path fsPath = create_fs_path(path);
            if (recursive) fs::remove_all(fsPath, ec); else fs::remove(fsPath, ec);
            if (ec) { errs.push_back("fs.delete: " + ec.message()); return X::Value(); }
            return X::Value(true);
        }

        if (command == "copy") {
            std::string src = S("src");
            std::string dst = S("dst");
            bool overwrite = B("overwrite", false);
            bool recursive = B("recursive", true);
            if (src.empty() || dst.empty()) { errs.push_back("fs.copy: missing 'src' or 'dst'"); return X::Value(); }

            std::error_code ec;
            auto opts = fs::copy_options::none;
            if (recursive) opts |= fs::copy_options::recursive;
            opts |= overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::skip_existing;

            // Use Unicode-aware path creation
            fs::path srcPath = create_fs_path(src);
            fs::path dstPath = create_fs_path(dst);
            fs::copy(srcPath, dstPath, opts, ec);
            if (ec) { errs.push_back("fs.copy: " + ec.message()); return X::Value(); }
            return X::Value(true);
        }

        if (command == "move") {
            std::string src = S("src");
            std::string dst = S("dst");
            bool overwrite = B("overwrite", false);
            if (src.empty() || dst.empty()) { errs.push_back("fs.move: missing 'src' or 'dst'"); return X::Value(); }
            std::error_code ec;

            // Use Unicode-aware path creation
            fs::path srcPath = create_fs_path(src);
            fs::path dstPath = create_fs_path(dst);

            if (overwrite && fs::exists(dstPath, ec)) { fs::remove_all(dstPath, ec); }
            fs::rename(srcPath, dstPath, ec);
            if (ec) { errs.push_back("fs.move: " + ec.message()); return X::Value(); }
            return X::Value(true);
        }

        if (command == "mkdir") {
            std::string path = S("path");
            bool recursive = B("recursive", true);
            if (path.empty()) { errs.push_back("fs.mkdir: missing 'path'"); return X::Value(); }
            std::error_code ec;

            // Use Unicode-aware path creation
            fs::path fsPath = create_fs_path(path);
            bool ok = recursive ? fs::create_directories(fsPath, ec) : fs::create_directory(fsPath, ec);
            if (ec) { errs.push_back("fs.mkdir: " + ec.message()); return X::Value(); }
            (void)ok;
            return X::Value(true);
        }

        if (command == "stat") {
            std::string path = S("path");
            if (path.empty()) { errs.push_back("fs.stat: missing 'path'"); return X::Value("{}"); }
            std::error_code ec;
            return X::Value(to_json_stat(path, ec));
        }

        if (command == "exists") {
            std::string path = S("path");
            if (path.empty()) { errs.push_back("fs.exists: missing 'path'"); return X::Value(); }
            std::error_code ec;

            // Use Unicode-aware path creation
            fs::path fsPath = create_fs_path(path);
            bool ex = fs::exists(fsPath, ec);
            if (ec) { errs.push_back("fs.exists: " + ec.message()); return X::Value(); }
            return X::Value(ex);
        }

        errs.push_back("fs: unknown command: " + command);
        return X::Value();
    }

    // Command catalog for LLM/system prompts
    std::vector<CommandInfo> DescribeCommands() const override {
        using A = CommandInfo::Arg;
        std::vector<CommandInfo> v;

        v.push_back(CommandInfo{
            "read_file",
            "Read a file into a string (optionally a slice via offset/max_bytes).",
            {
                A{"path","string",true, "Absolute or relative path.",""},
                A{"max_bytes","number",false,"Read at most this many bytes; -1 means to EOF.","-1"},
                A{"offset","number",false,"Byte offset where reading starts.","0"},
                A{"as","string",false,"Capture output to a variable.",""},
                A{"return","bool",false,"If true, this command's output becomes the filter result.","false"},
            },
            "string (raw bytes).",
            {
                "#fs.read_file{\"path\":\"${root}/readme.txt\",\"as\":\"txt\"}",
                "#fs.read_file{\"path\":\"${root}/video.mp4\",\"offset\":0,\"max_bytes\":4096,\"return\":true}"
            }
            });

        v.push_back(CommandInfo{
            "write_file",
            "Write a string to a file (binary-safe).",
            {
                A{"path","string",true, "Destination file path.",""},
                A{"data","string",true, "Content to write.",""},
                A{"append","bool",false, "Append instead of overwrite.","false"},
                A{"as","string",false,"Capture bytes-written to a var.",""},
                A{"return","bool",false,"Mark as final output.","false"}
            },
            "number (bytes written).",
            {
                "#fs.write_file{\"path\":\"${root}/out.txt\",\"data\":\"hello\\n\"}",
                "#fs.write_file{\"path\":\"${root}/log.txt\",\"data\":\"line\",\"append\":true,\"as\":\"bytes\"}"
            }
            });

        v.push_back(CommandInfo{
            "list",
            "List directory entries; supports wildcard pattern and recursion.",
            {
                A{"dir","string",true,"Directory to list.",""},
                A{"pattern","string",false,"Glob pattern (* and ?).","\"*\""},
                A{"recursive","bool",false,"Recurse into subdirectories.","false"},
                A{"include_dirs","bool",false,"Include directories in results.","false"},
                A{"as","string",false,"Capture JSON array string.",""},
                A{"return","bool",false,"Mark as final output.","false"}
            },
            "string (JSON array of relative paths).",
            {
                "#fs.list{\"dir\":\"${root}\",\"pattern\":\"*.mp4\",\"as\":\"files\"}",
                "#fs.list{\"dir\":\"${root}\",\"recursive\":true,\"include_dirs\":false,\"return\":true}"
            }
            });

        v.push_back(CommandInfo{
            "delete",
            "Delete a file or directory.",
            {
                A{"path","string",true,"Target path.",""},
                A{"recursive","bool",false,"If true, remove directories recursively.","false"},
                A{"as","string",false,"Capture boolean result.",""},
                A{"return","bool",false,"Mark as final output.","false"}
            },
            "bool.",
            {
                "#fs.delete{\"path\":\"${root}/tmp.txt\"}",
                "#fs.delete{\"path\":\"${root}/build\",\"recursive\":true}"
            }
            });

        v.push_back(CommandInfo{
            "copy",
            "Copy file or directory.",
            {
                A{"src","string",true,"Source path.",""},
                A{"dst","string",true,"Destination path.",""},
                A{"overwrite","bool",false,"Overwrite destination if exists.","false"},
                A{"recursive","bool",false,"Copy directories recursively.","true"},
                A{"as","string",false,"Capture boolean result.",""},
                A{"return","bool",false,"Mark as final output.","false"}
            },
            "bool.",
            {
                "#fs.copy{\"src\":\"${root}/a.txt\",\"dst\":\"${root}/b.txt\",\"overwrite\":true}",
                "#fs.copy{\"src\":\"${root}/assets\",\"dst\":\"${root}/assets_backup\",\"recursive\":true}"
            }
            });

        v.push_back(CommandInfo{
            "move",
            "Move/rename file or directory.",
            {
                A{"src","string",true,"Source path.",""},
                A{"dst","string",true,"Destination path.",""},
                A{"overwrite","bool",false,"Overwrite destination if exists.","false"},
                A{"as","string",false,"Capture boolean result.",""},
                A{"return","bool",false,"Mark as final output.","false"}
            },
            "bool.",
            {
                "#fs.move{\"src\":\"${root}/old.txt\",\"dst\":\"${root}/new.txt\"}"
            }
            });

        v.push_back(CommandInfo{
            "mkdir",
            "Create a directory.",
            {
                A{"path","string",true,"Directory path.",""},
                A{"recursive","bool",false,"Create parent directories if needed.","true"},
                A{"as","string",false,"Capture boolean result.",""},
                A{"return","bool",false,"Mark as final output.","false"}
            },
            "bool.",
            {
                "#fs.mkdir{\"path\":\"${root}/out\",\"recursive\":true}"
            }
            });

        v.push_back(CommandInfo{
            "stat",
            "Get basic file status.",
            {
                A{"path","string",true,"Path to stat.",""},
                A{"as","string",false,"Capture JSON object string.",""},
                A{"return","bool",false,"Mark as final output.","false"}
            },
            "string (JSON with path, exists, is_dir, is_file, size).",
            {
                "#fs.stat{\"path\":\"${root}/video.mp4\",\"as\":\"meta\"}"
            }
            });

        v.push_back(CommandInfo{
            "exists",
            "Check if a path exists.",
            {
                A{"path","string",true,"Path to check.",""},
                A{"as","string",false,"Capture boolean result.",""},
                A{"return","bool",false,"Mark as final output.","false"}
            },
            "bool.",
            {
                "#fs.exists{\"path\":\"${root}/video.mp4\",\"return\":true}"
            }
            });

        return v;
    }
};

} // namespace Galaxy