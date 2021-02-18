#include "pch.h"

#include <vector>
#include <unordered_map>

#include <combaseapi.h>
#include <Fade_3D.h>


#pragma pack(push, 1)
union uint4{
    struct{
        unsigned long x, y, z, w;
    };
    unsigned long raw[4];
};
union float3{
    struct{
        float x, y, z;
    };
    float raw[3];
};
union float3x3{
    struct{
        float _11, _12, _13;
        float _21, _22, _23;
        float _31, _32, _33;
    };
    float3 vec[3];
    float raw[9];
};
union float4x3{
    struct{
        float _11, _12, _13;
        float _21, _22, _23;
        float _31, _32, _33;
        float _41, _42, _43;
    };
    float3 vec[4];
    float raw[12];
};
#pragma pack(pop)


static std::vector<FADE3D::Tet3*> glb_tetList;
static std::unordered_map<FADE3D::Tet3*, unsigned long> glb_tetToIndex;

static std::vector<FADE3D::Point3> glb_tmpPointTable;
static std::vector<float4x3> glb_tmpVertices;
static std::vector<uint4> glb_tmpIntraIndices;
static std::vector<uint4> glb_tmpTetAdjIndices;
static std::vector<float4x3> glb_tmpTetBaryMatrices;


static void lcl_makeBaryMatrix(const float3& v0, const float3& v1, const float3& v2, const float3& v3, float4x3* mOut){
    {
        float3x3 m;
        m.vec[0] = { v0.x - v3.x, v0.y - v3.y, v0.z - v3.z };
        m.vec[1] = { v1.x - v3.x, v1.y - v3.y, v1.z - v3.z };
        m.vec[2] = { v2.x - v3.x, v2.y - v3.y, v2.z - v3.z };

        auto cal0 = m._22 * m._33 - m._23 * m._32;
        auto cal1 = m._23 * m._31 - m._21 * m._33;
        auto cal2 = m._21 * m._32 - m._22 * m._31;

        auto det = m._11 * cal0 + m._12 * cal1 + m._13 * cal2;
        if(!(det != det)){
            float3x3 r;

            r._11 = cal0 / det;
            r._12 = (m._13 * m._32 - m._12 * m._33) / det;
            r._13 = (m._12 * m._23 - m._13 * m._22) / det;

            r._21 = cal1 / det;
            r._22 = (m._11 * m._33 - m._13 * m._31) / det;
            r._23 = (m._13 * m._21 - m._11 * m._23) / det;

            r._31 = cal2 / det;
            r._32 = (m._12 * m._31 - m._11 * m._32) / det;
            r._33 = (m._11 * m._22 - m._12 * m._21) / det;

            CopyMemory(mOut->raw, r.raw, sizeof(float3x3));
        }
        else
            CopyMemory(mOut->raw, m.raw, sizeof(float3x3));
    }

    {
        mOut->_41 = v3.x;
        mOut->_42 = v3.y;
        mOut->_43 = v3.z;
    }
}


extern "C" __declspec(dllexport) bool _cdecl TGBuildTets(float* vertices, unsigned long numVert){
    FADE3D::Fade_3D fade3D;

    if(numVert % 3)
        return false;

    {
        glb_tmpPointTable.clear();
        glb_tmpPointTable.reserve(numVert / 3);

        for(auto i = decltype(numVert){ 0 }; i < numVert; i += 3){
            FADE3D::Point3 pt(vertices[i], vertices[i + 1], vertices[i + 2]);
            glb_tmpPointTable.emplace_back(pt);
        }

        fade3D.insert(glb_tmpPointTable);
    }

    {
        glb_tetList.clear();
        glb_tetToIndex.clear();
        glb_tetToIndex.rehash(1 << 15);

        fade3D.getTetrahedra(glb_tetList);

        unsigned long uIndex = 0;
        for(auto* pTet : glb_tetList){
            if(glb_tetToIndex.emplace(pTet, uIndex).second)
                ++uIndex;
        }
    }

    glb_tmpVertices.clear();
    glb_tmpIntraIndices.clear();
    glb_tmpTetAdjIndices.clear();
    glb_tmpTetBaryMatrices.clear();
    glb_tmpVertices.resize(glb_tetList.size());
    glb_tmpIntraIndices.resize(glb_tetList.size());
    glb_tmpTetAdjIndices.resize(glb_tetList.size());
    glb_tmpTetBaryMatrices.resize(glb_tetList.size());

    auto itVertices = glb_tmpVertices.begin();
    auto itIntraIndices = glb_tmpIntraIndices.begin();
    auto itTetAdjIndices = glb_tmpTetAdjIndices.begin();
    auto itTetBaryMatrices = glb_tmpTetBaryMatrices.begin();
    for(auto itTet = glb_tetList.cbegin(), etTet = glb_tetList.cend(); itTet != etTet; ++itTet, ++itVertices, ++itIntraIndices, ++itTetAdjIndices, ++itTetBaryMatrices){
        FADE3D::Point3* pts[4];
        (*itTet)->getCorners(pts[0], pts[1], pts[2], pts[3]);

        for(size_t idx = 0; idx < _countof(pts); ++idx){
            {
                itVertices->vec[idx].x = float(pts[idx]->x());
                itVertices->vec[idx].y = float(pts[idx]->y());
                itVertices->vec[idx].z = float(pts[idx]->z());
            }
            {
                auto iIntraIndex = (*itTet)->getIntraTetIndex(pts[idx]);
                itIntraIndices->raw[idx] = (unsigned long)(iIntraIndex);
            }
            {
                unsigned long uOppTet = 0xffffffff;
                {
                    auto* pOppTet = (*itTet)->getOppTet(pts[idx]);
                    auto itFind = glb_tetToIndex.find(pOppTet);
                    if(itFind != glb_tetToIndex.end())
                        uOppTet = itFind->second;
                }

                itTetAdjIndices->raw[idx] = uOppTet;
            }
        }

        {
            lcl_makeBaryMatrix(
                itVertices->vec[itIntraIndices->x],
                itVertices->vec[itIntraIndices->y],
                itVertices->vec[itIntraIndices->z],
                itVertices->vec[itIntraIndices->w],
                &(*itTetBaryMatrices)
            );
        }
    }

    return true;
}

extern "C" __declspec(dllexport) unsigned long _cdecl TGGetTetCount(void){
    return (unsigned long)(glb_tetList.size());
}

extern "C" __declspec(dllexport) void _cdecl TGGetTetVertices(float* vOut){
    CopyMemory(vOut, glb_tmpVertices.data(), glb_tmpVertices.size() * sizeof(float4x3));
}
extern "C" __declspec(dllexport) void _cdecl TGGetTetIntraIndices(unsigned long* vOut){
    CopyMemory(vOut, glb_tmpIntraIndices.data(), glb_tmpIntraIndices.size() * sizeof(uint4));
}
extern "C" __declspec(dllexport) void _cdecl TGGetTetAjacentIndices(unsigned long* vOut){
    CopyMemory(vOut, glb_tmpTetAdjIndices.data(), glb_tmpTetAdjIndices.size() * sizeof(uint4));
}
extern "C" __declspec(dllexport) void _cdecl TGGetTetBaryMatrices(float* vOut){
    CopyMemory(vOut, glb_tmpTetBaryMatrices.data(), glb_tmpTetBaryMatrices.size() * sizeof(float4x3));
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

