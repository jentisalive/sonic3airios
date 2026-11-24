#include <rmxbase.h>
#include "lemon/program/Module.h"
#include "lemon/program/Program.h"
#include "lemon/program/function/ScriptFunction.h"
#include "lemon/runtime/Runtime.h"
#include "lemon/runtime/RuntimeFunction.h"
#include <cassert>
#include <iostream>

namespace {
std::vector<uint8> save(lemon::Runtime& runtime) {
    std::vector<uint8> bytes;
    VectorBinarySerializer writer(false, bytes);
    assert(runtime.serializeState(writer));
    return bytes;
}
bool load(lemon::Runtime& runtime, const std::vector<uint8>& bytes) {
    VectorBinarySerializer reader(true, bytes);
    std::string error;
    return runtime.serializeState(reader, &error);
}
void put32(std::vector<uint8>& b, size_t p, uint32 value) {
    for (size_t i=0; i<4; ++i) b.at(p+i) = uint8(value >> (8*i));
}
lemon::ScriptFunction& function(lemon::Module& module, const char* name, size_t count=6) {
    auto& result=module.addScriptFunction(name, &lemon::PredefinedDataTypes::VOID, {});
    result.mOpcodes.resize(count);
    for(auto& op:result.mOpcodes) op.mType=lemon::Opcode::Type::NOP;
    result.mOpcodes.back().mType=lemon::Opcode::Type::RETURN;
    return result;
}
void seek(lemon::Runtime& runtime, size_t pc) {
    auto& state=const_cast<lemon::ControlFlow::State&>(runtime.getMainControlFlow().getState());
    state.mProgramCounter=state.mRuntimeFunction->translateToRuntimeProgramCounter(pc);
}
std::vector<uint8> legacy(const lemon::ScriptFunction& fn, uint32 pc) {
    std::vector<uint8> bytes; VectorBinarySerializer w(false,bytes);
    w.write<uint32>(0x7c4e4d4c); w.write<uint16>(1); w.write<uint32>(1);
    w.write(fn.getName().getString()); w.write(fn.getSignatureHash());
    w.write(pc); w.write<uint32>(0); w.write<uint32>(0); w.write<uint32>(0);
    return bytes;
}
}
int main() {
    lemon::Module base("base"), mod("override");
    auto& underlying=function(base,"step");
    auto& overriding=function(mod,"step",4);
    auto& parent=function(base,"parent");
    parent.mOpcodes[0].mType=lemon::Opcode::Type::CALL;
    parent.mOpcodes[0].mParameter=underlying.getNameAndSignatureHash();
    parent.mOpcodes[0].mDataType=lemon::BaseType::VOID;
    auto& global=base.addGlobalVariable("snapshotGlobal", &lemon::PredefinedDataTypes::INT_64);
    lemon::Program program; program.addModule(base); program.addModule(mod);
    lemon::Runtime runtime; runtime.setProgram(program);
    // An implicit engine hook calls a function while a caller is suspended at
    // another PC. Legacy best-match fixups incorrectly relocate that caller.
    runtime.callRuntimeFunction(*runtime.getRuntimeFunction(parent),0); seek(runtime,4);
    runtime.callRuntimeFunction(*runtime.getRuntimeFunction(underlying),1); seek(runtime,2);
    *runtime.accessGlobalVariableValue(global)=0x123456789abcdef;
    runtime.getSelectedControlFlowMutable().pushValueStack<int64>(-987654321);
    const auto snapshot=save(runtime);
    *runtime.accessGlobalVariableValue(global)=42;
    runtime.getSelectedControlFlowMutable().writeValueStack<int64>(-1,123);
    assert(load(runtime,snapshot));
    assert(*runtime.accessGlobalVariableValue(global)==0x123456789abcdef);
    assert(runtime.getSelectedControlFlow().readValueStack<int64>(-1)==-987654321);
    const auto& stack=runtime.getMainControlFlow().getCallStack();
    assert(stack.count==2);
    assert(stack[0].mRuntimeFunction->mFunction==&parent);
    assert(stack[0].mRuntimeFunction->translateFromRuntimeProgramCounter(stack[0].mProgramCounter)==4);
    assert(stack[1].mRuntimeFunction->mFunction==&underlying);
    assert(stack[1].mBaseCallIndex==1);
    assert(stack[1].mRuntimeFunction->translateFromRuntimeProgramCounter(stack[1].mProgramCounter)==2);
    assert(save(runtime)==snapshot);
    // Truncation is rejected without destroying the currently running VM.
    for(size_t n=0;n<snapshot.size();++n) {
        std::vector<uint8> b(snapshot.begin(),snapshot.begin()+n);
        assert(!load(runtime,b)); assert(save(runtime)==snapshot);
    }
    auto malformed=snapshot;
    put32(malformed,6,0xffffffff); assert(!load(runtime,malformed));
    assert(save(runtime)==snapshot);
    // First frame: string length, name, signature, version index, base depth,
    // bytecode hash, PC and locals count. All offsets use the actual name size.
    const size_t fields=10+4+parent.getName().getString().size();
    for(size_t offset : {fields+4, fields+8, fields+20, fields+24}) {
        malformed=snapshot; put32(malformed,offset,0xffffffff);
        assert(!load(runtime,malformed)); assert(save(runtime)==snapshot);
    }
    malformed=snapshot; malformed[4]=0xff; assert(!load(runtime,malformed));
    underlying.mOpcodes[0].mParameter=123;
    assert(!load(runtime,snapshot)); underlying.mOpcodes[0].mParameter=0;
    assert(save(runtime)==snapshot);
    // Old ambiguous snapshots cannot identify which step body was executing.
    assert(!load(runtime,legacy(underlying,2))); assert(save(runtime)==snapshot);
    // Old unambiguous vanilla frames remain supported by the engine loader.
    assert(load(runtime,legacy(parent,4)));
    assert(runtime.getMainControlFlow().getState().mRuntimeFunction->mFunction==&parent);
    // Growing past a previous CArray allocation must not copy the new count
    // from the old allocation (the old read-then-resize order did that).
    runtime.clearAllControlFlows();
    runtime.callRuntimeFunction(*runtime.getRuntimeFunction(overriding),0);
    const auto small=save(runtime);
    runtime.clearAllControlFlows();
    for(int i=0;i<40;++i) runtime.callRuntimeFunction(*runtime.getRuntimeFunction(underlying),1);
    const auto deep=save(runtime);
    assert(load(runtime,small)); assert(load(runtime,deep));
    assert(runtime.getMainControlFlow().getCallStack().count==40);
    for(size_t i=0;i<40;++i) assert(runtime.getMainControlFlow().getCallStack()[i].mBaseCallIndex==1);
    std::cout << "Runtime state: exact overrides/base calls, indirect caller PCs, bounded atomic loads, legacy rejection and stack growth passed\n";
}
