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

#include "oglglobals.h"
#include "gldd.h"

#include <cstring>

namespace OGL
{

struct Global
{
    // Device dependent state.
    DDProcTable mddProcTable;
    DDHANDLE mhDD;
};

void Initialize(Global &global)
{
    DDInitProcTable(global.mddProcTable);
    global.mhDD = global.mddProcTable.pfnCreateContext();
}

static bool gGlobalsInitialized = false;
static Global gGlobal;

Global &GetGlobals()
{
    if (!gGlobalsInitialized)
    {
        Initialize(gGlobal);
        gGlobalsInitialized = true;
    }

    return gGlobal;
}

DDProcTable &GetDDProcTable()
{
    return GetGlobals().mddProcTable;
}

DDHANDLE GetDDHandle()
{
    return GetGlobals().mhDD;
}

void DestroyGlobals()
{
    memset(&gGlobal, 0, sizeof(Global));
    gGlobalsInitialized = false;
}
}
