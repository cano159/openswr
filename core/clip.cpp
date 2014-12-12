// Copyright 2014 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <assert.h>

#include "os.h"
#include "clip.h"

inline void intersect(
    SWR_CLIPCODES clippingPlane, // clip plane (left, right, bottom, top, zerow)
    int s,                       // index to first edge vertex v0 in pInPts.
    int p,                       // index to second edge vertex v1 in pInPts.
    const float *pInPts,         // array of all the input positions.
    const float *pInAttribs,     // array of all attributes for all vertex. All the attributes for each vertex is contiguous.
    int numInAttribs,            // number of attributes per vertex.
    int i,                       // output index.
    float *pOutPts,              // array of output positions. We'll write our new intersection point at i*4.
    float *pOutAttribs)          // array of output attributes. We'll write our new attributes at i*numInAttribs.
{
    float t;

    // Find the parameter of the intersection.
    //		t = (v1.w - v1.x) / ((v2.x - v1.x) - (v2.w - v1.w)) for x = w (RIGHT) plane, etc.
    const float *v1 = &pInPts[s * 4];
    const float *v2 = &pInPts[p * 4];

    switch (clippingPlane)
    {
    case FRUSTUM_LEFT:
        t = (v1[0] + v1[3]) / (v1[0] - v2[0] - v2[3] + v1[3]);
        break;
    case FRUSTUM_RIGHT:
        t = (v1[0] - v1[3]) / (v1[0] - v2[0] - v1[3] + v2[3]);
        break;
    case FRUSTUM_TOP:
        t = (v1[1] + v1[3]) / (v1[1] - v2[1] - v2[3] + v1[3]);
        break;
    case FRUSTUM_BOTTOM:
        t = (v1[1] - v1[3]) / (v1[1] - v2[1] - v1[3] + v2[3]);
        break;
    case FRUSTUM_NEAR:
        t = (v1[2] + v1[3]) / (v1[2] - v2[2] - v2[3] + v1[3]);
        break;
    case FRUSTUM_FAR:
        t = (v1[2] - v1[3]) / (v1[2] - v2[2] - v1[3] + v2[3]);
        break;
    default:
        assert(0 && "invalid clipping plane");
    };

    const float *a1 = &pInAttribs[s * numInAttribs];
    const float *a2 = &pInAttribs[p * numInAttribs];

    float *pOutP = &pOutPts[i * 4];
    float *pOutA = &pOutAttribs[i * numInAttribs];

    // Interpolate new position.
    for (int j = 0; j < 4; ++j)
    {
        pOutP[j] = v1[j] + (v2[j] - v1[j]) * t;
    }

    // Interpolate Attributes
    for (int attr = 0; attr < numInAttribs; ++attr)
    {
        pOutA[attr] = a1[attr] + (a2[attr] - a1[attr]) * t;
    }
}

// Checks whether vertex v lies inside clipping plane
// in homogenous coords check -w < {x,y,z} < w;
//
inline int inside(const float v[4], SWR_CLIPCODES clippingPlane)
{
    switch (clippingPlane)
    {
    case FRUSTUM_LEFT:
        return (v[0] >= -v[3]);
    case FRUSTUM_RIGHT:
        return (v[0] <= v[3]);
    case FRUSTUM_TOP:
        return (v[1] >= -v[3]);
    case FRUSTUM_BOTTOM:
        return (v[1] <= v[3]);
    case FRUSTUM_NEAR:
        return (v[2] >= -v[3]);
    case FRUSTUM_FAR:
        return (v[2] <= v[3]);
    default:
        assert(0 && "invalid clipping plane");
        return 0;
    }
}

// Clips a polygon in homogenous coordinates to a particular clipping plane.
// Takes in vertices of the polygon (InPts) and the clipping plane
// Puts the vertices of the clipped polygon in OutPts
// Returns number of points in clipped polygon
//
int ClipTriToPlane(const float *pInPts, int numInPts,
                   const float *pInAttribs, int numInAttribs,
                   SWR_CLIPCODES clippingPlane,
                   float *pOutPts, float *pOutAttribs)
{
    int i = 0; // index number of OutPts, # of vertices in OutPts = i div 4;

    for (int j = 0; j < numInPts; ++j)
    {
        int s = j;
        int p = (j + 1) % numInPts;

        int s_in = inside(&pInPts[s * 4], clippingPlane);
        int p_in = inside(&pInPts[p * 4], clippingPlane);

        // test if vertex is to be added to output vertices
        if (s_in != p_in) // edge crosses clipping plane
        {
            // find point of intersection
            intersect(clippingPlane, s, p, pInPts, pInAttribs, numInAttribs, i, pOutPts, pOutAttribs);
            i++;
        }
        if (p_in) // 2nd vertex is inside clipping volume, add it to output
        {
            // Copy 2nd vertex position of edge over to output.
            for (int k = 0; k < 4; ++k)
            {
                pOutPts[i * 4 + k] = pInPts[p * 4 + k];
            }
            // Copy 2nd vertex attributes of edge over to output.
            for (int attr = 0; attr < numInAttribs; ++attr)
            {
                pOutAttribs[i * numInAttribs + attr] = pInAttribs[p * numInAttribs + attr];
            }
            i++;
        }
        // edge does not cross clipping plane and vertex outside clipping volume
        //  => do not add vertex
    }
    return i;
}

void Clip(const float *pTriangle, const float *pAttribs, int numAttribs, float *pOutTriangles, int *numVerts, float *pOutAttribs)
{

    // temp storage to hold at least 6 sets of vertices, the max number that can be created during clipping
    OSALIGN(float, 16) tempPts[6 * 4];
    OSALIGN(float, 16) tempAttribs[6 * KNOB_NUM_ATTRIBUTES * 4];

    // we opt to clip to viewport frustum to produce smaller triangles for rasterization precision
    int NumOutPts = ClipTriToPlane(pTriangle, 3, pAttribs, numAttribs, FRUSTUM_NEAR, tempPts, tempAttribs);
    NumOutPts = ClipTriToPlane(tempPts, NumOutPts, tempAttribs, numAttribs, FRUSTUM_FAR, pOutTriangles, pOutAttribs);
    NumOutPts = ClipTriToPlane(pOutTriangles, NumOutPts, pOutAttribs, numAttribs, FRUSTUM_LEFT, tempPts, tempAttribs);
    NumOutPts = ClipTriToPlane(tempPts, NumOutPts, tempAttribs, numAttribs, FRUSTUM_RIGHT, pOutTriangles, pOutAttribs);
    NumOutPts = ClipTriToPlane(pOutTriangles, NumOutPts, pOutAttribs, numAttribs, FRUSTUM_BOTTOM, tempPts, tempAttribs);
    NumOutPts = ClipTriToPlane(tempPts, NumOutPts, tempAttribs, numAttribs, FRUSTUM_TOP, pOutTriangles, pOutAttribs);

    assert(NumOutPts <= 6);

    *numVerts = NumOutPts;
    return;
}
