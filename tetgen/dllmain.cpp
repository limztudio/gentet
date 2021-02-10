#include "pch.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <combaseapi.h>
#include <Fade_3D.h>


struct Point3Hasher{
    inline size_t operator()(const FADE3D::Point3& key)const{
        return std::hash<std::string>{}(std::to_string(key.x()) + "," + std::to_string(key.y()) + "," + std::to_string(key.z()));
    }
};
#pragma pack(push, 1)
struct Uint4{
    unsigned long raw[4];
};
#pragma pack(pop)


static std::unordered_map<FADE3D::Point3, unsigned long, Point3Hasher> glb_pointIndexer;
static std::vector<FADE3D::Point3> glb_tmpPointTable;
static std::vector<Uint4> glb_tmpTetIndices;


extern "C" __declspec(dllexport) bool _cdecl TGBuildTets(float* vertices, unsigned long numVert){
    FADE3D::Fade_3D fade3D;

    if(numVert % 3)
        return false;

    {
        glb_pointIndexer.clear();
        glb_tmpPointTable.clear();

        glb_pointIndexer.rehash(1 << 17);
        glb_tmpPointTable.reserve(numVert / 3);

        unsigned long uIndex = 0u;
        for(auto i = decltype(numVert){ 0 }; i < numVert; i += 3){
            FADE3D::Point3 pt(vertices[i], vertices[i + 1], vertices[i + 2]);
            if(glb_pointIndexer.emplace(pt, uIndex).second){
                glb_tmpPointTable.emplace_back(pt);
                ++uIndex;
            }
        }

        fade3D.insert(glb_tmpPointTable);
    }

    std::vector<FADE3D::Tet3*> tets;
    fade3D.getTetrahedra(tets);

    glb_tmpTetIndices.clear();
    glb_tmpTetIndices.resize(tets.size());

    auto itOutIndices = glb_tmpTetIndices.begin();
    for(auto itTet = tets.cbegin(), etTet = tets.cend(); itTet != etTet; ++itTet, ++itOutIndices){
        FADE3D::Point3* pts[4];
        (*itTet)->getCorners(pts[0], pts[1], pts[2], pts[3]);

        for(size_t idx = 0; idx < _countof(pts); ++idx){
            auto fIt = glb_pointIndexer.find(*pts[idx]);
            itOutIndices->raw[idx] = (fIt != glb_pointIndexer.cend()) ? fIt->second : 0xffffffff;
        }
    }

    return true;
}
extern "C" __declspec(dllexport) unsigned long _cdecl TGGetTetIndexCount(void){
    return (unsigned long)(glb_tmpTetIndices.size() << 2);
}
extern "C" __declspec(dllexport) void _cdecl TGGetTetIndices(unsigned long* vOut){
    CopyMemory(vOut, glb_tmpTetIndices.data(), glb_tmpTetIndices.size() * sizeof(Uint4));
}


BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch(ul_reason_for_call){
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

