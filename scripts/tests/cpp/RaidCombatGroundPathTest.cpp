// Real PathGenerator point construction and Detour on two adjacent synthetic ground polygons.
// Only owner/Z normalization and unused slope hooks are doubled; no client assets are required.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <G3D/Vector3.h>
#include "DetourAlloc.h"
#include "DetourCommon.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
using uint32 = uint32_t;
using int32 = int32_t;
using uint16 = uint16_t;
using NavTerrain = int;
using dtQueryFilterExt = dtQueryFilter;
namespace Movement { using PointsArray = std::vector<G3D::Vector3>; }
#define LOG_ERROR(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (0)
#include "GroundPathDeclarations.inc"

PathGenerator::PathGenerator(WorldObject const* owner)
    : _polyLength(0), _type(PATHFIND_NORMAL), _useStraightPath(false), _forceDestination(false),
      _slopeCheck(false), _pointPathLimit(MAX_POINT_PATH_LENGTH), _useRaycast(false),
      _source(owner), _navMesh(nullptr), _navMeshQuery(nullptr) { }
PathGenerator::~PathGenerator() = default;
void PathGenerator::NormalizePath() { } // Synthetic floor is exactly Z=0, no server Map/owner required.
bool PathGenerator::IsSwimmableSegment(float const*, float const*, bool) const
{
    throw std::runtime_error("unexpected swimming check");
}
bool PathGenerator::IsWalkableClimb(float const*, float const*) const
{
    throw std::runtime_error("unexpected slope check");
}
#include "GroundPathMethods.inc"

void ConfigureGroundCapacity(PathGenerator& path)
{
#include "GroundAdapterCapacity.inc"
}

int main()
{
    unsigned short vertices[] = {0,0,0, 0,0,8, 4,0,8, 4,0,0, 8,0,8, 8,0,0};
    unsigned short polygons[] = {0,1,2,3, 0xffff,0xffff,1,0xffff,
                                 3,2,4,5, 0,0xffff,0xffff,0xffff};
    unsigned short flags[] = {1,1};
    unsigned char areas[] = {0,0};
    dtNavMeshCreateParams params{};
    params.verts = vertices;
    params.vertCount = 6;
    params.polys = polygons;
    params.polyFlags = flags;
    params.polyAreas = areas;
    params.polyCount = 2;
    params.nvp = 4;
    params.bmax[0] = params.bmax[2] = 8;
    params.bmax[1] = 3;
    params.cs = params.ch = 1;
    params.walkableHeight = 2;
    params.walkableRadius = 0.25f;
    params.walkableClimb = 1;
    params.buildBvTree = true;
    unsigned char* data = nullptr;
    int size = 0;
    CHECK(dtCreateNavMeshData(&params, &data, &size));
    dtNavMesh mesh;
    CHECK(dtStatusSucceed(mesh.init(data, size, DT_TILE_FREE_DATA)));
    dtNavMeshQuery query;
    CHECK(dtStatusSucceed(query.init(&mesh, 128)));
    for (bool crossing : {false, true})
    {
        for (float capacity : {6.0f, 8.0f, -1.0f})
        {
            PathGenerator path(nullptr);
            path._navMesh = &mesh;
            path._navMeshQuery = &query;
            if (capacity < 0)
                ConfigureGroundCapacity(path);
            else
                path.SetPathLengthLimit(capacity);
            float start[] = {crossing ? 3.0f : 1.0f, 0, 4};
            float end[] = {crossing ? 5.0f : 3.0f, 0, 4};
            float extents[] = {1, 2, 1};
            dtPolyRef first = 0, last = 0;
            CHECK(dtStatusSucceed(query.findNearestPoly(start, extents, &path._filter, &first, nullptr)));
            CHECK(dtStatusSucceed(query.findNearestPoly(end, extents, &path._filter, &last, nullptr)));
            CHECK(first && last);
            CHECK(dtStatusSucceed(query.findPath(first, last, start, end, &path._filter,
                path._pathPolyRefs, reinterpret_cast<int*>(&path._polyLength), MAX_PATH_LENGTH)));
            CHECK(path._polyLength == (crossing ? 2u : 1u));
            path.SetStartPosition({start[2], start[0], start[1]});
            path.SetEndPosition({end[2], end[0], end[1]});
            path.BuildPointPath(start, end);
            std::cout << "polygons=" << (crossing ? 2 : 1) << " capacity=" << path._pointPathLimit
                      << " type=" << path.GetPathType() << " points=" << path.GetPath().size() << '\n';
            if (capacity == 6)
                CHECK(path.GetPathType() == (crossing ? (PATHFIND_SHORTCUT | PATHFIND_NOPATH) : PATHFIND_NORMAL));
            else if (capacity == 8)
                CHECK(path.GetPathType() == (PATHFIND_SHORTCUT | PATHFIND_SHORT));
            else
            {
                CHECK(path.GetPathType() == PATHFIND_NORMAL);
                CHECK(path.GetPath().size() == 2);
                CHECK((path.GetPath().back() - path.GetEndPosition()).length() < 0.001f);
                CHECK(std::abs(path.getPathLength() - 2) < 0.001f);
            }
        }
    }
    std::cout << "actual PathGenerator/Detour capacity regression passed\n";
}
