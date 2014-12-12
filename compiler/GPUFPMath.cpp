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

#if defined(_WIN32)
#pragma warning(disable : 4146 4267)
#pragma warning(disable : 4800)
#endif

#include "GPUFPMath.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace
{

struct GPUFPMath : public FunctionPass
{
    static char ID;

    GPUFPMath(bool safish = true)
        : FunctionPass(ID), mSafishTransformsOnly(safish)
    {
    }

    // Most of the optimizations we're interested are captured by Inst Combine & Simplify.
    // We're going to add just a few (very unsafe) optimizations at the IR level.
    virtual bool runOnFunction(Function &F)
    {
        bool mutatedFunction = false;

        auto &BBs = F.getBasicBlockList();

        for (auto bbtr = BBs.begin(), bbend = BBs.end(); bbtr != bbend; ++bbtr)
        {
            SmallVector<CallInst *, 8> fmadToFmul;
            SmallVector<CallInst *, 8> fmadToFadd0;
            SmallVector<CallInst *, 8> fmadToFadd1;

            auto &Instrs = bbtr->getInstList();
            for (auto itr = Instrs.begin(), ind = Instrs.end(); itr != ind; ++itr)
            {
                if (!(itr->isBinaryOp() || itr->getOpcode() == Instruction::Call))
                    continue;

                Value *op0 = itr->getOperand(0);
                Value *op1 = itr->getOperand(1);

                if (!op0->getType()->isFPOrFPVectorTy())
                    continue;
                if (!op1->getType()->isFPOrFPVectorTy())
                    continue;

                Constant *zero = 0;
                Constant *one = 0;
                if (op0->getType()->isVectorTy())
                {
                    zero = ConstantVector::get(std::vector<Constant *>(op0->getType()->getVectorNumElements(), ConstantFP::get(op0->getType()->getVectorElementType(), 0)));
                    one = ConstantVector::get(std::vector<Constant *>(op0->getType()->getVectorNumElements(), ConstantFP::get(op0->getType()->getVectorElementType(), 1)));
                }
                else
                {
                    zero = ConstantFP::get(op0->getType(), 0);
                    one = ConstantFP::get(op0->getType(), 1);
                }

                bool op0Is0 = isa<Constant>(op0) && (cast<Constant>(op0) == zero);
                bool op1Is0 = isa<Constant>(op1) && (cast<Constant>(op1) == zero);
                bool op0Is1 = isa<Constant>(op0) && (cast<Constant>(op0) == one);
                bool op1Is1 = isa<Constant>(op1) && (cast<Constant>(op1) == one);

                switch (itr->getOpcode())
                {
                case Instruction::Call:
                {
                    // 0 * x + b = b
                    // x * 0 + b = b
                    // a * b + 0 = a * b
                    // 1 * x + b = x + b
                    // x * 1 + b = x + b
                    CallInst *call = cast<CallInst>(itr);
                    Function *func = call->getCalledFunction();
                    StringRef name;
                    if (func)
                        name = func->getName();

                    bool mad = (name == "swrc.fmuladd.f32") ||
                               (name == "llvm.fmuladd.v4f32") ||
                               (name == "llvm.fmuladd.v8f32");

                    if (!mad)
                        break;

                    if (op0Is0 || op1Is0)
                    {
                        itr->replaceAllUsesWith(itr->getOperand(2));
                        mutatedFunction = true;
                    }
                    else if (op0Is1)
                    {
                        fmadToFadd1.push_back(call);
                    }
                    else if (op1Is1)
                    {
                        fmadToFadd0.push_back(call);
                    }
                    else
                    {
                        Value *op2 = itr->getOperand(2);
                        if (!op2->getType()->isFPOrFPVectorTy())
                            continue;
                        bool op2Is0 = isa<Constant>(op2) && (cast<Constant>(op2) == zero);
                        if (op2Is0)
                        {
                            fmadToFmul.push_back(call);
                        }
                    }
                    break;
                }
                case Instruction::FSub:
                    if (!mSafishTransformsOnly)
                    {
                        if (op0 == op1)
                        {
                            // X - X => 0
                            itr->replaceAllUsesWith(zero);
                            mutatedFunction = true;
                        }
                    }
                    break;
                case Instruction::FAdd:
                    if (op0Is0 || op1Is0)
                    {
                        // X + 0 => X
                        // 0 + X => X
                        itr->replaceAllUsesWith(op0Is0 ? op1 : op0);
                        mutatedFunction = true;
                    }
                    break;
                case Instruction::FMul:
                    if (!mSafishTransformsOnly)
                    {
                        if (op0Is0 || op1Is0)
                        {
                            // X * 0 => 0
                            // 0 * X => 0
                            itr->replaceAllUsesWith(zero);
                            mutatedFunction = true;
                        }
                    }

                    if (op0Is1 || op1Is1)
                    {
                        // X * 1 => X
                        // 1 * X => X
                        itr->replaceAllUsesWith(op0Is1 ? op1 : op0);
                        mutatedFunction = true;
                    }
                    break;
                case Instruction::FDiv:
                    if (!mSafishTransformsOnly)
                    {
                        if (op0Is0)
                        {
                            // 0 / X => 0
                            itr->replaceAllUsesWith(zero);
                            mutatedFunction = true;
                        }
                    }

                    if (op1Is1)
                    {
                        // X / 1 => X
                        itr->replaceAllUsesWith(op0);
                        mutatedFunction = true;
                    }
                    break;
                default:
                    break;
                }
            }

            for (size_t i = 0; i < fmadToFmul.size(); i++)
            {
                CallInst *call = fmadToFmul[i];
                BinaryOperator *fmul = BinaryOperator::Create(Instruction::FMul, call->getOperand(0), call->getOperand(1));
                ReplaceInstWithInst(call, fmul);
                mutatedFunction = true;
            }
            for (size_t i = 0; i < fmadToFadd0.size(); i++)
            {
                CallInst *call = fmadToFadd0[i];
                BinaryOperator *fadd = BinaryOperator::Create(Instruction::FAdd, call->getOperand(0), call->getOperand(2));
                ReplaceInstWithInst(call, fadd);
                mutatedFunction = true;
            }
            for (size_t i = 0; i < fmadToFadd1.size(); i++)
            {
                CallInst *call = fmadToFadd1[i];
                BinaryOperator *fadd = BinaryOperator::Create(Instruction::FAdd, call->getOperand(1), call->getOperand(2));
                ReplaceInstWithInst(call, fadd);
                mutatedFunction = true;
            }
        }

        return mutatedFunction;
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.setPreservesCFG();
    }

    bool mSafishTransformsOnly;
};
}

char GPUFPMath::ID = 0;
static RegisterPass<GPUFPMath> X("GPUFPMath", "GPU FP Math", false, false);

namespace llvm
{

FunctionPass *createIgnoreTheAnnoyingPartsOfTheIEEE754SpecPass(bool safish)
{
    return new GPUFPMath(safish);
}

} // end namespace llvm
