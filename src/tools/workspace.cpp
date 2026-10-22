// ECE 309 Project 3 - Workspace path checking.
// Only check the path text; the supplied code checks symbolic links later.

#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    try {
        if (user_path.empty()) {
            *reason = "empty path";
            return false;
        }

        if (user_path.find('\0') != std::string_view::npos) {
            *reason = "path contains NUL character";
            return false;
        }

        fs::path p{std::string(user_path)};
        if (p.is_absolute() || p.has_root_path()) {
            *reason = "absolute paths are not allowed";
            return false;
        }

        fs::path n = p.lexically_normal();
        if (!n.empty() && *n.begin() == "..") {
            *reason = "path goes outside workspace";
            return false;
        }

        if (n == ".") {
            *out = root();
        } else {
            *out = root() / n;
        }
        reason->clear();
        return true;
    } catch (const std::exception&) {
        *reason = "invalid path";
        return false;
    }
}
