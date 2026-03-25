#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class DynLibLoader {
public:
    DynLibLoader() : handle_(nullptr) {}
    ~DynLibLoader() { unload(); }

    bool load(const std::string& filepath) {
#ifdef _WIN32
        handle_ = LoadLibraryA(filepath.c_str());
#else
        handle_ = dlopen(filepath.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
        return handle_ != nullptr;
    }

    std::string get_error() const {
#ifdef _WIN32
        DWORD error = GetLastError();
        if (error == 0) return "";
        LPSTR messageBuffer = nullptr;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
        return message;
#else
        const char* err = dlerror();
        return err ? std::string(err) : "";
#endif
    }

    void unload() {
        if (handle_) {
#ifdef _WIN32
            FreeLibrary((HMODULE)handle_);
#else
            dlclose(handle_);
#endif
            handle_ = nullptr;
        }
    }

    template<typename T>
    T get_function(const std::string& func_name) {
        if (!handle_) return nullptr;
#ifdef _WIN32
        return reinterpret_cast<T>(GetProcAddress((HMODULE)handle_, func_name.c_str()));
#else
        return reinterpret_cast<T>(dlsym(handle_, func_name.c_str()));
#endif
    }

private:
    void* handle_;
};