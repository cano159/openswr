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

DEF_BUCKET(0, APIClearRenderTarget, 1);
DEF_BUCKET(0, APIDraw, 1);
DEF_BUCKET(1, APIDrawWakeAllThreads, 0);
DEF_BUCKET(0, APIDrawIndexed, 1);
DEF_BUCKET(0, APIExecuteDisplayList, 0);
DEF_BUCKET(0, APIOptimizeDisplayList, 0);
DEF_BUCKET(0, APIPresent, 1);
DEF_BUCKET(0, APIFlip, 1);
DEF_BUCKET(0, APIGetDrawContext, 0);
DEF_BUCKET(0, APIReadPixels, 1);
DEF_BUCKET(0, APITexImage, 1);
DEF_BUCKET(0, FEProcessIndices, 1);
DEF_BUCKET(1, FEVertexCheckCache, 0);
DEF_BUCKET(2, FENumCacheHits, 0);
DEF_BUCKET(1, FEVertexStoreCache, 0);
DEF_BUCKET(0, FEProcessDrawVertices, 1);
DEF_BUCKET(0, FEProcessDrawIndexedVertices, 1);
DEF_BUCKET(0, FEProcessDraw, 1);
DEF_BUCKET(1, FEFetchShader, 0);
DEF_BUCKET(1, FEVertexShader, 0);
DEF_BUCKET(1, FEPAAssemble, 0);
DEF_BUCKET(1, FEBinTriangles, 0);
DEF_BUCKET(2, FEBinPacker, 0);
DEF_BUCKET(1, FECoarseZ, 0);
DEF_BUCKET(1, FETriangleSetup, 0);
DEF_BUCKET(2, FEViewportCull, 0);
DEF_BUCKET(2, FEGuardbandClip, 0);
DEF_BUCKET(2, FECullZeroAreaAndBackface, 0);
DEF_BUCKET(2, FECullBetweenCenters, 0)
DEF_BUCKET(2, FEEarlyRast, 0);
DEF_BUCKET(0, FEProcessPresent, 1);
DEF_BUCKET(0, WorkerWorkOnFifoBE, 0);
DEF_BUCKET(1, WorkerFoundWork, 1);
DEF_BUCKET(2, BEClear, 0);
DEF_BUCKET(2, BERasterizeOneTileTri, 0);
DEF_BUCKET(2, BERasterizeSmallTri, 0);
DEF_BUCKET(2, BERasterizeLargeTri, 0);
DEF_BUCKET(3, BETriangleSetup, 0);
DEF_BUCKET(3, BEStepSetup, 0);
DEF_BUCKET(3, BECullZeroArea, 0);
DEF_BUCKET(3, BEEmptyTriangle, 0);
DEF_BUCKET(3, BETrivialAccept, 0);
DEF_BUCKET(3, BETrivialReject, 0);
DEF_BUCKET(3, BERasterizePartial, 0);
DEF_BUCKET(3, BEPixelShader, 0);
DEF_BUCKET(4, BEPixelShaderFunc, 0);
DEF_BUCKET(4, BilinearSample, 0);
DEF_BUCKET(2, BEStoreTiles, 1);
DEF_BUCKET(2, BEProcessCopy, 0);
DEF_BUCKET(0, WorkerWaitForThreadEvent, 0);
