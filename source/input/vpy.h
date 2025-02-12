/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Vladimir Kontserenko <djatom@beatrice-raws.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at license @ x265.com.
 *****************************************************************************/

#ifndef X265_VPY_H
#define X265_VPY_H

#include <unordered_map>
#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <vapoursynth/VSScript.h>
#include <vapoursynth/VSHelper.h>
#include "input.h"

#if _WIN32
#include <windows.h>
using lib_path_t = std::wstring;
using lib_t = HMODULE;
using func_t = FARPROC;
#else
#include <unistd.h>
#define Sleep(x) usleep(x)
#include <dlfcn.h>
#define __stdcall
using lib_path_t = std::string;
using lib_t = void*;
using func_t = void*;
#endif

#if defined(_WIN32) || defined(_WIN64)
#define CloseEvent CloseHandle
#else
#include "event.h"
#endif

#ifdef X86_64
    #define LOAD_VS_FUNC(name, _) \
    {\
        vss_func.name = reinterpret_cast<decltype(vss_func.name)>((void*)vs_address("vsscript_" #name));\
        if (!vss_func.name) goto fail;\
    }
#else
    #define LOAD_VS_FUNC(name, decorated_name) \
    {\
        vss_func.name = reinterpret_cast<decltype(vss_func.name)>((void*)vs_address(decorated_name));\
        if (!vss_func.name) goto fail;\
    }
#endif


namespace X265_NS {

class VPYInput : public InputFile
{
protected:

    std::unordered_map<int, std::pair<HANDLE, const VSFrameRef*>> frameMap;
    int parallelRequests {-1};
    std::atomic<int> requestedFrames {-1};
    std::atomic<int> completedFrames {-1};
    int framesToRequest {-1};
    std::atomic<bool> isRunning {false};
    int nextFrame {0};
    int nodeIndex {0};
    bool useScriptSar {false};
    bool vpyFailed {false};
    char frameError[512];
    size_t frame_size {0};
    uint8_t* frame_buffer {nullptr};
    InputFileInfo _info;
    lib_t vss_library;
    struct {
        int (VS_CC *init)();
        int (VS_CC *finalize)();
        int (VS_CC *evaluateFile)(VSScript** handle, const char* scriptFilename, int flags);
        void (VS_CC *freeScript)(VSScript* handle);
        const char* (VS_CC *getError)(VSScript* handle);
        VSNodeRef* (VS_CC *getOutput)(VSScript* handle, int index);
        VSCore* (VS_CC *getCore)(VSScript* handle);
        const VSAPI* (VS_CC *getVSApi2)(int version);
    } vss_func;
    #if _WIN32
        lib_path_t vss_library_path {L"vsscript"};
        void vs_open() { vss_library = LoadLibraryW(vss_library_path.c_str()); }
        void vs_close() { FreeLibrary(vss_library); vss_library = nullptr; }
        func_t vs_address(LPCSTR func) { return GetProcAddress(vss_library, func); }
    #else
        #ifdef __MACH__
            lib_path_t vss_library_path {"libvapoursynth-script.dylib"};
        #else
            lib_path_t vss_library_path {"libvapoursynth-script.so"};
        #endif
        void vs_open() { vss_library = dlopen(vss_library_path.c_str(), RTLD_GLOBAL | RTLD_LAZY | RTLD_NOW); }
        void vs_close() { dlclose(vss_library); vss_library = nullptr; }
        func_t vs_address(const char* func) { return dlsym(vss_library, func); }
    #endif
	lib_path_t convertLibraryPath(std::string);
    void parseVpyOptions(const char* _options);
    const VSFrameRef* getAsyncFrame(int n);
    const VSAPI* vsapi = nullptr;
    VSScript* script = nullptr;
    VSNodeRef* node = nullptr;
	void load_vs();

public:

    VPYInput(InputFileInfo& info);
    ~VPYInput() {};
    void setAsyncFrame(int n, const VSFrameRef* f, const char* errorMsg);
    void release();
    bool isEof() const                            { return nextFrame >= _info.frameCount; }
    bool isFail() { return vpyFailed; }
    void startReader();
    void stopReader();
    bool readPicture(x265_picture&);

    const char* getName() const                   { return "vpy"; }

    int getWidth() const                          { return _info.width; }

    int getHeight() const                         { return _info.height; }

    int outputFrame()                             { return nextFrame; }
};
}

#endif // ifndef X265_VPY_H
