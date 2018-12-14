//===-- lib/CodeGen/GlobalISel/Combiner.cpp -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file constains common code to combine machine functions at generic
// level.
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/GlobalISel/Combiner.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/CodeGen/GlobalISel/CombinerInfo.h"
#include "llvm/CodeGen/GlobalISel/GISelChangeObserver.h"
#include "llvm/CodeGen/GlobalISel/GISelWorkList.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/GlobalISel/Utils.h"
#include "llvm/CodeGen/MachineOptimizationRemarkEmitter.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "gi-combiner"

using namespace llvm;

namespace {
/// This class acts as the glue the joins the CombinerHelper to the overall
/// Combine algorithm. The CombinerHelper is intended to report the
/// modifications it makes to the MIR to the GISelChangeObserver and the
/// observer subclass will act on these events. In this case, instruction
/// erasure will cancel any future visits to the erased instruction and
/// instruction creation will schedule that instruction for a future visit.
/// Other Combiner implementations may require more complex behaviour from
/// their GISelChangeObserver subclass.
class WorkListMaintainer : public GISelChangeObserver,
                           public MachineFunction::Delegate {
  using WorkListTy = GISelWorkList<512>;
  MachineFunction &MF;
  WorkListTy &WorkList;
  /// The instructions that have been created but we want to report once they
  /// have their operands. This is only maintained if debug output is requested.
  SmallPtrSet<const MachineInstr *, 4> CreatedInstrs;

public:
  WorkListMaintainer(MachineFunction &MF, WorkListTy &WorkList)
      : GISelChangeObserver(), MF(MF), WorkList(WorkList) {
    MF.setDelegate(this);
  }
  virtual ~WorkListMaintainer() {
    MF.resetDelegate(this);
  }

  void erasingInstr(const MachineInstr &MI) override {
    LLVM_DEBUG(dbgs() << "Erased: " << MI << "\n");
    WorkList.remove(&MI);
  }
  void createdInstr(const MachineInstr &MI) override {
    LLVM_DEBUG(dbgs() << "Creating: " << MI << "\n");
    WorkList.insert(&MI);
    LLVM_DEBUG(CreatedInstrs.insert(&MI));
  }
  void changingInstr(const MachineInstr &MI) override {
    LLVM_DEBUG(dbgs() << "Changing: " << MI << "\n");
    WorkList.insert(&MI);
  }
  void changedInstr(const MachineInstr &MI) override {
    LLVM_DEBUG(dbgs() << "Changed: " << MI << "\n");
    WorkList.insert(&MI);
  }

  void reportFullyCreatedInstrs() {
    LLVM_DEBUG(for (const auto *MI
                    : CreatedInstrs) {
      dbgs() << "Created: ";
      MI->print(dbgs());
    });
    LLVM_DEBUG(CreatedInstrs.clear());
  }

  void MF_HandleInsertion(const MachineInstr &MI) override {
    createdInstr(MI);
  }
  void MF_HandleRemoval(const MachineInstr &MI) override {
    erasingInstr(MI);
  }
};
}

Combiner::Combiner(CombinerInfo &Info, const TargetPassConfig *TPC)
    : CInfo(Info), TPC(TPC) {
  (void)this->TPC; // FIXME: Remove when used.
}

bool Combiner::combineMachineInstrs(MachineFunction &MF) {
  // If the ISel pipeline failed, do not bother running this pass.
  // FIXME: Should this be here or in individual combiner passes.
  if (MF.getProperties().hasProperty(
          MachineFunctionProperties::Property::FailedISel))
    return false;

  MRI = &MF.getRegInfo();
  Builder.setMF(MF);

  LLVM_DEBUG(dbgs() << "Generic MI Combiner for: " << MF.getName() << '\n');

  MachineOptimizationRemarkEmitter MORE(MF, /*MBFI=*/nullptr);

  bool MFChanged = false;
  bool Changed;

  do {
    // Collect all instructions. Do a post order traversal for basic blocks and
    // insert with list bottom up, so while we pop_back_val, we'll traverse top
    // down RPOT.
    Changed = false;
    GISelWorkList<512> WorkList(&MF);
    WorkListMaintainer Observer(MF, WorkList);
    for (MachineBasicBlock *MBB : post_order(&MF)) {
      if (MBB->empty())
        continue;
      for (auto MII = MBB->rbegin(), MIE = MBB->rend(); MII != MIE;) {
        MachineInstr *CurMI = &*MII;
        ++MII;
        // Erase dead insts before even adding to the list.
        if (isTriviallyDead(*CurMI, *MRI)) {
          LLVM_DEBUG(dbgs() << *CurMI << "Is dead; erasing.\n");
          CurMI->eraseFromParentAndMarkDBGValuesForRemoval();
          continue;
        }
        WorkList.insert(CurMI);
      }
    }
    // Main Loop. Process the instructions here.
    while (!WorkList.empty()) {
      MachineInstr *CurrInst = WorkList.pop_back_val();
      LLVM_DEBUG(dbgs() << "\nTry combining " << *CurrInst;);
      Changed |= CInfo.combine(Observer, *CurrInst, Builder);
      Observer.reportFullyCreatedInstrs();
    }
    MFChanged |= Changed;
  } while (Changed);

  return MFChanged;
}
