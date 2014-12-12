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

#ifdef WIN32
#pragma warning(disable : 4146 4244 4267 4715 4800 4996)
#endif

#include "Simtize.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <vector>

using namespace llvm;

class Simtize
{
    LLVMContext *Context;
    llvm::Module *Module;
    Function *ScalarFunction;

    uint32_t NumLanes;

    FunctionType *NewFunctionType;
    Function *NewFunction;
    std::vector<PHINode *> PHINodes;
    ValueMap<Value *, Value *> ValuesMap;
    ValueMap<Function *, Function *> FunctionMap;
    ValueMap<BasicBlock *, BasicBlock *> BlockMap;
    BasicBlock *CurrentBlock;

public:
    Simtize(LLVMContext *context = 0, uint32_t numLanes = 0)
    {
        Context = context;
        NumLanes = numLanes;
    }
    Type *VectorizeType(Type *Ty)
    {
        assert(!Ty->isVectorTy() && "Vector types are unsupported for simtization.");
        if (Ty->isIntegerTy())
            return VectorType::get(Ty, NumLanes);
        if (Ty->isFloatingPointTy())
            return VectorType::get(Ty, NumLanes);
        if (Ty->isArrayTy())
            return ArrayType::get(VectorizeType(Ty->getArrayElementType()), Ty->getArrayNumElements());
        if (Ty->isStructTy())
        {
            std::vector<Type *> FieldTys;
            for (std::size_t II = 0, N = Ty->getStructNumElements(); II < N; ++II)
            {
                FieldTys.push_back(VectorizeType(Ty->getStructElementType(II)));
            }
            return StructType::get(*Context, FieldTys, cast<StructType>(Ty)->isPacked());
        }
        if (Ty->isPointerTy())
        {
            return PointerType::get(VectorizeType(Ty->getPointerElementType()), cast<PointerType>(Ty)->getAddressSpace());
        }
        if (Ty->isFunctionTy())
        {
            FunctionType *fTy = cast<FunctionType>(Ty);
            Type *returnType = VectorizeType(fTy->getReturnType());
            std::vector<Type *> ArgTys;
            for (std::size_t II = 0, N = fTy->getNumParams(); II < N; ++II)
            {
                ArgTys.push_back(VectorizeType(fTy->getParamType(II)));
            }
            return FunctionType::get(returnType, ArgTys, fTy->isVarArg());
        }
        if (Ty->isMetadataTy() || Ty->isLabelTy() || Ty->isVoidTy())
            return Ty;
        assert(false && "No clue what this is.");
        return NULL;
    }
    Value *LookupValue(Value *value)
    {
        if (isa<UndefValue>(value))
        {
            return UndefValue::get(VectorizeType(value->getType()));
        }
        if (isa<Constant>(value))
        {
            if (isa<ConstantFP>(value))
            {
                return ConstantVector::get(std::vector<Constant *>(NumLanes, cast<Constant>(value)));
            }
            else if (isa<ConstantInt>(value))
            {
                return ConstantVector::get(std::vector<Constant *>(NumLanes, cast<Constant>(value)));
            }
            else if (isa<ConstantStruct>(value))
            {
                std::vector<Constant *> fields;
                auto S = cast<ConstantStruct>(value);
                for (uint32_t FF = 0, FN = S->getNumOperands(); FF != FN; ++FF)
                {
                    fields.push_back(cast<Constant>(LookupValue(S->getOperand(FF))));
                }
                return ConstantStruct::get(cast<StructType>(VectorizeType(value->getType())), fields);
            }
            assert(false && "Cannot handle that type of constant!");
        }
        return ValuesMap[value];
    }
    Value *BroadcastValue(Value *value)
    {
        Type *Ty = value->getType();
        if (Ty->isIntegerTy() || Ty->isFloatingPointTy())
        {
            Value *U = UndefValue::get(VectorType::get(Ty, NumLanes));
            Value *mask = ConstantVector::get(std::vector<Constant *>(NumLanes, ConstantInt::get(IntegerType::get(*Context, 32), 0)));
            Value *ins = InsertElementInst::Create(U, value, ConstantInt::get(IntegerType::get(*Context, 32), 0), "", CurrentBlock);
            return new ShuffleVectorInst(ins, U, mask, "", CurrentBlock);
        }
        else if (Ty->isAggregateType())
        {
            std::vector<Value *> elts;
            Value *U = UndefValue::get(VectorizeType(Ty));
            for (std::size_t EE = 0, N = Ty->isStructTy() ? Ty->getStructNumElements() : Ty->getArrayNumElements(); EE < N; ++EE)
            {
                Value *elt = ExtractValueInst::Create(value, std::vector<uint32_t>(1, EE), "", CurrentBlock);
                Value *bcElt = BroadcastValue(elt);
                U = InsertValueInst::Create(U, bcElt, std::vector<uint32_t>(1, EE), "", CurrentBlock);
            }
            return U;
        }
        assert(false && "Other types are not supported.");
        return 0;
    }
    void StoreValue(Value *oldValue, Value *newValue)
    {
        ValuesMap[oldValue] = newValue;
    }
    Function *Create(Function *ScFn, uint32_t NL, ValueMap<Function *, Function *> const &FMap)
    {
        // Initialize state.
        ScalarFunction = ScFn;
        Module = ScalarFunction->getParent();
        Context = &ScalarFunction->getContext();
        NumLanes = NL;
        for (auto FF = FMap.begin(), FE = FMap.end(); FF != FE; ++FF)
        {
            FunctionMap[FF->first] = FF->second;
        }

        // Convert the function's type.
        NewFunctionType = cast<FunctionType>(VectorizeType(ScalarFunction->getType()->getPointerElementType()));

        // Create the new function.
        NewFunction = Function::Create(NewFunctionType, ScalarFunction->getLinkage(), ScalarFunction->getName(), Module);
        NewFunction->takeName(ScalarFunction);

        FunctionMap[ScalarFunction] = NewFunction;

        auto &oldArgs = ScalarFunction->getArgumentList();
        auto &newArgs = NewFunction->getArgumentList();

        for (auto OA = oldArgs.begin(), OAE = oldArgs.end(), NA = newArgs.begin(); OA != OAE; ++OA, ++NA)
        {
            ValuesMap[OA] = NA;
            NA->setName(OA->getName());
        }

        PHINodes.clear();

        // Walk the blocks.
        auto &blocks = ScalarFunction->getBasicBlockList();
        for (auto BB = blocks.begin(), BD = blocks.end(); BB != BD; ++BB)
        {
            BasicBlock *block = BasicBlock::Create(*Context, BB->getName(), NewFunction);
            BlockMap[BB] = block;
        }
        for (auto BB = blocks.begin(), BD = blocks.end(); BB != BD; ++BB)
        {
            CurrentBlock = BlockMap[BB];
            auto &instrs = BB->getInstList();
            for (auto II = instrs.begin(), ID = instrs.end(); II != ID; ++II)
            {
                if (II->isBinaryOp())
                {
                    BinaryOperator *binOp = cast<BinaryOperator>(II);
                    StoreValue(II, BinaryOperator::Create(binOp->getOpcode(), LookupValue(binOp->getOperand(0)), LookupValue(binOp->getOperand(1)), binOp->getName(), CurrentBlock));
                }
                else if (isa<CallInst>(II))
                {
                    CallInst *call = cast<CallInst>(II);
                    std::vector<Value *> Args;
                    for (std::size_t AA = 0, AE = call->getNumArgOperands(); AA < AE; ++AA)
                    {
                        Args.push_back(LookupValue(call->getArgOperand(AA)));
                    }
                    assert((FunctionMap.find(call->getCalledFunction()) != FunctionMap.end()) && "Unregistered function!");
                    StoreValue(II, CallInst::Create(FunctionMap[call->getCalledFunction()], Args, "", CurrentBlock));
                }
                else if (isa<AllocaInst>(II))
                {
                    assert(false && "Alloca is unsupported.");
                }
                else if (isa<GetElementPtrInst>(II))
                {
                    GetElementPtrInst *gep = cast<GetElementPtrInst>(II);
                    std::vector<Value *> Indices;
                    for (std::size_t JJ = 0, JE = gep->getNumIndices(); JJ < JE; ++JJ)
                    {
                        //assert(isa<Constant>(gep->getOperand(JJ)) && "Non-constants are not supported.");
                        Indices.push_back(gep->getOperand(JJ + 1)); // Constants, only.
                    }
                    StoreValue(II, GetElementPtrInst::Create(LookupValue(gep->getPointerOperand()), Indices, gep->getName(), CurrentBlock));
                }
                else if (isa<LoadInst>(II))
                {
                    LoadInst *load = cast<LoadInst>(II);
                    bool doBroadcast = false;
                    if (MDNode *md = load->getMetadata(SimtizeMetadataName()))
                    {
                        MDString *ms = cast<MDString>(md->getOperand(0));
                        doBroadcast = ms->getString() == SimtizeMetadataBroadcast();
                    }
                    if (doBroadcast)
                    {
                        LoadInst *nLoad = new LoadInst(LookupValue(load->getPointerOperand()), load->getName(), CurrentBlock);
                        nLoad->setAlignment(load->getAlignment());
                        StoreValue(II, BroadcastValue(nLoad));
                    }
                    else
                    {
                        LoadInst *nLoad = new LoadInst(LookupValue(load->getPointerOperand()), load->getName(), CurrentBlock);
                        nLoad->setAlignment(load->getAlignment());
                        StoreValue(II, nLoad);
                    }
                }
                else if (isa<StoreInst>(II))
                {
                    StoreInst *store = cast<StoreInst>(II);
                    new StoreInst(LookupValue(store->getValueOperand()), LookupValue(store->getPointerOperand()), CurrentBlock);
                }
                else if (isa<BitCastInst>(II))
                {
                    CastInst *cst = cast<CastInst>(II);
                    MDNode *md = cst->getMetadata(SimtizeMetadataName());
                    MDString *ms = cast<MDString>(md->getOperand(0));
                    if (ms->getString() == SimtizeMetadataDont())
                    {
                        StoreValue(II, CastInst::Create(cst->getOpcode(), LookupValue(cst->getOperand(0)), cst->getDestTy(), cst->getName(), CurrentBlock));
                    }
                    else
                    {
                        StoreValue(II, CastInst::Create(cst->getOpcode(), LookupValue(cst->getOperand(0)), VectorizeType(cst->getDestTy()), cst->getName(), CurrentBlock));
                    }
                }
                else if (isa<InsertValueInst>(II))
                {
                    InsertValueInst *ins = cast<InsertValueInst>(II);
                    StoreValue(II, InsertValueInst::Create(LookupValue(ins->getOperand(0)), LookupValue(ins->getOperand(1)), ins->getIndices(), ins->getName(), CurrentBlock));
                }
                else if (isa<ExtractValueInst>(II))
                {
                    ExtractValueInst *ext = cast<ExtractValueInst>(II);
                    StoreValue(II, ExtractValueInst::Create(LookupValue(ext->getOperand(0)), ext->getIndices(), ext->getName(), CurrentBlock));
                }
                else if (isa<SelectInst>(II))
                {
                    SelectInst *sel = cast<SelectInst>(II);
                    StoreValue(II, SelectInst::Create(LookupValue(sel->getCondition()), LookupValue(sel->getTrueValue()), LookupValue(sel->getFalseValue()), "", CurrentBlock));
                }
                else if (isa<FCmpInst>(II))
                {
                    FCmpInst *fcmp = cast<FCmpInst>(II);
                    StoreValue(II, FCmpInst::Create(fcmp->getOpcode(), fcmp->getPredicate(), LookupValue(fcmp->getOperand(0)), LookupValue(fcmp->getOperand(1)), "", CurrentBlock));
                }
                else if (II->isTerminator())
                {
                    if (isa<ReturnInst>(II))
                    {
                        ReturnInst::Create(*Context, CurrentBlock);
                    }
                    else if (isa<BranchInst>(II))
                    {
                        BranchInst *br = cast<BranchInst>(II);
                        assert(br->isUnconditional() && "Branches must be unconditional, for now.");
                        BranchInst::Create(BlockMap[br->getSuccessor(0)], CurrentBlock);
                    }
                    else
                    {
                        assert(false && "Unsupported terminator (indirectbr, switch, etc.)");
                    }
                }
                else
                {
                    assert(false && "That instruction is unhandled.");
                }
            }
        }

        return NewFunction;
    }
};

namespace llvm
{

Function *CreateSimtizedFunction(Function *scalarFunction, uint32_t numLanes, ValueMap<Function *, Function *> const &FMap)
{
    Simtize S;
    return S.Create(scalarFunction, numLanes, FMap);
}

Type *SimtizeType(LLVMContext *context, uint32_t numLanes, Type *ty)
{
    Simtize S(context, numLanes);
    return S.VectorizeType(ty);
}

const char *SimtizeMetadataName()
{
    return "simtize-md";
}

const char *SimtizeMetadataSimtize()
{
    return "simtize";
}

const char *SimtizeMetadataDont()
{
    return "dont-simtize";
}

const char *SimtizeMetadataBroadcast()
{
    return "broadcast";
}

} // end namespace llvm
