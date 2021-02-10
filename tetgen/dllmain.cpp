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


static std::unordered_map<FADE3D::Point3, unsigned long, Point3Hasher> glb_pointIndexer;
static std::vector<FADE3D::Point3> glb_tmpPointTable;

static unsigned long glb_tetIndexCount = 0u;
static unsigned long* glb_tetIndices = nullptr;


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

    if(glb_tetIndices)
        CoTaskMemFree(glb_tetIndices);

    glb_tetIndexCount = decltype(glb_tetIndexCount)(tets.size() << 2);
    glb_tetIndices = decltype(glb_tetIndices)(CoTaskMemAlloc(sizeof(unsigned long) * glb_tetIndexCount));

    auto* pTetIndex = glb_tetIndices;
    for(const auto* pTet : tets){
        FADE3D::Point3* pts[4];
        pTet->getCorners(pts[0], pts[1], pts[2], pts[3]);

        for(const auto* pt : pts){
            auto fIt = glb_pointIndexer.find(*pt);
            if(fIt != glb_pointIndexer.cend())
                *pTetIndex = fIt->second;
            else
                *pTetIndex = 0xffffffff;

            ++pTetIndex;
        }
    }

    return true;
}
extern "C" __declspec(dllexport) unsigned long _cdecl TGGetTetIndexCount(void){
    return glb_tetIndexCount;
}
extern "C" __declspec(dllexport) unsigned long* _cdecl TGGetTetIndices(void){
    return glb_tetIndices;
}
extern "C" __declspec(dllexport) void _cdecl TGCleanup(void){
    if(glb_tetIndices){
        glb_tetIndexCount = 0u;

        CoTaskMemFree(glb_tetIndices);
        glb_tetIndices = nullptr;
    }
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

