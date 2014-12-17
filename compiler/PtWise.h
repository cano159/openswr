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

#ifndef LLVM_PTWISE_H
#define LLVM_PTWISE_H

#ifdef WIN32
#pragma warning(disable : 4146 4244 4267 4800 4996)
#endif

#include "llvm/IR/IRBuilder.h"
#include "llvm/Config/llvm-config.h"

namespace llvm
{

template <bool preserveNames = true, typename T = ConstantFolder,
          typename Inserter = IRBuilderDefaultInserter<preserveNames>>
class PtWiseBuilder
{
    typedef IRBuilder<preserveNames, T, Inserter> IRBT;
    IRBT &Builder;

    bool PtWiseType(Type *Ty) const
    {
        if (Ty->isFPOrFPVectorTy() || Ty->isIntOrIntVectorTy())
            return true;
        if (Ty->isArrayTy())
            return PtWiseType(Ty->getArrayElementType());
        if (Ty->isStructTy())
        {
            for (uint32_t eltNum = 0, NumElts = Ty->getStructNumElements(); eltNum < NumElts; ++eltNum)
            {
                if (!PtWiseType(Ty->getStructElementType(eltNum)))
                    return false;
            }
            return true;
        }
        return false;
    }

    Type *PtWiseCmpType(Type *Ty) const
    {
#if (LLVM_VERSION_MAJOR == 3) && (LLVM_VERSION_MINOR >= 5)
        if (Ty->isSingleValueType())
#else
        if (Ty->isPrimitiveType())
#endif
            return Builder.getInt1Ty();
        if (Ty->isArrayTy())
            return ArrayType::get(PtWiseCmpType(Ty->getArrayElementType()), Ty->getArrayNumElements());
        if (Ty->isStructTy())
        {
            StructType *sTy = cast<StructType>(Ty);
            std::vector<Type *> fieldTys(Ty->getStructNumElements());
            for (uint32_t eltNum = 0, NumElts = Ty->getStructNumElements(); eltNum < NumElts; ++eltNum)
            {
                fieldTys[eltNum] = PtWiseCmpType(Ty->getStructElementType(eltNum));
            }
            return StructType::get(Builder.getContext(), fieldTys, sTy->isPacked());
        }
        assert(false && "Not a valid comparable type.");
        return 0;
    }

public:
    PtWiseBuilder(IRBT &builder)
        : Builder(builder)
    {
    }

    Constant *CreateMatchingConstant(Constant *constant, Type *ptWiseTy)
    {
        if (constant->getType() == ptWiseTy)
            return constant;
        if (ptWiseTy->isArrayTy())
        {
            return ConstantArray::get(cast<ArrayType>(ptWiseTy), std::vector<Constant *>(ptWiseTy->getArrayNumElements(), constant));
        }
        if (ptWiseTy->isStructTy())
        {
            return ConstantStruct::get(cast<StructType>(ptWiseTy), std::vector<Constant *>(ptWiseTy->getStructNumElements(), constant));
        }
        return 0;
    }

    Value *CreateBinOp(Instruction::BinaryOps Opc,
                       Value *LHS, Value *RHS, const Twine &Name = "")
    {
        assert((LHS->getType() == RHS->getType()) && "Type mismatch!");
        assert(PtWiseType(LHS->getType()) && "Not a PtWise type!");
        Type *Ty = LHS->getType();
        if (Ty->isFPOrFPVectorTy() || Ty->isIntOrIntVectorTy())
            return Builder.CreateBinOp(Opc, LHS, RHS, Name);
        uint32_t NumElts = Ty->isArrayTy() ? Ty->getArrayNumElements() : Ty->getStructNumElements();
        Value *result = UndefValue::get(Ty);
        for (uint32_t elt = 0; elt < NumElts; ++elt)
        {
            Value *lhsI = Builder.CreateExtractValue(LHS, std::vector<uint32_t>(1, elt), Name);
            Value *rhsI = Builder.CreateExtractValue(RHS, std::vector<uint32_t>(1, elt), Name);
            Value *resI = CreateBinOp(Opc, lhsI, rhsI, Name);
            result = Builder.CreateInsertValue(result, resI, std::vector<uint32_t>(1, elt), Name);
        }
        return result;
    }

    template <typename Fn>
    Value *CreateFPBinOp(Fn fn, Value *LHS, Value *RHS, const Twine &Name = "",
                         MDNode * FPMathTag = 0)
    {
        assert((LHS->getType() == RHS->getType()) && "Type mismatch!");
        assert(PtWiseType(LHS->getType()) && "Not a PtWise type!");
        Type *Ty = LHS->getType();
        if (Ty->isFPOrFPVectorTy())
            return ((Builder).*(fn))(LHS, RHS, Name, FPMathTag);
        uint32_t NumElts = Ty->isArrayTy() ? Ty->getArrayNumElements() : Ty->getStructNumElements();
        Value *result = UndefValue::get(Ty);
        for (uint32_t elt = 0; elt < NumElts; ++elt)
        {
            Value *lhsI = Builder.CreateExtractValue(LHS, std::vector<uint32_t>(1, elt), Name);
            Value *rhsI = Builder.CreateExtractValue(RHS, std::vector<uint32_t>(1, elt), Name);
            Value *resI = CreateFPBinOp(fn, lhsI, rhsI, Name, FPMathTag);
            result = Builder.CreateInsertValue(result, resI, std::vector<uint32_t>(1, elt), Name);
        }
        return result;
    }

    template <typename Fn>
    Value *CreateFPCmpOp(Fn fn, Value *LHS, Value *RHS, const Twine &Name = "")
    {
        assert((LHS->getType() == RHS->getType()) && "Type mismatch!");
        assert(PtWiseType(LHS->getType()) && "Not a PtWise type!");
        Type *Ty = LHS->getType();
        if (Ty->isFPOrFPVectorTy())
            return ((Builder).*(fn))(LHS, RHS, Name);
        uint32_t NumElts = Ty->isArrayTy() ? Ty->getArrayNumElements() : Ty->getStructNumElements();
        Value *result = UndefValue::get(PtWiseCmpType(Ty));
        for (uint32_t elt = 0; elt < NumElts; ++elt)
        {
            Value *lhsI = Builder.CreateExtractValue(LHS, std::vector<uint32_t>(1, elt), Name);
            Value *rhsI = Builder.CreateExtractValue(RHS, std::vector<uint32_t>(1, elt), Name);
            Value *resI = CreateFPCmpOp(fn, lhsI, rhsI, Name);
            result = Builder.CreateInsertValue(result, resI, std::vector<uint32_t>(1, elt), Name);
        }
        return result;
    }

    Value *CreateFAdd(Value *LHS, Value *RHS, const Twine &Name = "",
                      MDNode *FPMathTag = 0)
    {
        return CreateFPBinOp(&IRBT::CreateFAdd, LHS, RHS, Name, FPMathTag);
    }

    Value *CreateFSub(Value *LHS, Value *RHS, const Twine &Name = "",
                      MDNode *FPMathTag = 0)
    {
        return CreateFPBinOp(&IRBT::CreateFSub, LHS, RHS, Name, FPMathTag);
    }

    Value *CreateFMul(Value *LHS, Value *RHS, const Twine &Name = "",
                      MDNode *FPMathTag = 0)
    {
        return CreateFPBinOp(&IRBT::CreateFMul, LHS, RHS, Name, FPMathTag);
    }

    Value *CreateFDiv(Value *LHS, Value *RHS, const Twine &Name = "",
                      MDNode *FPMathTag = 0)
    {
        return CreateFPBinOp(&IRBT::CreateFDiv, LHS, RHS, Name, FPMathTag);
    }

    Value *CreateFRem(Value *LHS, Value *RHS, const Twine &Name = "",
                      MDNode *FPMathTag = 0)
    {
        return CreateFPBinOp(&IRBT::CreateFRem, LHS, RHS, Name, FPMathTag);
    }

    template <typename Fn>
    Value *CreateIntBitOp(Fn fn, Value *LHS, Value *RHS, const Twine &Name = "")
    {
        assert((LHS->getType() == RHS->getType()) && "Type mismatch!");
        assert(PtWiseType(LHS->getType()) && "Not a PtWise type!");
        Type *Ty = LHS->getType();
        if (Ty->isIntOrIntVectorTy())
            return ((Builder).*(fn))(LHS, RHS, Name);
        uint32_t NumElts = Ty->isArrayTy() ? Ty->getArrayNumElements() : Ty->getStructNumElements();
        Value *result = UndefValue::get(Ty);
        for (uint32_t elt = 0; elt < NumElts; ++elt)
        {
            Value *lhsI = Builder.CreateExtractValue(LHS, std::vector<uint32_t>(1, elt), Name);
            Value *rhsI = Builder.CreateExtractValue(RHS, std::vector<uint32_t>(1, elt), Name);
            Value *resI = CreateIntBitOp(fn, lhsI, rhsI, Name);
            result = Builder.CreateInsertValue(result, resI, std::vector<uint32_t>(1, elt), Name);
        }
        return result;
    }

    template <typename Fn>
    Value *CreateIntBinOp(Fn fn, Value *LHS, Value *RHS, const Twine &Name = "",
                          bool HasNUW = false, bool HasNSW = false)
    {
        assert((LHS->getType() == RHS->getType()) && "Type mismatch!");
        assert(PtWiseType(LHS->getType()) && "Not a PtWise type!");
        Type *Ty = LHS->getType();
        if (Ty->isIntOrIntVectorTy())
            return ((Builder).*(fn))(LHS, RHS, Name, HasNUW, HasNSW);
        uint32_t NumElts = Ty->isArrayTy() ? Ty->getArrayNumElements() : Ty->getStructNumElements();
        Value *result = UndefValue::get(Ty);
        for (uint32_t elt = 0; elt < NumElts; ++elt)
        {
            Value *lhsI = Builder.CreateExtractValue(LHS, std::vector<uint32_t>(1, elt), Name);
            Value *rhsI = Builder.CreateExtractValue(RHS, std::vector<uint32_t>(1, elt), Name);
            Value *resI = CreateIntBinOp(fn, lhsI, rhsI, Name, HasNUW, HasNSW);
            result = Builder.CreateInsertValue(result, resI, std::vector<uint32_t>(1, elt), Name);
        }
        return result;
    }

    template <typename Fn>
    Value *CreateIntDivOp(Fn fn, Value *LHS, Value *RHS, const Twine &Name = "", bool isExact = false)
    {
        assert((LHS->getType() == RHS->getType()) && "Type mismatch!");
        assert(PtWiseType(LHS->getType()) && "Not a PtWise type!");
        Type *Ty = LHS->getType();
        if (Ty->isIntOrIntVectorTy())
            return ((Builder).*(fn))(LHS, RHS, Name, isExact);
        uint32_t NumElts = Ty->isArrayTy() ? Ty->getArrayNumElements() : Ty->getStructNumElements();
        Value *result = UndefValue::get(Ty);
        for (uint32_t elt = 0; elt < NumElts; ++elt)
        {
            Value *lhsI = Builder.CreateExtractValue(LHS, std::vector<uint32_t>(1, elt), Name);
            Value *rhsI = Builder.CreateExtractValue(RHS, std::vector<uint32_t>(1, elt), Name);
            Value *resI = CreateIntDivOp(fn, lhsI, rhsI, Name, isExact);
            result = Builder.CreateInsertValue(result, resI, std::vector<uint32_t>(1, elt), Name);
        }
        return result;
    }

    Value *CreateSelect(Value *C, Value *True, Value *False, const Twine &Name = "")
    {
        assert((True->getType() == False->getType()) && "Type mismatch!");
        assert(PtWiseType(True->getType()) && "Not a PtWise type!");
        Type *Ty = True->getType();
        if (Ty->isFPOrFPVectorTy())
            return Builder.CreateSelect(C, True, False, Name);
        uint32_t NumElts = Ty->isArrayTy() ? Ty->getArrayNumElements() : Ty->getStructNumElements();
        Value *result = UndefValue::get(Ty);
        for (uint32_t elt = 0; elt < NumElts; ++elt)
        {
            Value *lhsI = Builder.CreateExtractValue(True, std::vector<uint32_t>(1, elt), Name);
            Value *rhsI = Builder.CreateExtractValue(False, std::vector<uint32_t>(1, elt), Name);
            Value *cndI = Builder.CreateExtractValue(C, std::vector<uint32_t>(1, elt), Name);
            Value *resI = Builder.CreateSelect(cndI, lhsI, rhsI, Name);
            result = Builder.CreateInsertValue(result, resI, std::vector<uint32_t>(1, elt), Name);
        }
        return result;
    }

    Value *CreateFCmpOEQ(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpOEQ, LHS, RHS, Name);
    }

    Value *CreateFCmpOGT(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpOGT, LHS, RHS, Name);
    }

    Value *CreateFCmpOGE(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpOGE, LHS, RHS, Name);
    }

    Value *CreateFCmpOLT(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpOLT, LHS, RHS, Name);
    }

    Value *CreateFCmpOLE(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpOLE, LHS, RHS, Name);
    }

    Value *CreateFCmpONE(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpONE, LHS, RHS, Name);
    }

    Value *CreateFCmpORD(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpORD, LHS, RHS, Name);
    }

    Value *CreateFCmpUNO(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpONO, LHS, RHS, Name);
    }

    Value *CreateFCmpUEQ(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpUEQ, LHS, RHS, Name);
    }

    Value *CreateFCmpUGT(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpUGT, LHS, RHS, Name);
    }

    Value *CreateFCmpUGE(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpUGE, LHS, RHS, Name);
    }

    Value *CreateFCmpULT(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpULT, LHS, RHS, Name);
    }

    Value *CreateFCmpULE(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpULE, LHS, RHS, Name);
    }

    Value *CreateFCmpUNE(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateFPCmpOp(&IRBT::CreateFCmpUNE, LHS, RHS, Name);
    }

    Value *CreateAdd(Value *LHS, Value *RHS, const Twine &Name = "",
                     bool HasNUW = false, bool HasNSW = false)
    {
        return CreateIntBinOp(&IRBT::CreateAdd, LHS, RHS, Name, HasNUW, HasNSW);
    }

    Value *CreateNSWAdd(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateAdd, LHS, RHS, Name, false, true);
    }

    Value *CreateNUWAdd(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateAdd, LHS, RHS, Name, true, false);
    }

    Value *CreateSub(Value *LHS, Value *RHS, const Twine &Name = "",
                     bool HasNUW = false, bool HasNSW = false)
    {
        return CreateIntBinOp(&IRBT::CreateSub, LHS, RHS, Name, HasNUW, HasNSW);
    }

    Value *CreateNSWSub(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateSub, LHS, RHS, Name, false, true);
    }

    Value *CreateNUWSub(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateSub, LHS, RHS, Name, true, false);
    }

    Value *CreateMul(Value *LHS, Value *RHS, const Twine &Name = "",
                     bool HasNUW = false, bool HasNSW = false)
    {
        return CreateIntBinOp(&IRBT::CreateMul, LHS, RHS, Name, HasNUW, HasNSW);
    }

    Value *CreateNSWMul(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateMul, LHS, RHS, Name, false, true);
    }

    Value *CreateNUWMul(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateMul, LHS, RHS, Name, true, false);
    }

    Value *CreateUDiv(Value *LHS, Value *RHS, const Twine &Name = "", bool isExact = false)
    {
        return CreateIntDivOp(&IRBT::CreateUDiv, LHS, RHS, Name, isExact);
    }

    Value *CreateExactUDiv(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntExactDivOp(&IRBT::CreateUDiv, LHS, RHS, Name, true);
    }

    Value *CreateSDiv(Value *LHS, Value *RHS, const Twine &Name = "", bool isExact = false)
    {
        return CreateIntDivOp(&IRBT::CreateSDiv, LHS, RHS, Name, isExact);
    }

    Value *CreateExactSDiv(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntExactDivOp(&IRBT::CreateSDiv, LHS, RHS, Name, true);
    }

    Value *CreateURem(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntDivOp(&IRBT::CreateURem, LHS, RHS, Name);
    }

    Value *CreateSRem(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntDivOp(&IRBT::CreateSRem, LHS, RHS, Name);
    }

    Value *CreateShl(Value *LHS, Value *RHS, const Twine &Name = "",
                     bool HasNUW = false, bool HasNSW = false)
    {
        return CreateIntBinOp(&IRBT::CreateShl, LHS, RHS, Name, HasNUW, HasNSW);
    }

    Value *CreateNSWShl(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateShl, LHS, RHS, Name, false, true);
    }

    Value *CreateNUWShl(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateShl, LHS, RHS, Name, true, false);
    }

    Value *CreateLShr(Value *LHS, Value *RHS, const Twine &Name = "",
                      bool HasNUW = false, bool HasNSW = false)
    {
        return CreateIntBinOp(&IRBT::CreateLShr, LHS, RHS, Name, HasNUW, HasNSW);
    }

    Value *CreateNSWLShr(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateLShr, LHS, RHS, Name, false, true);
    }

    Value *CreateNUWLShr(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateLShr, LHS, RHS, Name, true, false);
    }

    Value *CreateAShr(Value *LHS, Value *RHS, const Twine &Name = "",
                      bool HasNUW = false, bool HasNSW = false)
    {
        return CreateIntBinOp(&IRBT::CreateAShr, LHS, RHS, Name, HasNUW, HasNSW);
    }

    Value *CreateNSWAShr(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateAShr, LHS, RHS, Name, false, true);
    }

    Value *CreateNUWAShr(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBinOp(&IRBT::CreateAShr, LHS, RHS, Name, true, false);
    }

    Value *CreateAnd(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBitOp(&IRBT::CreateAnd, LHS, RHS, Name);
    }

    Value *CreateOr(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBitOp(&IRBT::CreateOr, LHS, RHS, Name);
    }

    Value *CreateXor(Value *LHS, Value *RHS, const Twine &Name = "")
    {
        return CreateIntBitOp(&IRBT::CreateXor, LHS, RHS, Name);
    }

    Value *CreateCall(Value *Callee, ArrayRef<Value *> Args,
                      const Twine &Name = "")
    {
        // All the types are expected to have the same "macro" structure.
        Type *Ty = Args[0]->getType();
        if (Ty->isIntOrIntVectorTy() || Ty->isFPOrFPVectorTy())
            return Builder.CreateCall(Callee, Args, Name);
        uint32_t NumElts = Ty->isArrayTy() ? Ty->getArrayNumElements() : Ty->getStructNumElements();
        Value *result = UndefValue::get(Ty);
        for (uint32_t elt = 0; elt < NumElts; ++elt)
        {
            std::vector<Value *> SubArgs;
            for (std::size_t i = 0, N = Args.size(); i < N; ++i)
                SubArgs.push_back(Builder.CreateExtractValue(Args[i], std::vector<uint32_t>(1, elt), Name));
            Value *resI = CreateCall(Callee, SubArgs, Name);
            result = Builder.CreateInsertValue(result, resI, std::vector<uint32_t>(1, elt), Name);
        }
        return result;
    }
};
}

#endif //LLVM_PTWISE_H
