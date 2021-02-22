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

struct tetrahedron{
    float3 v[4];
};
struct octahedron{
    float3 v[6];
};


static std::vector<FADE3D::Tet3*> glb_tetList;
static std::unordered_map<FADE3D::Tet3*, unsigned long> glb_tetToIndex;

static std::vector<FADE3D::Point3> glb_tmpPointTable;
static std::vector<float4x3> glb_tmpVertices;
static std::vector<uint4> glb_tmpIntraIndices;
static std::vector<uint4> glb_tmpTetAdjIndices;
static std::vector<float4x3> glb_tmpTetBaryMatrices;


static bool lcl_equal(const float3& lhs, const float3& rhs){
    if(lhs.x != rhs.x)
        return false;
    if(lhs.y != rhs.y)
        return false;
    if(lhs.z != rhs.z)
        return false;
    return true;
}

static float3 lcl_cross(const float3& v0, const float3& v1){
    float3 v = { (v0.y * v1.z) - (v0.z * v1.y), (v0.z * v1.x) - (v0.x * v1.z), (v0.x * v1.y) - (v0.y * v1.x) };
    return v;
}
static float lcl_dot(const float3& v0, const float3& v1){
    float v = (v0.x * v1.x) + (v0.y * v1.y) + (v0.z * v1.z);
    return v;
}

static float3 lcl_lerp(const float3& v0, const float3& v1, float t){
    float3 v = { v0.x + t * (v1.x - v0.x), v0.y + t * (v1.y - v0.y), v0.z + t * (v1.z - v0.z) };
    return v;
}
static float3 lcl_middle(const float3& v0, const float3& v1){
    float3 v = { (v1.x + v0.x) * 0.5f, (v1.y + v0.y) * 0.5f, (v1.z + v0.z) * 0.5f };
    return v;
}
static float3 lcl_centroid(const tetrahedron& oc){
    float3 vO = { 0.f, 0.f, 0.f };
    for(const auto& vC : oc.v){
        vO.x += vC.x;
        vO.y += vC.y;
        vO.z += vC.z;
    }
    vO.x /= float(_countof(tetrahedron::v));
    vO.y /= float(_countof(tetrahedron::v));
    vO.z /= float(_countof(tetrahedron::v));
    return vO;
}
static float3 lcl_centroid(const octahedron& oc){
    float3 vO = { 0.f, 0.f, 0.f };
    for(const auto& vC : oc.v){
        vO.x += vC.x;
        vO.y += vC.y;
        vO.z += vC.z;
    }
    vO.x /= float(_countof(octahedron::v));
    vO.y /= float(_countof(octahedron::v));
    vO.z /= float(_countof(octahedron::v));
    return vO;
}

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


// not a good idea, but let's just hard-code to do it.

// input: 4 vertices of tet
// output: every 4 vertices of 4 tets and 6 vertices of oct
extern "C" __declspec(dllexport) void _cdecl TGSubdivideTet(const float* pRawInputTetVerts, float* pRawOutputTetsVerts, float* pRawOutputOctVerts){
    const auto* pInputTet = reinterpret_cast<const tetrahedron*>(pRawInputTetVerts);

    auto* pOutputTets = reinterpret_cast<tetrahedron*>(pRawOutputTetsVerts);
    auto* pOutputOct = reinterpret_cast<octahedron*>(pRawOutputOctVerts);

    const auto& vP0 = pInputTet->v[0];
    const auto& vP1 = pInputTet->v[1];
    const auto& vP2 = pInputTet->v[2];
    const auto& vP3 = pInputTet->v[3];

    const auto vM01 = lcl_middle(vP0, vP1);
    const auto vM02 = lcl_middle(vP0, vP2);
    const auto vM03 = lcl_middle(vP0, vP3);
    const auto vM12 = lcl_middle(vP1, vP2);
    const auto vM13 = lcl_middle(vP1, vP3);
    const auto vM23 = lcl_middle(vP2, vP3);

    pOutputTets[0].v[0] = vP0;
    pOutputTets[0].v[1] = vM01;
    pOutputTets[0].v[2] = vM02;
    pOutputTets[0].v[3] = vM03;

    pOutputTets[1].v[0] = vP1;
    pOutputTets[1].v[1] = vM01;
    pOutputTets[1].v[2] = vM12;
    pOutputTets[1].v[3] = vM13;

    pOutputTets[2].v[0] = vP2;
    pOutputTets[2].v[1] = vM02;
    pOutputTets[2].v[2] = vM23;
    pOutputTets[2].v[3] = vM12;

    pOutputTets[3].v[0] = vP3;
    pOutputTets[3].v[1] = vM03;
    pOutputTets[3].v[2] = vM23;
    pOutputTets[3].v[3] = vM13;

    pOutputOct->v[0] = vM03;
    pOutputOct->v[1] = vM01;
    pOutputOct->v[2] = vM02;
    pOutputOct->v[3] = vM23;
    pOutputOct->v[4] = vM13;
    pOutputOct->v[5] = vM12;
}
// input: 6 vertices of oct
// output: every 4 vertices of 8 tets and every 6 vertices of 6 octs
extern "C" __declspec(dllexport) void _cdecl TGSubdivideOct(const float* pRawInputOctVerts, float* pRawOutputTetsVerts, float* pRawOutputOctsVerts){
    const auto* pInputOct = reinterpret_cast<const octahedron*>(pRawInputOctVerts);

    auto* pOutputTets = reinterpret_cast<tetrahedron*>(pRawOutputTetsVerts);
    auto* pOutputOcts = reinterpret_cast<octahedron*>(pRawOutputOctsVerts);

    const auto& vP0 = pInputOct->v[0];
    const auto& vP1 = pInputOct->v[1];
    const auto& vP2 = pInputOct->v[2];
    const auto& vP3 = pInputOct->v[3];
    const auto& vP4 = pInputOct->v[4];
    const auto& vP5 = pInputOct->v[5];

    const auto vM01 = lcl_middle(vP0, vP1);
    const auto vM02 = lcl_middle(vP0, vP2);
    const auto vM03 = lcl_middle(vP0, vP3);
    const auto vM04 = lcl_middle(vP0, vP4);
    const auto vM12 = lcl_middle(vP1, vP2);
    const auto vM14 = lcl_middle(vP1, vP4);
    const auto vM15 = lcl_middle(vP1, vP5);
    const auto vM23 = lcl_middle(vP2, vP3);
    const auto vM25 = lcl_middle(vP2, vP5);
    const auto vM34 = lcl_middle(vP3, vP4);
    const auto vM35 = lcl_middle(vP3, vP5);
    const auto vM45 = lcl_middle(vP4, vP5);

    const auto vC = lcl_centroid(*pInputOct);

    pOutputTets[0].v[0] = vC;
    pOutputTets[0].v[1] = vM01;
    pOutputTets[0].v[2] = vM12;
    pOutputTets[0].v[3] = vM02;

    pOutputTets[1].v[0] = vC;
    pOutputTets[1].v[1] = vM02;
    pOutputTets[1].v[2] = vM23;
    pOutputTets[1].v[3] = vM03;

    pOutputTets[2].v[0] = vC;
    pOutputTets[2].v[1] = vM03;
    pOutputTets[2].v[2] = vM34;
    pOutputTets[2].v[3] = vM04;

    pOutputTets[3].v[0] = vC;
    pOutputTets[3].v[1] = vM04;
    pOutputTets[3].v[2] = vM14;
    pOutputTets[3].v[3] = vM01;

    pOutputTets[4].v[0] = vC;
    pOutputTets[4].v[1] = vM12;
    pOutputTets[4].v[2] = vM15;
    pOutputTets[4].v[3] = vM25;

    pOutputTets[5].v[0] = vC;
    pOutputTets[5].v[1] = vM23;
    pOutputTets[5].v[2] = vM25;
    pOutputTets[5].v[3] = vM35;

    pOutputTets[6].v[0] = vC;
    pOutputTets[6].v[1] = vM34;
    pOutputTets[6].v[2] = vM35;
    pOutputTets[6].v[3] = vM45;

    pOutputTets[7].v[0] = vC;
    pOutputTets[7].v[1] = vM14;
    pOutputTets[7].v[2] = vM45;
    pOutputTets[7].v[3] = vM15;

    pOutputOcts[0].v[0] = vP0;
    pOutputOcts[0].v[1] = vM01;
    pOutputOcts[0].v[2] = vM02;
    pOutputOcts[0].v[3] = vM03;
    pOutputOcts[0].v[4] = vM04;
    pOutputOcts[0].v[5] = vC;

    pOutputOcts[1].v[0] = vP1;
    pOutputOcts[1].v[1] = vM01;
    pOutputOcts[1].v[2] = vM12;
    pOutputOcts[1].v[3] = vM15;
    pOutputOcts[1].v[4] = vM14;
    pOutputOcts[1].v[5] = vC;

    pOutputOcts[2].v[0] = vP2;
    pOutputOcts[2].v[1] = vM02;
    pOutputOcts[2].v[2] = vM23;
    pOutputOcts[2].v[3] = vM25;
    pOutputOcts[2].v[4] = vM12;
    pOutputOcts[2].v[5] = vC;

    pOutputOcts[3].v[0] = vP3;
    pOutputOcts[3].v[1] = vM03;
    pOutputOcts[3].v[2] = vM34;
    pOutputOcts[3].v[3] = vM35;
    pOutputOcts[3].v[4] = vM23;
    pOutputOcts[3].v[5] = vC;

    pOutputOcts[4].v[0] = vP4;
    pOutputOcts[4].v[1] = vM04;
    pOutputOcts[4].v[2] = vM14;
    pOutputOcts[4].v[3] = vM45;
    pOutputOcts[4].v[4] = vM34;
    pOutputOcts[4].v[5] = vC;

    pOutputOcts[5].v[0] = vP5;
    pOutputOcts[5].v[1] = vM45;
    pOutputOcts[5].v[2] = vM35;
    pOutputOcts[5].v[3] = vM25;
    pOutputOcts[5].v[4] = vM15;
    pOutputOcts[5].v[5] = vC;
}

extern "C" __declspec(dllexport) float _cdecl TGGetTetVolume(float* pRawInputTetVerts){
    const auto* pInputTet = reinterpret_cast<const tetrahedron*>(pRawInputTetVerts);

    float3 a = { pInputTet->v[0].x - pInputTet->v[3].x, pInputTet->v[0].y - pInputTet->v[3].y, pInputTet->v[0].z - pInputTet->v[3].z };
    float3 b = { pInputTet->v[1].x - pInputTet->v[3].x, pInputTet->v[1].y - pInputTet->v[3].y, pInputTet->v[1].z - pInputTet->v[3].z };
    float3 c = { pInputTet->v[2].x - pInputTet->v[3].x, pInputTet->v[2].y - pInputTet->v[3].y, pInputTet->v[2].z - pInputTet->v[3].z };

    auto det = fabs(lcl_dot(a, lcl_cross(b, c)));
    det /= 6.f;

    return det;
}
extern "C" __declspec(dllexport) float _cdecl TGGetOctVolume(float* pRawInputOctVerts){
    const auto* pInputOct = reinterpret_cast<const octahedron*>(pRawInputOctVerts);

    tetrahedron pInputTets[] = {
        {
            pInputOct->v[0],
            pInputOct->v[1],
            pInputOct->v[2],
            pInputOct->v[3],
        },
        {
            pInputOct->v[0],
            pInputOct->v[1],
            pInputOct->v[3],
            pInputOct->v[4],
        },
        {
            pInputOct->v[5],
            pInputOct->v[3],
            pInputOct->v[2],
            pInputOct->v[1],
        },
        {
            pInputOct->v[5],
            pInputOct->v[4],
            pInputOct->v[3],
            pInputOct->v[1],
        },
    };

    float det = 0.f;
    for(const auto* pInputTet = pInputTets; (pInputTet - pInputTets) < _countof(pInputTets); ++pInputTet){
        float3 a = { pInputTet->v[0].x - pInputTet->v[3].x, pInputTet->v[0].y - pInputTet->v[3].y, pInputTet->v[0].z - pInputTet->v[3].z };
        float3 b = { pInputTet->v[1].x - pInputTet->v[3].x, pInputTet->v[1].y - pInputTet->v[3].y, pInputTet->v[1].z - pInputTet->v[3].z };
        float3 c = { pInputTet->v[2].x - pInputTet->v[3].x, pInputTet->v[2].y - pInputTet->v[3].y, pInputTet->v[2].z - pInputTet->v[3].z };

        det += fabs(lcl_dot(a, lcl_cross(b, c)));
    }

    det /= 6.f;

    return det;
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

