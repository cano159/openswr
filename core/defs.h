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

#ifndef __SWR_DEFS_H__
#define __SWR_DEFS_H__

// shared defines between public API and swr core

#define CLEAR_NONE 0
#define CLEAR_COLOR (1 << 0)
#define CLEAR_DEPTH (1 << 1)

enum DRIVER_TYPE
{
    DX,
    GL
};

#endif //__SWR_DEFS_H__
