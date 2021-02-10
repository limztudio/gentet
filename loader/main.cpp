#include <vector>

#include <windows.h>


typedef void(_cdecl*_TGBuildTets)(float* vertices, unsigned long numVert);
typedef unsigned long(_cdecl*_TGGetTetIndexCount)(void);
typedef unsigned long*(_cdecl*_TGGetTetIndices)(void);
typedef void(_cdecl*_TGCleanup)(void);


struct Uint4{
    unsigned long raw[4];
};


int main(){
#if defined(_M_X64) || defined(__amd64__)
    HMODULE pLibrary = LoadLibrary("tetgen_x64.dll");
#else
    HMODULE pLibrary = LoadLibrary("tetgen_Win32.dll");
#endif

    _TGBuildTets TGBuildTets = reinterpret_cast<_TGBuildTets>(GetProcAddress(pLibrary, "TGBuildTets"));
    _TGGetTetIndexCount TGGetTetIndexCount = reinterpret_cast<_TGGetTetIndexCount>(GetProcAddress(pLibrary, "TGGetTetIndexCount"));
    _TGGetTetIndices TGGetTetIndices = reinterpret_cast<_TGGetTetIndices>(GetProcAddress(pLibrary, "TGGetTetIndices"));
    _TGCleanup TGCleanup = reinterpret_cast<_TGCleanup>(GetProcAddress(pLibrary, "TGCleanup"));

    float vertices[] = {
        0, 0, 0,
        1, 0, 0,
        0, 0, 1,
        1, 0, 1,

        0, 1, 0,
        1, 1, 0,
        0, 1, 1,
        1, 1, 1,
    };
    TGBuildTets(vertices, _countof(vertices));

    std::vector<Uint4> indices(TGGetTetIndexCount() >> 2);
    CopyMemory(indices.data(), TGGetTetIndices(), indices.size() * sizeof(Uint4));

    TGCleanup();

    FreeLibrary(pLibrary);
    return 0;
}