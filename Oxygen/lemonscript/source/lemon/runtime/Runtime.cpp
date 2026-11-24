/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2026 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "lemon/pch.h"
#include "lemon/runtime/Runtime.h"
#include "lemon/runtime/RuntimeFunction.h"
#include "lemon/runtime/RuntimeOpcodeContext.h"
#include "lemon/program/Program.h"
#include "lemon/program/StringRef.h"
#include "lemon/program/function/NativeFunction.h"


namespace lemon
{

	Runtime::Runtime()
	{
		// Create default control flow
		mControlFlows.push_back(new ControlFlow(*this));
		mSelectedControlFlow = mControlFlows[0];

		mRuntimeOpcodesPool.setPageSize(0x40000);
	}

	Runtime::~Runtime()
	{
		for (ControlFlow* controlFlow : mControlFlows)
		{
			delete controlFlow;
		}
	}

	void Runtime::reset()
	{
		mEncounteredBuildError = false;

		clearAllControlFlows();

		mRuntimeFunctions.clear();
		mRuntimeFunctionsMapped.clear();
		mRuntimeFunctionsBySignature.clear();
		mRuntimeOpcodesPool.clear();
		mStrings.clear();

		if (nullptr != mProgram)
		{
			// Setup runtime functions (all empty at first)
			const std::vector<ScriptFunction*>& scriptFunctions = mProgram->getScriptFunctions();
			mRuntimeFunctions.resize(scriptFunctions.size());
			for (size_t i = 0; i < scriptFunctions.size(); ++i)
			{
				RuntimeFunction& runtimeFunc = mRuntimeFunctions[i];
				ScriptFunction& scriptFunc = *scriptFunctions[i];
				runtimeFunc.mFunction = &scriptFunc;

				// Register in lookup mapping script functions to runtime functions
				mRuntimeFunctionsMapped[&scriptFunc] = &runtimeFunc;

				// Register function under its actual name
				std::vector<RuntimeFunction*>& funcs = mRuntimeFunctionsBySignature[scriptFunc.getNameAndSignatureHash()];
				funcs.insert(funcs.begin(), &runtimeFunc);		// Insert as first

				// Register function under all of its aliases
				for (const Function::AliasName& aliasName : scriptFunc.getAliasNames())
				{
					std::vector<RuntimeFunction*>& funcs = mRuntimeFunctionsBySignature[aliasName.mName.getHash() + scriptFunc.getSignatureHash()];
					funcs.insert(funcs.begin(), &runtimeFunc);		// Insert as first
				}
			}

			// Load all string literals
			mProgram->collectAllStringLiterals(mStrings);
		}
	}

	void Runtime::clearAllControlFlows()
	{
		for (ControlFlow* controlFlow : mControlFlows)
		{
			controlFlow->reset();	// Existing control flows are only reset, not destroyed
		}
		mSelectedControlFlow = mControlFlows[0];	// Reset to main control flow
	}

	void Runtime::setProgram(const Program& program)
	{
		mProgram = &program;
		for (ControlFlow* controlFlow : mControlFlows)
		{
			controlFlow->mProgram = &program;
		}

		reset();
		setupGlobalVariables();
	}

	void Runtime::setMemoryAccessHandler(MemoryAccessHandler* handler)
	{
		mMemoryAccessHandler = handler;
		for (ControlFlow* controlFlow : mControlFlows)
		{
			controlFlow->mMemoryAccessHandler = mMemoryAccessHandler;
		}
	}

	void Runtime::setRuntimeDetailHandler(RuntimeDetailHandler* handler)
	{
		mRuntimeDetailHandler = handler;
	}

	void Runtime::resetRuntimeState()
	{
		// Reset global variables back to defaults
		setupGlobalVariables();
	}

	void Runtime::buildAllRuntimeFunctions()
	{
		for (Function* function : mProgram->getFunctions())
		{
			if (function->isA<ScriptFunction>())
			{
				getRuntimeFunction(function->as<ScriptFunction>());
			}
		}
	}

	RuntimeFunction* Runtime::getRuntimeFunction(const ScriptFunction& scriptFunction)
	{
		const auto it = mRuntimeFunctionsMapped.find(&scriptFunction);
		if (it == mRuntimeFunctionsMapped.end())
			return nullptr;

		RuntimeFunction* runtimeFunction = it->second;
		if (!runtimeFunction->build(*this))
		{
			mEncounteredBuildError = true;
			return nullptr;
		}

		return runtimeFunction;
	}

	RuntimeFunction* Runtime::getRuntimeFunctionBySignature(uint64 signatureHash, size_t index)
	{
		const auto it = mRuntimeFunctionsBySignature.find(signatureHash);
		if (it == mRuntimeFunctionsBySignature.end() || index >= it->second.size())
			return nullptr;

		RuntimeFunction* runtimeFunction = it->second[index];
		runtimeFunction->build(*this);
		return runtimeFunction;
	}

	bool Runtime::hasStringWithKey(uint64 key) const
	{
		return (nullptr != mStrings.getStringByHash(key));
	}

	const FlyweightString* Runtime::resolveStringByKey(uint64 key) const
	{
		return mStrings.getStringByHash(key);
	}

	uint64 Runtime::addString(std::string_view str)
	{
		const uint64 hash = rmx::getMurmur2_64(str);
		if (nullptr == mStrings.getStringByHash(hash))
		{
			mStrings.addString(str, hash);
		}
		return hash;
	}

	AnyBaseValue Runtime::getGlobalVariableValue(const GlobalVariable& variable)
	{
		AnyBaseValue result;
		const int64* valuePtr = accessGlobalVariableValue(variable);
		if (nullptr != valuePtr)
			result.set<int64>(*valuePtr);
		return result;
	}

	void Runtime::setGlobalVariableValue(const GlobalVariable& variable, AnyBaseValue value)
	{
		int64* valuePtr = accessGlobalVariableValue(variable);
		if (nullptr != valuePtr)
		{
			*valuePtr = value.get<int64>();
		}
	}

	int64* Runtime::accessGlobalVariableValue(const GlobalVariable& variable)
	{
		RMX_CHECK(variable.isA<GlobalVariable>(), "Variable " << variable.getName() << " is not a global variable", return nullptr);
		const size_t offset = variable.getStaticMemoryOffset();
		return (int64*)&mStaticMemory[offset];
	}

	void Runtime::callRuntimeFunction(const RuntimeFunction& runtimeFunction, size_t baseCallIndex)
	{
		if (mSelectedControlFlow->mLocalVariablesSize + runtimeFunction.mFunction->mLocalVariablesByID.size() > ControlFlow::VAR_STACK_LIMIT)
		{
			throw std::runtime_error("Reached var stack limit, possibly due to recursive function calls");
		}

		// Push new state to call stack
		ControlFlow::State& state = *mSelectedControlFlow->mCallStack.add();
		state.mRuntimeFunction = &runtimeFunction;
		state.mBaseCallIndex = baseCallIndex;
		state.mProgramCounter = runtimeFunction.getFirstRuntimeOpcode();
		state.mLocalVariablesStart = mSelectedControlFlow->mLocalVariablesSize;
		RMX_ASSERT(nullptr != state.mProgramCounter, "Invalid program counter in function " << runtimeFunction.mFunction->getName());
	}

	void Runtime::callFunction(const Function& function, size_t baseCallIndex)
	{
		switch (function.getType())
		{
			case Function::Type::SCRIPT:
			{
				const ScriptFunction& func = function.as<ScriptFunction>();
				callRuntimeFunction(*getRuntimeFunction(func));
				break;
			}

			case Function::Type::NATIVE:
			{
				// Directly execute it
				const NativeFunction& func = function.as<NativeFunction>();
				func.execute(NativeFunction::Context(*mSelectedControlFlow));
				break;
			}
		}
	}

	bool Runtime::callFunctionAtLabel(const Function& function, FlyweightString labelName)
	{
		if (!function.isA<ScriptFunction>())
			return false;

		const ScriptFunction& func = function.as<ScriptFunction>();
		const ScriptFunction::Label* label = func.findLabelByName(labelName);
		if (nullptr == label)
			return false;

		return callFunctionAtLabel(func, *label);
	}

	bool Runtime::callFunctionAtLabel(const ScriptFunction& function, const ScriptFunction::Label& label)
	{
		RuntimeFunction* runtimeFunction = getRuntimeFunction(function);
		RMX_ASSERT(nullptr != runtimeFunction, "Got invalid runtime function");
		callRuntimeFunction(*runtimeFunction);

		// Build up scope accordingly (all local variables will have a value of zero, though)
		int numLocalVars = (int)function.mLocalVariablesByID.size();
		//for (size_t i = 0; i < label.mOffset; ++i)
		//{
		//	if (func.mOpcodes[i].mType == Opcode::Type::MOVE_VAR_STACK)
		//	{
		//		numLocalVars += (int)func.mOpcodes[i].mParameter;
		//	}
		//}
		memset(&mSelectedControlFlow->mLocalVariablesBuffer[mSelectedControlFlow->mLocalVariablesSize], 0, numLocalVars * sizeof(int64));
		mSelectedControlFlow->mLocalVariablesSize += numLocalVars;
		RMX_CHECK(mSelectedControlFlow->mLocalVariablesSize <= ControlFlow::VAR_STACK_LIMIT, "Reached var stack limit, probably due to recursive function calls", RMX_REACT_THROW);
		mSelectedControlFlow->mCallStack.back().mProgramCounter = runtimeFunction->translateToRuntimeProgramCounter(label.mOffset);
		return true;
	}

	bool Runtime::callFunctionByName(FlyweightString functionName, FlyweightString labelName)
	{
		if (nullptr == mProgram)
			return false;

		const uint64 nameAndSignatureHash = functionName.getHash() + Function::getVoidSignatureHash();
		const Function* function = mProgram->getFunctionBySignature(nameAndSignatureHash);
		if (nullptr != function)
		{
			if (labelName.isEmpty())
			{
				callFunction(*function);
				return true;
			}
			else
			{
				if (callFunctionAtLabel(*function, labelName))
					return true;
			}
		}
		return false;
	}

	bool Runtime::callFunctionWithParameters(FlyweightString functionName, const FunctionCallParameters& params)
	{
		if (nullptr == mProgram)
			return false;

		const DataTypeDefinition& returnType = (nullptr != params.mReturnType) ? *params.mReturnType : PredefinedDataTypes::VOID;

		// Build the function signature hash
		uint32 signatureHash = Function::getVoidSignatureHash();
		if (!returnType.isA<VoidDataType>() || !params.mParams.empty())
		{
			Function::SignatureBuilder builder;
			builder.clear(returnType);
			for (const FunctionCallParameters::Parameter& param : params.mParams)
				builder.addParameterType(*param.mDataType);
			signatureHash = builder.getSignatureHash();
		}

		const uint64 nameAndSignatureHash = functionName.getHash() + signatureHash;
		const Function* function = mProgram->getFunctionBySignature(nameAndSignatureHash);
		if (nullptr != function)
		{
			// Push parameters accordingly
			for (const FunctionCallParameters::Parameter& param : params.mParams)
			{
				mSelectedControlFlow->pushValueStack(param.mStorage);
			}

			// Call
			callFunction(*function);
			return true;
		}
		return false;
	}

	bool Runtime::returnFromFunction()
	{
		if (mSelectedControlFlow->mCallStack.count == 0)
			return false;

		mSelectedControlFlow->mLocalVariablesSize = mSelectedControlFlow->mCallStack.back().mLocalVariablesStart;
		mSelectedControlFlow->mCallStack.pop_back();
		return true;
	}

	bool Runtime::canExecuteSteps() const
	{
		return !mEncounteredBuildError;
	}

	void Runtime::executeSteps(ExecuteConnector& result, size_t stepsLimit, size_t minimumCallStackSize)
	{
		if (mEncounteredBuildError)
		{
			result.mResult = ExecuteResult::Result::HALT;
			return;
		}

		result.mStepsExecuted = 0;
		result.mResult = ExecuteResult::Result::OKAY;

		if (mSelectedControlFlow->mCallStack.count <= minimumCallStackSize)
		{
			result.mResult = ExecuteResult::Result::HALT;
			return;
		}

		RuntimeOpcodeContext context;
		context.mControlFlow = mSelectedControlFlow;
		mActiveControlFlow = mSelectedControlFlow;
		mCurrentOpcodePtr = &context.mOpcode;

		// Outer loop
		//  -> Gets restarted whenever the currently running function changes
		//  -> Gets exited only by a stop signal or a return
		mReceivedStopSignal = false;
		while (!mReceivedStopSignal)
		{
			ControlFlow::State& state = mSelectedControlFlow->mCallStack.back();

		#ifdef DEBUG
			// Reached the end already?
			//  -> Should not happen actually, as all functions end with a return opcode
			//  -> That's why this check is only active in debug builds
			if (state.mProgramCounter > state.mRuntimeFunction->mRuntimeOpcodeBuffer.getEnd())
			{
				RMX_ASSERT(false, "Program counter exceeded the end of function");
				returnFromFunction();
				result.mResult = ExecuteResult::Result::HALT;
				mActiveControlFlow = nullptr;
				return;
			}
		#endif

			RMX_CHECK(mSelectedControlFlow->mValueStackPtr >= mSelectedControlFlow->mValueStackStart, "Value stack error: Removed elements from empty stack", mSelectedControlFlow->mValueStackPtr = mSelectedControlFlow->mValueStackStart);
			RMX_CHECK(mSelectedControlFlow->mValueStackPtr < &mSelectedControlFlow->mValueStackBuffer[ControlFlow::VALUE_STACK_LAST_INDEX], "Value stack error: Too many elements", mSelectedControlFlow->mValueStackPtr = &mSelectedControlFlow->mValueStackBuffer[0x77]);

			RMX_ASSERT(mSelectedControlFlow->mLocalVariablesSize <= ControlFlow::VAR_STACK_LIMIT, "Reached var stack limit");
			mSelectedControlFlow->mCurrentLocalVariables = reinterpret_cast<uint8*>(&mSelectedControlFlow->mLocalVariablesBuffer[state.mLocalVariablesStart]);
			RMX_ASSERT(nullptr != mSelectedControlFlow->mCurrentLocalVariables, "Reached var stack limit");

			context.mOpcode = (const RuntimeOpcode*)state.mProgramCounter;

			// Inner loop
			//  -> Main execution of opcodes inside a single function
			//  -> Not exited on jumps
			//  -> Gets exited by changing stayInsideInnerLoop when the running function was changed
			//  -> Gets exited by a return when control needs to be returned to the caller
			bool stayInsideInnerLoop = true;
			while (stayInsideInnerLoop)
			{
				while (context.mOpcode->mSuccessiveHandledOpcodes > 0)
				{
					// Optimization: Do multiple opcodes in a row without overheads if possible
					if (context.mOpcode->mSuccessiveHandledOpcodes >= 4)
					{
						(*context.mOpcode->mExecFunc)(context);
						context.mOpcode = context.mOpcode->getNextOpcode();

						(*context.mOpcode->mExecFunc)(context);
						context.mOpcode = context.mOpcode->getNextOpcode();

						(*context.mOpcode->mExecFunc)(context);
						context.mOpcode = context.mOpcode->getNextOpcode();

						(*context.mOpcode->mExecFunc)(context);
						context.mOpcode = context.mOpcode->getNextOpcode();

						result.mStepsExecuted += 4;
					}
					else
					{
						(*context.mOpcode->mExecFunc)(context);
						context.mOpcode = context.mOpcode->getNextOpcode();

						++result.mStepsExecuted;
					}
				}

				switch (context.mOpcode->mOpcodeType)
				{
					case Opcode::Type::JUMP_CONDITIONAL:
					{
						--mSelectedControlFlow->mValueStackPtr;
						if (*mSelectedControlFlow->mValueStackPtr != 0)
						{
							context.mOpcode = context.mOpcode->getNextOpcode();
							++result.mStepsExecuted;
							break;
						}

						// Fallthrough to unconditional jump
					}

					case Opcode::Type::JUMP:
					{
						state.mProgramCounter = reinterpret_cast<const uint8*>(context.mOpcode->getParameter<uint64>());

						// Check if steps limit is reached (this usually means the limit was exceeded already, but that's okay)
						//  -> This is needed to prevent endless loops
						++result.mStepsExecuted;
						if (result.mStepsExecuted >= stepsLimit)
						{
							mActiveControlFlow = nullptr;
							return;
						}

						context.mOpcode = (const RuntimeOpcode*)state.mProgramCounter;
						break;
					}

					case Opcode::Type::JUMP_SWITCH:
					{
						// Jump if top of stack is zero
						if (mSelectedControlFlow->mValueStackPtr[-1] == 0)
						{
							--mSelectedControlFlow->mValueStackPtr;
							context.mOpcode = reinterpret_cast<const RuntimeOpcode*>(context.mOpcode->getParameter<uint64>());
						}
						else
						{
							// Otherwise decrease it and go on with the next opcode
							--mSelectedControlFlow->mValueStackPtr[-1];
							context.mOpcode = context.mOpcode->getNextOpcode();
							++result.mStepsExecuted;
						}
						break;
					}

					case Opcode::Type::CALL:
					{
						state.mProgramCounter = (uint8*)context.mOpcode + context.mOpcode->mOffsetToNext;
						const uint64 callTarget = context.mOpcode->getParameter<uint64>();
						++result.mStepsExecuted;

						const Function* func = handleResultCall(*context.mOpcode);
						if (result.handleCall(func, callTarget))
						{
							// Restart the outer loop now that the running function has changed
							stayInsideInnerLoop = false;
							break;
						}
						else
						{
							// Call handling failed, return control to the caller
							mActiveControlFlow = nullptr;
							return;
						}
					}

					case Opcode::Type::RETURN:
					{
						mSelectedControlFlow->mLocalVariablesSize = mSelectedControlFlow->mCallStack.back().mLocalVariablesStart;
						mSelectedControlFlow->mCallStack.pop_back();
						++result.mStepsExecuted;

						if (result.handleReturn())
						{
							// Check stop conditions
							if (mSelectedControlFlow->mCallStack.count > minimumCallStackSize && result.mStepsExecuted < stepsLimit)
							{
								// Restart the outer loop now that the running function has changed
								stayInsideInnerLoop = false;
								break;
							}
						}

						// Handling failed or a stop condition triggered, return control to the caller
						mActiveControlFlow = nullptr;
						return;
					}

					case Opcode::Type::EXTERNAL_CALL:
					{
						state.mProgramCounter = (uint8*)context.mOpcode + context.mOpcode->mSize;
						--mSelectedControlFlow->mValueStackPtr;
						const uint64 targetAddress = *mSelectedControlFlow->mValueStackPtr;
						++result.mStepsExecuted;

						if (result.handleExternalCall(targetAddress))
						{
							// Restart the outer loop now that the running function has changed
							stayInsideInnerLoop = false;
							break;
						}
						else
						{
							mActiveControlFlow = nullptr;
							return;
						}
					}

					case Opcode::Type::EXTERNAL_JUMP:
					{
						state.mProgramCounter = (uint8*)context.mOpcode + context.mOpcode->mSize;
						--mSelectedControlFlow->mValueStackPtr;
						returnFromFunction();
						const uint64 targetAddress = *mSelectedControlFlow->mValueStackPtr;
						++result.mStepsExecuted;

						if (result.handleExternalJump(targetAddress))
						{
							// Check stop conditions
							if (mSelectedControlFlow->mCallStack.count > minimumCallStackSize && result.mStepsExecuted < stepsLimit)
							{
								// Restart the outer loop now that the running function has changed
								stayInsideInnerLoop = false;
								break;
							}
						}

						mActiveControlFlow = nullptr;
						return;
					}

					default:
						throw std::runtime_error("Unhandled opcode");
				}
			}
		}

		// Outer loop was exited by a stop signal
		mActiveControlFlow = nullptr;
	}

	const Function* Runtime::handleResultCall(const RuntimeOpcode& runtimeOpcode)
	{
		// Consider base function call, if additional data says so
		const size_t baseCallIndex = runtimeOpcode.mFlags.isSet(RuntimeOpcode::Flag::CALL_IS_BASE_CALL) ? (mSelectedControlFlow->getState().mBaseCallIndex + 1) : 0;

		if (runtimeOpcode.mFlags.isSet(RuntimeOpcode::Flag::CALL_TARGET_RUNTIME_FUNC))
		{
			// Take the runtime function shortcut (this is the most common one)
			const RuntimeFunction* runtimeFunction = runtimeOpcode.getParameter<const RuntimeFunction*>();
			callRuntimeFunction(*runtimeFunction, baseCallIndex);
			return runtimeFunction->mFunction;
		}
		else if (runtimeOpcode.mFlags.isSet(RuntimeOpcode::Flag::CALL_TARGET_RESOLVED))
		{
			// Take the shortcut to a normal function
			const Function* function = runtimeOpcode.getParameter<const Function*>();
			callFunction(*function, baseCallIndex);
			return function;
		}
		else
		{
			const uint64 callTarget = runtimeOpcode.getParameter<uint64>();
			RuntimeOpcode& runtimeOpcodeMutable = const_cast<RuntimeOpcode&>(runtimeOpcode);

			// If it's a script function call, there should be an associated runtime function that can be called directly
			RuntimeFunction* runtimeFunction = getRuntimeFunctionBySignature(callTarget, baseCallIndex);
			if (nullptr != runtimeFunction)
			{
				// Create a shortcut for next time
				runtimeOpcodeMutable.setParameter(runtimeFunction);
				runtimeOpcodeMutable.mFlags.set(RuntimeOpcode::Flag::CALL_TARGET_RUNTIME_FUNC);

				// Call the function now
				callRuntimeFunction(*runtimeFunction, baseCallIndex);
				return runtimeFunction->mFunction;
			}

			// Another try, in case it's not a script function
			const Function* function = mProgram->getFunctionBySignature(callTarget, baseCallIndex);
			if (nullptr != function)
			{
				// Create a shortcut for next time
				runtimeOpcodeMutable.setParameter(function);
				runtimeOpcodeMutable.mFlags.set(RuntimeOpcode::Flag::CALL_TARGET_RESOLVED);

				// Call the function now
				callFunction(*function, baseCallIndex);
				return function;
			}

			// Failed
			return nullptr;
		}
	}

	bool Runtime::serializeState(VectorBinarySerializer& serializer, std::string* outError)
	{
		// v2 stores the exact override and base-call depth for every frame. A name
		// alone cannot distinguish a mod's wrapper from the base game function.
		auto fail = [&](const char* error) {
			if (outError) *outError = error;
			return false;
		};
		if (!mProgram) return fail("No program loaded");
		constexpr uint32 SIGNATURE = 0x7c4e4d4c; // LMN|
		constexpr uint16 CURRENT_VERSION = 2;
		constexpr uint32 MAX_CALL_FRAMES = 4096;
		ControlFlow& flow = *mControlFlows[0];
		if (!serializer.isReading())
		{
			serializer.write(SIGNATURE);
			serializer.write(CURRENT_VERSION);
			serializer.writeAs<uint32>(flow.mCallStack.count);
			for (size_t i = 0; i < flow.mCallStack.count; ++i)
			{
				const auto& state = flow.mCallStack[i];
				const ScriptFunction& function = *state.mRuntimeFunction->mFunction;
				uint32 functionIndex = 0;
				while (mProgram->getFunctionBySignature(function.getNameAndSignatureHash(), functionIndex) != &function)
				{
					if (!mProgram->getFunctionBySignature(function.getNameAndSignatureHash(), functionIndex++))
						return fail("Call stack function is not in the active program");
				}
				serializer.write(function.getName().getString());
				serializer.write(function.getSignatureHash());
				serializer.write(functionIndex);
				serializer.writeAs<uint32>(state.mBaseCallIndex);
				serializer.write(function.addToCompiledHash(rmx::startFNV1a_64()));
				serializer.writeAs<uint32>(state.mRuntimeFunction->translateFromRuntimeProgramCounter(state.mProgramCounter));
				const size_t end = i + 1 < flow.mCallStack.count ? flow.mCallStack[i + 1].mLocalVariablesStart : flow.mLocalVariablesSize;
				serializer.writeAs<uint32>(end - state.mLocalVariablesStart);
				for (size_t k = state.mLocalVariablesStart; k < end; ++k) serializer.write(flow.mLocalVariablesBuffer[k]);
			}
			const uint32 valueCount = uint32(flow.getValueStackSize());
			serializer.write(valueCount);
			for (size_t i = 0; i < valueCount; ++i) serializer.write(flow.mValueStackStart[i]);
			serializer.writeAs<uint32>(mProgram->getGlobalVariables().size());
			for (const Variable* variable : mProgram->getGlobalVariables())
			{
				serializer.write(variable->getName().getString());
				const size_t offset = variable->isA<GlobalVariable>() ? variable->as<GlobalVariable>().getStaticMemoryOffset() : size_t(-1);
				if (offset <= mStaticMemory.size() && sizeof(int64) <= mStaticMemory.size() - offset)
					serializer.write(&mStaticMemory[offset], sizeof(int64));
				else serializer.write<int64>(0);
			}
			return !serializer.hasError();
		}

		// Parse and validate into temporary storage before mutating a live VM.
		// In particular, never resize CArray using a count already read into it:
		// that can make resize copy beyond the previous allocation.
		if (serializer.getRemaining() < 4) return fail("Truncated runtime state");
		uint16 version = 0;
		uint32 storedSignature = 0;
		memcpy(&storedSignature, serializer.peek(), sizeof(storedSignature));
		if (storedSignature == SIGNATURE)
		{
			if (serializer.getRemaining() < 6) return fail("Truncated runtime header");
			serializer.skip(4);
			version = serializer.read<uint16>();
			if (version > CURRENT_VERSION) return fail("Unsupported runtime state version");
		}
		auto readName = [&](std::string_view& name) {
			if (serializer.getRemaining() < 4) return false;
			const uint32 length = serializer.read<uint32>();
			if (length > 4096 || length > serializer.getRemaining()) return false;
			name = length ? std::string_view(reinterpret_cast<const char*>(serializer.peek()), length) : std::string_view();
			serializer.skip(length);
			return true;
		};
		struct FrameData
		{
			ControlFlow::State state;
			std::vector<int64> locals;
		};
		std::vector<FrameData> frames;
		if (serializer.getRemaining() < 4) return fail("Missing call stack");
		const uint32 frameCount = serializer.read<uint32>();
		if (frameCount > MAX_CALL_FRAMES || frameCount > serializer.getRemaining() / 16)
			return fail("Invalid call stack size");
		frames.reserve(frameCount);
		size_t localsCount = 0;
		for (uint32 i = 0; i < frameCount; ++i)
		{
			std::string_view name;
			if (!readName(name) || serializer.getRemaining() < (version >= 2 ? 28u : 12u))
				return fail("Truncated call stack entry");
			uint32 signature = serializer.read<uint32>();
			const uint32 functionIndex = version >= 2 ? serializer.read<uint32>() : 0;
			const uint32 baseCallIndex = version >= 2 ? serializer.read<uint32>() : 0;
			const uint64 codeHash = version >= 2 ? serializer.read<uint64>() : 0;
			const uint64 nameHash = rmx::getMurmur2_64(name);
			const Function* function = mProgram->getFunctionBySignature(nameHash + signature, functionIndex);
			if (!function && signature == 0xd202ef8d)
			{
				signature = 0x76e88724;
				function = mProgram->getFunctionBySignature(nameHash + signature, functionIndex);
			}
			if (!function || !function->isA<ScriptFunction>()) return fail("Saved script function is missing");
			if (version < 2 && mProgram->getFunctionBySignature(nameHash + signature, 1))
				return fail("Legacy state has ambiguous overridden functions; record again");
			const auto& scriptFunction = function->as<ScriptFunction>();
			if (version >= 2 && scriptFunction.addToCompiledHash(rmx::startFNV1a_64()) != codeHash)
				return fail("Saved script function code has changed");
			const uint32 pc = serializer.read<uint32>();
			const uint32 localCount = serializer.read<uint32>();
			if (pc >= scriptFunction.mOpcodes.size() || baseCallIndex > MAX_CALL_FRAMES)
				return fail("Invalid script program counter or base-call depth");
			if (localCount > ControlFlow::VAR_STACK_LIMIT - localsCount || localCount > serializer.getRemaining() / sizeof(int64))
				return fail("Invalid local variable stack size");
			RuntimeFunction* runtimeFunction = getRuntimeFunction(scriptFunction);
			if (!runtimeFunction) return fail("Could not build saved script function");
			FrameData frame;
			frame.state.mRuntimeFunction = runtimeFunction;
			frame.state.mBaseCallIndex = baseCallIndex;
			frame.state.mProgramCounter = runtimeFunction->translateToRuntimeProgramCounter(pc);
			frame.state.mLocalVariablesStart = localsCount;
			frame.locals.resize(localCount);
			for (auto& value : frame.locals) value = serializer.read<int64>();
			localsCount += localCount;
			frames.push_back(std::move(frame));
		}
		if (serializer.getRemaining() < 4) return fail("Missing value stack");
		const uint32 valueCount = serializer.read<uint32>();
		if (valueCount >= ControlFlow::VALUE_STACK_LAST_INDEX - ControlFlow::VALUE_STACK_FIRST_INDEX ||
			valueCount > serializer.getRemaining() / sizeof(uint64)) return fail("Invalid value stack size");
		std::vector<uint64> values(valueCount);
		for (auto& value : values) value = serializer.read<uint64>();
		if (serializer.getRemaining() < 4) return fail("Missing globals");
		const uint32 globalsCount = serializer.read<uint32>();
		if (globalsCount > 100000 || globalsCount > serializer.getRemaining() / (version ? 12u : 8u))
			return fail("Invalid global variable count");
		std::vector<std::pair<const GlobalVariable*, int64>> globals;
		globals.reserve(globalsCount);
		for (uint32 i = 0; i < globalsCount; ++i)
		{
			const Variable* variable = nullptr;
			if (version)
			{
				std::string_view name;
				if (!readName(name)) return fail("Truncated global variable name");
				variable = mProgram->getGlobalVariableByName(rmx::getMurmur2_64(name));
			}
			else if (i < mProgram->getGlobalVariables().size()) variable = mProgram->getGlobalVariables()[i];
			if (serializer.getRemaining() < sizeof(int64)) return fail("Truncated global variable value");
			const int64 value = serializer.read<int64>();
			if (variable && variable->isA<GlobalVariable>()) globals.emplace_back(&variable->as<GlobalVariable>(), value);
		}
		if (serializer.hasError()) return fail("Truncated runtime state");
		for (ControlFlow* controlFlow : mControlFlows) controlFlow->reset();
		flow.mCallStack.resize(frameCount);
		flow.mCallStack.count = frameCount;
		for (size_t i = 0; i < frames.size(); ++i)
		{
			flow.mCallStack[i] = frames[i].state;
			std::copy(frames[i].locals.begin(), frames[i].locals.end(), flow.mLocalVariablesBuffer + frames[i].state.mLocalVariablesStart);
		}
		flow.mLocalVariablesSize = localsCount;
		flow.mValueStackPtr = flow.mValueStackStart + values.size();
		std::copy(values.begin(), values.end(), flow.mValueStackStart);
		for (const auto& global : globals)
		{
			const size_t offset = global.first->getStaticMemoryOffset();
			if (offset <= mStaticMemory.size() && sizeof(int64) <= mStaticMemory.size() - offset)
				memcpy(&mStaticMemory[offset], &global.second, sizeof(int64));
		}
		// Exact snapshots must retain caller PCs, including indirect/base calls.
		// Guessing another call site can change control flow even without mods.
		return true;
	}

	void Runtime::setupGlobalVariables()
	{
		if (nullptr == mProgram)
			return;

		// Setup memory offsets and sizes
		size_t totalSize = 0;
		for (size_t index = 0; index < mProgram->getGlobalVariables().size(); ++index)
		{
			Variable& var = *mProgram->getGlobalVariables()[index];
			if (var.isA<GlobalVariable>())	// The other variable types don't use static memory size
			{
				GlobalVariable& variable = var.as<GlobalVariable>();
				size_t variableSize = variable.getDataType()->getBytes();
				variableSize = (variableSize + 7) / 8 * 8;		// Align to multiples of 8 bytes (i.e. int64 size)
				variable.mStaticMemoryOffset = totalSize;
				variable.mStaticMemorySize = variableSize;
				totalSize += variableSize;
			}
		}

		mStaticMemory.resize(totalSize);

		for (size_t index = 0; index < mProgram->getGlobalVariables().size(); ++index)
		{
			Variable& var = *mProgram->getGlobalVariables()[index];
			if (var.isA<GlobalVariable>())
			{
				GlobalVariable& variable = var.as<GlobalVariable>();
				if (variable.getStaticMemorySize() > 0)
				{
					const int64 value = variable.mInitialValue.get<int64>();
					*(int64*)&mStaticMemory[variable.getStaticMemoryOffset()] = value;
				}
			}
		}
	}

}
