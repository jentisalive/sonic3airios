/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2026 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "oxygen/pch.h"
#include "oxygen/simulation/Simulation.h"
#include "oxygen/simulation/CodeExec.h"
#include "oxygen/simulation/EmulatorInterface.h"
#include "oxygen/simulation/GameRecorder.h"
#include "oxygen/simulation/LogDisplay.h"
#include "oxygen/simulation/SaveStateSerializer.h"
#include "oxygen/simulation/SimulationState.h"
#include "oxygen/simulation/analyse/ROMDataAnalyser.h"
#include "oxygen/application/Configuration.h"
#include "oxygen/application/audio/AudioOutBase.h"
#include "oxygen/application/input/InputRecorder.h"
#include "oxygen/application/video/VideoOut.h"
#include "oxygen/engine/EngineMain.h"
#include "oxygen/engine/modding/ModManager.h"
#include "oxygen/helper/Logging.h"
#include "oxygen/network/netplay/NetplayManager.h"
#include "oxygen/platform/PlatformFunctions.h"
#include "oxygen/rendering/parts/RenderParts.h"


namespace
{
	void recordKeyFrame(uint32 frameNumber, Simulation& simulation, GameRecorder& gameRecorder, const GameRecorder::InputData& inputData)
	{
		static std::vector<uint8> data;
		data.reserve(0x128000);
		data.clear();

		SaveStateSerializer serializer(simulation, RenderParts::instance());
		serializer.saveState(data);

		gameRecorder.addKeyFrame(frameNumber, inputData, data);
	}
}


Simulation::Simulation() :
	mCodeExec(*new CodeExec()),
	mSimulationState(*new SimulationState()),
	mGameRecorder(*new GameRecorder()),
	mInputRecorder(*new InputRecorder())
{
	if (EngineMain::getDelegate().useDeveloperFeatures())
	{
		mROMDataAnalyser = new ROMDataAnalyser();
	}
}

Simulation::~Simulation()
{
	delete &mCodeExec;
	delete &mSimulationState;
	delete &mGameRecorder;
	delete &mInputRecorder;
	delete mROMDataAnalyser;
}

bool Simulation::startup()
{
	Configuration& config = Configuration::instance();

	RMX_LOG_INFO("Setup of EmulatorInterface");
	mCodeExec.startup();

	// Load scripts
	RMX_LOG_INFO("Loading scripts");
	bool success = mCodeExec.reloadScripts(true, false);	// Note: First parameter could just as well be set to false
	if (success)
	{
		mCodeExec.reinitRuntime(nullptr, CodeExec::CallStackInitPolicy::RESET);
	}

	// Optionally load save state
	mStateLoaded.clear();
	if (success && EngineMain::getDelegate().useDeveloperFeatures() && !config.mLoadSaveState.empty() && config.mStartPhase == 3)
	{
		success = loadState(config.mSaveStatesDirLocal + config.mLoadSaveState + L".state", false);
		if (!success)
			loadState(config.mSaveStatesDir + config.mLoadSaveState + L".state", false);
	}
	RMX_LOG_INFO("Runtime environment ready");

	if (EngineMain::getDelegate().useDeveloperFeatures())
	{
		// Startup input recorder
		mInputRecorder.initFromConfig();
	}

	if (mGameRecorder.isPlaying())
	{
		// Try the long and short name
		if (mGameRecorder.loadRecording(L"gamerecording.bin"))
		{
			RMX_LOG_INFO("Playback of 'gamerecording.bin'");
		}
		else if (mGameRecorder.loadRecording(L"gamerec.bin"))
		{
			RMX_LOG_INFO("Playback of 'gamerec.bin'");
		}

		if (mGameRecorder.hasFrameNumber(mFrameNumber))
		{
			mGameRecorder.setIgnoreKeys(config.mGameRecorder.mPlaybackIgnoreKeys);
			config.setSettingsReadOnly(true);	// Do not overwrite settings
			jumpToFrame(config.mGameRecorder.mPlaybackStartFrame, false);
		}
	}

	return true;
}

void Simulation::shutdown()
{
	RMX_LOG_INFO("Simulation shutdown");
	mInputRecorder.shutdown();

	mIsRunning = false;
}

void Simulation::reloadScriptsAfterModsChange()
{
	// Immediate reload of the scripts (while the loading text box is shown)
	if (mCodeExec.reloadScripts(false, false))
	{
		mCodeExec.reinitRuntime(nullptr, CodeExec::CallStackInitPolicy::RESET);
	}
}

EmulatorInterface& Simulation::getEmulatorInterface()
{
	return mCodeExec.getEmulatorInterface();
}

void Simulation::resetState()
{
	EngineMain::instance().getAudioOut().reset();
	resetIntoGame(nullptr);
}

void Simulation::resetIntoGame(const std::vector<std::pair<std::string, std::string>>* enforcedCallStack)
{
	// Reset randomization
	randomize();

	// Reset simulation
	mSimulationState.reset();

	// Reset video & audio
	VideoOut::instance().reset();
	EngineMain::instance().getAudioOut().resetGame();

	// Reset code execution
	mCodeExec.reset();
	mStateLoaded.clear();

	// Reload and initialize scripts as needed
	if (mCodeExec.reloadScripts(false, false))
	{
		mCodeExec.reinitRuntime(enforcedCallStack, CodeExec::CallStackInitPolicy::RESET);
	}

	// Apply mod settings
	applyModSettingsToGlobals();

	mFrameNumber = 0;
	mCurrentTargetFrame = 0.0;
	mLastCorrectionFrame = 0;
	mStepsLimit = -1;
	mBreakConditions.clearAll();
	mGameRecorder.clear();
}

void Simulation::resetIntoGame(const std::string& entryFunctionName)
{
	const std::vector<std::pair<std::string, std::string>> enforcedCallStack = { { entryFunctionName, "" } };
	resetIntoGame(&enforcedCallStack);
}

void Simulation::reloadLastState()
{
	if (!mStateLoaded.empty())
	{
		loadState(mStateLoaded);
	}
}

bool Simulation::loadState(const std::wstring& filename, bool showError)
{
	std::vector<uint8> state;
	if (!FTX::FileSystem->readFile(filename, state))
	{
		if (showError) RMX_ERROR("Failed to read save state '" << WString(filename).toStdString() << "'", );
		return false;
	}
	if (!loadState(state, showError)) return false;
	mStateLoaded = filename;
	return true;
}

bool Simulation::loadState(const std::vector<uint8>& state, bool showError, bool exactSnapshot)
{
	VideoOut::instance().reset();
	EngineMain::instance().getAudioOut().reset();

	SaveStateSerializer::StateType stateType;
	SaveStateSerializer serializer(*this, RenderParts::instance());

	const bool success = serializer.loadState(state, &stateType);
	if (!success)
	{
		if (showError)
			RMX_ERROR("Failed to restore simulation state", );
		return false;
	}

	mStateLoaded.clear();
	mCodeExec.reinitRuntime(nullptr, (stateType == SaveStateSerializer::StateType::GENSX) ? CodeExec::CallStackInitPolicy::READ_FROM_ASM : CodeExec::CallStackInitPolicy::USE_EXISTING, nullptr, !exactSnapshot);

	if (!exactSnapshot && Configuration::instance().mDevMode.mApplyModSettingsAfterLoadState)
	{
		applyModSettingsToGlobals();
	}

	mFrameNumber = 0;
	mCurrentTargetFrame = 0.0;
	mLastCorrectionFrame = 0;
	mStepsLimit = -1;
	mBreakConditions.clearAll();

	mGameRecorder.clear();
	return true;
}

void Simulation::saveState(const std::wstring& filename)
{
	SaveStateSerializer serializer(*this, RenderParts::instance());
	const bool success = serializer.saveState(filename);
	RMX_CHECK(success, "Failed to save save state '" << WString(filename).toStdString() << "'", return);

	// Also save a screenshot
	Bitmap bmp;
	VideoOut::instance().getScreenshot(bmp);
	bmp.save(filename + L".bmp");

	// Set as default for "reloadLastState"
	mStateLoaded = filename;
}

bool Simulation::triggerFullScriptsReload()
{
	if (mCodeExec.reloadScripts(true, true))
	{
		mCodeExec.restoreRuntimeState(!mStateLoaded.empty());
		return true;
	}
	else
	{
		return false;
	}
}

void Simulation::update(float timeElapsed)
{
	if (!isRunning() || !mCodeExec.isCodeExecutionPossible())
		return;

	if (mRewindSteps >= 0)
	{
		setSpeed(0.0f);
		while (mRewindSteps >= 1)
		{
			if (jumpToFrame(mFrameNumber - mRewindSteps))
				mRewindSteps = 0;
			else
				--mRewindSteps;		// Try again with one step less
		}
		mRewindSteps = -1;
	}

	// Limit length of one frame to 100ms
	timeElapsed = clamp(timeElapsed, 0.0f, 0.1f);

	// Do nothing as long as not enough time has passed
	const double oldTargetFrame = mCurrentTargetFrame;
	if (mStepsLimit < 0)
	{
		if (mSimulationSpeed > 0.0f)
		{
			const float step = timeElapsed * mSimulationSpeed;
			mCurrentTargetFrame += (double)step * (double)getSimulationFrequency();
		}
		else
		{
			mCurrentTargetFrame = roundToDouble(mCurrentTargetFrame);
		}
	}
	else if (mStepsLimit > 0)
	{
		mCurrentTargetFrame = roundToDouble(mCurrentTargetFrame + 1.0);
	}

	const bool useFrameInterpolation = !Configuration::instance().mClassicMode && (Configuration::instance().mFrameSync == Configuration::FrameSyncType::FRAME_INTERPOLATION);
	const uint32 requiredFrameNumber = useFrameInterpolation ? (uint32)std::ceil(mCurrentTargetFrame) : (uint32)roundToInt(mCurrentTargetFrame);

	if (mFrameNumber < requiredFrameNumber)
	{
		const SDL_TicksType startTime = SDL_GetTicks();
		const SDL_TicksType limitTime = startTime + 200;

		while (mFrameNumber < requiredFrameNumber)
		{
			// Update emulation
			const bool result = generateFrame();
			if (!result)
				break;
			// A game callback can pause at an act boundary. Do not execute the
			// remaining catch-up frames or carry their time into the next update.
			if (mSimulationSpeed <= 0.0f)
			{
				mCurrentTargetFrame = (double)mFrameNumber;
				break;
			}

			// Time limit to prevent non-responding application
			if (SDL_GetTicks() >= limitTime)
			{
				// Reset target frame to an earlier frame, but still make sure we had any progress at all
				mCurrentTargetFrame = std::max((double)mFrameNumber, oldTargetFrame);
				break;
			}
		}

		// Each second, a small correction to the accumulated time gets applied
		if ((int)(mFrameNumber - mLastCorrectionFrame) >= (int)getSimulationFrequency() || mFrameNumber < mLastCorrectionFrame)
		{
			// The idea here is to bring the accumulated time towards the midpoint, where it's most stable against unintentional double frames or frame skips (which might happen otherwise)
			//  -> This is most useful for 60 Hz displays with V-sync on, but should have a similar effect on e.g. 75 Hz, 90 Hz, 120 Hz
			//  -> It should not introduce any noticeable issues or game speed changes in other cases
			const double stableOffset = useFrameInterpolation ? 0.25 : 0.0;
			const double diff = mCurrentTargetFrame - (roundToDouble(mCurrentTargetFrame - stableOffset) + stableOffset);
			const constexpr double maxChange = 0.1;
			mCurrentTargetFrame += clamp(-diff, -maxChange, maxChange);
			mLastCorrectionFrame = mFrameNumber;
		}
	}

	if (mFrameNumber == requiredFrameNumber)
	{
		const float position = saturate((float)(mCurrentTargetFrame - (double)(mFrameNumber - 1)));
		VideoOut::instance().setInterFramePosition(position);
	}
	else
	{
		VideoOut::instance().setInterFramePosition(0.0f);
	}

#if 0
	// Meant for debugging of accumulated time stability
	if ((mFrameNumber % 6) == 0)
		LogDisplay::instance().setModeDisplay(String(0, "diff = %+0.3f", (float)(mCurrentTargetFrame - roundToDouble(mCurrentTargetFrame))));
#endif
}

bool Simulation::generateFrame()
{
	ControlsIn& controlsIn = ControlsIn::instance();

	const bool beginningNewFrame = mCodeExec.willBeginNewFrame();
	const float tickLength = 1.0f / getSimulationFrequency();

	bool completedCurrentFrame = false;

	// Steps to do when beginning a new frame
	if (beginningNewFrame)
	{
		// Check if we can even begin a new frame
		if (!NetplayManager::instance().canBeginNextFrame(mFrameNumber))
			return false;

		// A replay can reject a checkpoint during this callback. Do not execute
		// another script frame after the game has stopped the simulation.
		const bool wasRunning = mIsRunning;
		EngineMain::getDelegate().onPreFrameUpdate();
		if (wasRunning && !mIsRunning) return false;

		// Tell video that we begin a new frame
		VideoOut::instance().preFrameUpdate();

		// Game recorder: Save initial frame
		if (mGameRecorder.isRecording() && mGameRecorder.getRangeEnd() == 0)
		{
			recordKeyFrame(0, *this, mGameRecorder, GameRecorder::InputData());
		}

		controlsIn.beginInputUpdate();

		// Update netplay
		NetplayManager::instance().onFrameUpdate(controlsIn, mFrameNumber);

		// If game recorder has input data for the frame transition, then use that
		//  -> This is particularly relevant for rewinds, namely for the small fast forwards from the previous keyframe
		GameRecorder::PlaybackResult result;
		if (mGameRecorder.getFrameData(mFrameNumber + 1, result))
		{
			if (mGameRecorder.isPlaying())
				LogDisplay::instance().setModeDisplay("Game recorder playback at frame: " + std::to_string(mFrameNumber + 1));

			if (nullptr != result.mData && !Configuration::instance().mGameRecorder.mPlaybackIgnoreKeys)
			{
				// Load save state
				SaveStateSerializer::StateType stateType;
				SaveStateSerializer serializer(*this, RenderParts::instance());

				const bool success = serializer.loadState(*result.mData, &stateType);
				if (success)
				{
					mCodeExec.reinitRuntime(nullptr, (stateType == SaveStateSerializer::StateType::GENSX) ? CodeExec::CallStackInitPolicy::READ_FROM_ASM : CodeExec::CallStackInitPolicy::USE_EXISTING);
					completedCurrentFrame = true;
				}
				else
				{
					RMX_ERROR("Failed to load save state", );
				}
			}

			controlsIn.injectInputs(result.mInput->mInputs);
		}

		// Input recorder playback
		if (EngineMain::getDelegate().useDeveloperFeatures())
		{
			if (mInputRecorder.isPlaying())
			{
				const InputRecorder::InputState& inputState = mInputRecorder.updatePlayback(mFrameNumber);
				controlsIn.injectInputs(inputState.mInputFlags);
			}
		}

		// Update input state
		{
			controlsIn.endInputUpdate();

			EngineMain::getDelegate().onControlsUpdate();

			// Input state can be queried by scripts via "Input.getController" and "Input.getControllerPrevious"
		}
	}

	if (!completedCurrentFrame)		// Can be true already if game recorder playback just loaded a state
	{
		// Perform game simulation
		completedCurrentFrame = mCodeExec.performFrameUpdate();
	}

	// Steps to do when a frame got completed
	if (completedCurrentFrame)
	{
		// Tell game instance
		EngineMain::getDelegate().onPostFrameUpdate();

		// Tell video that we begin a new frame
		VideoOut::instance().postFrameUpdate();

		if (EngineMain::getDelegate().useDeveloperFeatures())
		{
			// Update input recording
			if (mInputRecorde…5115 tokens truncated…ry single-byte corruption, must be rejected without
    // mutating the caller's already-loaded recording.
    for (size_t i = 0; i < bytes.size(); ++i) {
        auto truncated = bytes; truncated.resize(i);
        assert(!s3air::ReplayData::decode(truncated, copy));
        assert(copy.encode() == bytes);
        auto corrupted = bytes; corrupted[i] ^= 0x80;
        assert(!s3air::ReplayData::decode(corrupted, copy));
    }
    assert(copy.scope == s3air::ReplayScope::FULL_ZONE && copy.actTimes[1] == 3721);
    assert(copy.actions[0].frame == 1 && copy.actions[0].state == std::vector<uint8_t>({9,8,7}));
    assert(copy.controls[0].manualSkip && copy.controls[0].speedMilli == 4000);
    auto invalid = original;
    invalid.events = {{3, 123, 1}};
    assert(!s3air::ReplayData::decode(invalid.encode(), copy));
    invalid.events = {{2, 123, 1}, {1, 123, 2}};
    assert(!s3air::ReplayData::decode(invalid.encode(), copy));
    invalid = original; invalid.frequency = 0;
    assert(!s3air::ReplayData::decode(invalid.encode(), copy));
    invalid = original; invalid.frames.clear();
    assert(invalid.encode().empty());
    invalid = original; invalid.state.clear();
    assert(invalid.encode().empty());
    // Correct checksum must not allow unknown format versions or oversized strings.
    auto resign = [](std::vector<uint8_t>& data) {
        const auto hash = s3air::ReplayData::checksum(data, data.size() - 8);
        for (size_t i = 0; i < 8; ++i) data[data.size()-8+i] = uint8_t(hash >> (i*8));
    };
    auto corrupt = bytes; corrupt[4] = 4; resign(corrupt);
    assert(!s3air::ReplayData::decode(corrupt, copy));
    corrupt = bytes;
    // Compatibility string length follows magic/version/act/time/date/flags/cutscene/frequency.
    for (int i=0; i<4; ++i) corrupt[33+i] = 0xff;
    resign(corrupt);
    assert(!s3air::ReplayData::decode(corrupt, copy));
    invalid = original; invalid.actions[0].frame = 3;
    assert(!s3air::ReplayData::decode(invalid.encode(), copy));
    invalid = original; invalid.actions = {{2,0,false,{1}}, {1,0,false,{2}}};
    assert(!s3air::ReplayData::decode(invalid.encode(), copy));
    invalid = original; invalid.actions[0].state.clear();
    assert(!s3air::ReplayData::decode(invalid.encode(), copy));
    invalid = original; invalid.controls[0].speedMilli = 0;
    assert(!s3air::ReplayData::decode(invalid.encode(), copy));
    invalid = original; invalid.scope = s3air::ReplayScope(9);
    assert(!s3air::ReplayData::decode(invalid.encode(), copy));
    assert(s3air::replaySpeedMilli(4.0f) == 4000);
    assert(s3air::replaySpeedMilli(2.0f) == 2000);
    assert(s3air::replaySpeedMilli(1.0f) == 1000);
    assert(s3air::replaySpeedMilli(0.0f, 4000) == 4000);
    assert(s3air::replaySpeedMilli(-1.0f, 2000) == 2000);
    assert(s3air::replaySpeedMilli(100.0f) == 32000);
    using S = s3air::ReplayScope;
    for (unsigned zone : {0u, 5u, 11u}) {
        const uint16_t a = uint16_t(zone << 8), b = a | 1;
        assert(s3air::replayShouldStart(S::EACH_ACT,a) && s3air::replayShouldStart(S::EACH_ACT,b));
        assert(s3air::replayShouldStart(S::FULL_ZONE,a) && !s3air::replayShouldStart(S::FULL_ZONE,b));
        assert(s3air::replayShouldStart(S::ACT_1,a) && !s3air::replayShouldStart(S::ACT_1,b));
        assert(!s3air::replayShouldStart(S::ACT_2,a) && s3air::replayShouldStart(S::ACT_2,b));
        assert(!s3air::replayShouldFinish(S::FULL_ZONE,a,a,2,false));
        assert(s3air::replayShouldFinish(S::FULL_ZONE,a,b,2,false));
        assert(s3air::replayShouldFinish(S::FULL_ZONE,a,a,1,false));
        assert(s3air::replayShouldFinish(S::FULL_ZONE,a,a,2,true));
        assert(!s3air::replayShouldFinish(S::FULL_ZONE,a,uint16_t(a+0x100),2,true));
        assert(s3air::replayShouldFinish(S::EACH_ACT,a,a,2,false));
        assert(!s3air::replayShouldFinish(S::ACT_1,a,b,2,false));
    }
    // Golden v1 file produced by the previous released codec, retained for migration.
    const std::vector<uint8_t> legacy = {83,51,82,80,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,60,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,1,2,3,1,0,0,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,31,74,234,220,106,29,228,236};
    assert(s3air::ReplayData::decode(legacy, copy));
    assert(copy.formatVersion == 1 && copy.frames.size() == 1 && copy.zoneAct == 0x101);
    assert(copy.scope == S::EACH_ACT && copy.actions.empty());
    std::cout << "Replay v3: four-player input, state actions, cutscene speeds, scope boundaries, v1 migration and corruption checks passed\n";
}
