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

#ifndef LLVM_SIMTIZE_H
#define LLVM_SIMTIZE_H

#include "llvm/IR/Function.h"

#include "llvm/Config/llvm-config.h"
#if (LLVM_VERSION_MAJOR == 3) && (LLVM_VERSION_MINOR >= 5)
#include "llvm/IR/ValueMap.h"
#else
#include "llvm/ADT/ValueMap.h"
#endif

namespace llvm
{

Function *CreateSimtizedFunction(Function *scalarFunction, uint32_t numLanes, ValueMap<Function *, Function *> const &FMap);
Type *SimtizeType(LLVMContext *context, uint32_t numLanes, Type *ty);
const char *SimtizeMetadataName();
const char *SimtizeMetadataSimtize();
const char *SimtizeMetadataDont();
const char *SimtizeMetadataBroadcast();
}

#endif //LLVM_SIMTIZE_H
