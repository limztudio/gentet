#include "pch.h"

#include <map>

#include <combaseapi.h>
#include <Fade_3D.h>


static unsigned long glb_tetIndexCount = 0u;
static unsigned long* glb_tetIndices = nullptr;


extern "C" __declspec(dllexport) void _cdecl TGBuildTets(float* vertices, unsigned long numVert){
    std::map<FADE3D::Point3, unsigned long> pointIndexer;
    FADE3D::Fade_3D fade3D;

    {
        unsigned long uIndex = 0u;
        for(auto* pVert = vertices; (pVert - vertices) < numVert;){
            FADE3D::Point3 pt(*pVert++, *pVert++, *pVert++);
            fade3D.insert(pt);

            if(pointIndexer.emplace(pt, uIndex).second)
                ++uIndex;
        }
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
            auto fIt = pointIndexer.find(*pt);
            if(fIt != pointIndexer.cend())
                *pTetIndex = fIt->second;
            else
                *pTetIndex = 0xffffffff;

            ++pTetIndex;
        }
    }
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

