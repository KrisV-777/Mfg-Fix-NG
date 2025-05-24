#include "MfgConsoleFunc.h"
#include "BSFaceGenAnimationData.h"
#include "ActorManager.h"

namespace MfgFix::MfgConsoleFunc
{
	namespace
	{
		enum Mode : std::int32_t
		{
			Reset = -1,
			Phoneme,
			Modifier,
			ExpressionValue,
			ExpressionId
		};
	}

	inline bool SetPhoneme(BSFaceGenAnimationData* animData, std::uint32_t a_id, std::int32_t a_value)
	{
		if (!animData) {
			logger::error("SetPhoneme :: No animdata found");
			return false;
		}

		if (a_id > 15) {
			logger::error("SetPhoneme :: PhonemeId out of range 0-15:id {},value {}", a_id, a_value);
			return false;
		}
		animData->phoneme2.SetValue(a_id, std::clamp(a_value, 0, 200) / 100.0f);
		return true;
	}

	inline std::int32_t GetPhoneme(const BSFaceGenAnimationData& animData, std::uint32_t a_id)
	{
		return a_id < animData.phoneme2.count ? std::lround(animData.phoneme2.values[a_id] * 100.0f) : 0;
	}

	inline bool SetModifier(BSFaceGenAnimationData* animData, std::uint32_t a_id, std::int32_t a_value)
	{
		if (!animData) {
			logger::error("SetModifier :: No animdata found");
			return false;
		}
		if (a_id > 13) {
			logger::error("SetModifier :: ModifierId is out of range 0-13:id {},value {}", a_id, a_value);
			return false;
		}
		animData->modifier2.SetValue(a_id, std::clamp(a_value, 0, 200) / 100.0f);

		return true;
	}

	inline std::int32_t GetModifier(const BSFaceGenAnimationData& animData, std::uint32_t a_id)
	{
		return a_id < animData.modifier2.count ? std::lround(animData.modifier2.values[a_id] * 100.0f) : 0;
	}

	inline bool SetExpression(BSFaceGenAnimationData* animData, std::uint32_t a_mood, std::int32_t a_value)
	{
		if (!animData) {
			logger::error("SetExpression :: No animdata found");
			return false;
		}
		if (a_mood > 16) {
			logger::error("SetExpression :: Mood is out of range 0-16:id {}, value {}", a_mood, a_value);
			return false;
		}

		animData->expressionOverride = false;
		animData->SetExpressionOverride(a_mood, std::clamp(a_value, 0, 200) / 100.0f);
		animData->expressionOverride = true;
		return true;
	}

	std::uint32_t GetActiveExpression(const BSFaceGenAnimationData& a_animData)
	{
		std::uint32_t expression = BSFaceGenAnimationData::Expression::MoodNeutral;

		for (std::uint32_t i = 0; i < a_animData.expression1.count; ++i) {
			if (a_animData.expression1.values[i] > a_animData.expression1.values[expression]) {
				expression = i;
			}
		}

		return expression;
	}

	bool SetPhonemeModifierSmooth(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_mode, std::uint32_t a_id, std::int32_t a_value, float a_speed)
	{
		if (!a_actor) {
			logger::error("SetPhonemeModifierSmooth :: No actor selected");
			return false;
		}

		auto actorPtr = a_actor;
		SKSE::GetTaskInterface()->AddUITask([actorPtr, a_mode, a_id, a_value, a_speed]() {
			auto base = actorPtr->GetActorBase();
			std::string_view name = base ? base->GetFullName() : "<Unknown>";
			auto animData = reinterpret_cast<BSFaceGenAnimationData*>(actorPtr->GetFaceGenAnimationData());
			if (!animData) {
				logger::error("SetPhonemeModifierSmooth [UITask] :: No animData found for actor {}", name);
				return;
			}
			ActorManager::SetSpeed(actorPtr, a_speed);
			RE::BSSpinLockGuard locker(animData->lock);
			switch (a_mode) {
			case -1:
				animData->ClearExpressionOverride();
				animData->Reset(0.0f, true, true, true, false);
				return;
			case 0:
				SetPhoneme(animData, a_id, a_value);
				return;
			case 1:
				SetModifier(animData, a_id, a_value);
				return;
			case 2:
				SetExpression(animData, a_id, a_value);
				return;
			default:
				return;
			}
		});

		return true;
	}

	bool SetPhonemeModifier(RE::StaticFunctionTag* a_tag, RE::Actor* a_actor, std::int32_t a_mode, std::uint32_t a_id, std::int32_t a_value)
	{
		return SetPhonemeModifierSmooth(a_tag, a_actor, a_mode, a_id, a_value, 0.f);
	}

	std::int32_t GetPhonemeModifier(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_mode, std::uint32_t a_id)
	{
		if (!a_actor) {
			logger::error("GetPhonemeModifier :: No actor selected");
			return -1;
		}

		auto animData = reinterpret_cast<BSFaceGenAnimationData*>(a_actor->GetFaceGenAnimationData());

		if (!animData) {
			const auto base = a_actor->GetActorBase();
			const std::string_view name = base ? base->GetFullName() : "<Unknown>";
			logger::error("GetPhonemeModifier :: No animData found for actor {}", name);
			return -1;
		}

		RE::BSSpinLockGuard locker(animData->lock);

		switch (a_mode) {
		case Mode::Phoneme:
			{
				return a_id < animData->phoneme2.count ? std::lround(animData->phoneme2.values[a_id] * 100.0f) : 0;
			}
		case Mode::Modifier:
			{
				return a_id < animData->modifier2.count ? std::lround(animData->modifier2.values[a_id] * 100.0f) : 0;
			}
		case Mode::ExpressionValue:
			{
				return a_id < animData->expression1.count ? std::lround(animData->expression1.values[a_id] * 100.0f) : 0;
			}
		case Mode::ExpressionId:
			{
				return GetActiveExpression(*animData);
			}
		}

		return -1;
	}

	inline bool ResetMFGSmooth(RE::StaticFunctionTag*, RE::Actor* a_actor, int a_mode, float a_speed)
	{
		if (!a_actor) {
			logger::error("ResetMFGSmooth :: No actor selected");
			return false;
		}

		auto actorPtr = a_actor;
		SKSE::GetTaskInterface()->AddUITask([actorPtr, a_mode, a_speed]() {
			constexpr int kPhonemeCount = 16;
			constexpr int kModifierCount = 14;

			auto base = actorPtr->GetActorBase();
			std::string_view name = base ? base->GetFullName() : "<Unknown>";
			auto animData = reinterpret_cast<BSFaceGenAnimationData*>(actorPtr->GetFaceGenAnimationData());
			if (!animData) {
				logger::error("ResetMFGSmooth [UITask] :: No animData found for actor {}", name);
				return;
			}
			ActorManager::SetSpeed(actorPtr, a_speed);
			RE::BSSpinLockGuard locker(animData->lock);

			switch (a_mode) {
			case -1:
				for (int m = 0; m < kModifierCount; ++m) SetModifier(animData, m, 0);
				for (int p = 0; p < kPhonemeCount; ++p) SetPhoneme(animData, p, 0);
				SetExpression(animData, GetActiveExpression(*animData), 0);
				break;
			case 0:
				for (int p = 0; p < kPhonemeCount; ++p) SetPhoneme(animData, p, 0);
				break;
			case 1:
				for (int m = 0; m < kModifierCount; ++m) SetModifier(animData, m, 0);
				break;
			default:
				logger::warn("ResetMFGSmooth: unexpected mode value {}", a_mode);
				break;
			}
		});
		return true;
	}

	inline void LogExpressionVector(const std::vector<float>& a_expression, const std::string_view a_name)
	{
		if (a_expression.size() != 32) {
			logger::error("Expression vector size is invalid: {}", a_expression.size());
			return;
		}

		std::string output = "Expression Vector for '" + std::string(a_name) + "': {";

		for (size_t i = 0; i < a_expression.size(); ++i) {
			float val = a_expression[i];
			int intPart = static_cast<int>(val);
			int fracPart = static_cast<int>((val - intPart) * 100 + 0.5f);	// 2 decimal digits
			output += std::to_string(intPart) + "." + (fracPart < 10 ? "0" : "") + std::to_string(fracPart);

			if (i != a_expression.size() - 1) {
				output += ", ";
			}
		}

		output += "}";

		logger::info("{}", output);
	}

	inline bool ApplyExpressionPreset(RE::StaticFunctionTag*, RE::Actor* a_actor, std::vector<float> a_expression, bool a_openMouth, int exprPower, float exprStrModifier, float modStrModifier, float phStrModifier, float a_speed)
	{
		if (!a_actor) {
			logger::error("ApplyExpressionPreset :: No actor selected");
			return false;
		}

		constexpr size_t kExpectedSize = 32;
		if (a_expression.size() != kExpectedSize) {
			logger::error("ApplyExpressionPreset :: Expression vector incorrect size: {}, expected: {}", a_expression.size(), kExpectedSize);
			return false;
		}

		// Extract key values outside the task for safety
		const auto exprNum = static_cast<int>(a_expression[30]);
		int exprStrResult = static_cast<int>(std::round(a_expression[31] * 100.0f * exprStrModifier));
		if (exprNum > 0 && exprStrResult == 0) {
			exprStrResult = exprPower;
		}

		// Copy by value for safe lambda capture
		auto expressionCopy = a_expression;
		auto actorPtr = a_actor;

		// Schedule a safe, thread-aware update
		SKSE::GetTaskInterface()->AddUITask([actorPtr, expressionCopy, a_openMouth, exprNum, exprStrResult, modStrModifier, phStrModifier, a_speed]() {
			auto base = actorPtr->GetActorBase();
			std::string_view name = base ? base->GetFullName() : "<Unknown>";

			auto animData = reinterpret_cast<BSFaceGenAnimationData*>(actorPtr->GetFaceGenAnimationData());
			if (!animData) {
				logger::error("ApplyExpressionPreset [UITask] :: No animData found for actor {}", name);
				return;
			}

			ActorManager::SetSpeed(actorPtr, a_speed);
			RE::BSSpinLockGuard locker(animData->lock);

			SetExpression(animData, exprNum, exprStrResult);

			if (!a_openMouth) {
				for (int i = 0; i <= 15; ++i) {
					int currentVal = GetPhoneme(*animData, i);
					int targetIntVal = static_cast<int>(std::round(expressionCopy[i] * 100.0f * phStrModifier));
					if (currentVal != targetIntVal) {
						SetPhoneme(animData, i, targetIntVal);
					}
				}
			}

			for (int i = 16, m = 0; i <= 29; ++i, ++m) {
				int currentVal = GetModifier(*animData, m);
				int targetIntVal = static_cast<int>(std::round(expressionCopy[i] * 100.0f * modStrModifier));
				if (currentVal != targetIntVal) {
					SetModifier(animData, m, targetIntVal);
				}
			}
		});

		return true;
	}


	

	RE::Actor* GetPlayerSpeechTarget(RE::StaticFunctionTag*)
	{
		//SKSE::log::info("GetPlayerSpeechTarget");

		if (auto speakerObjPtr = RE::MenuTopicManager::GetSingleton()->speaker) {
			if (auto speakerPtr = speakerObjPtr.get()) {
				if (auto speaker = speakerPtr.get()) {
					if (auto actor = speaker->As<RE::Actor>()) {
						const auto base = actor->GetActorBase();
						const std::string_view name = base ? base->GetFullName() : "<Unknown>";
						logger::info("GetPlayerSpeechTarget :: Player speech target is '{}'", name);
						return actor;
					}
				}
			}
		}

		return nullptr;
	}



	void Register()
	{
		SKSE::GetPapyrusInterface()->Register([](RE::BSScript::IVirtualMachine* a_vm) {
			a_vm->RegisterFunction("SetPhonemeModifierSmooth", "MfgConsoleFuncExt", SetPhonemeModifierSmooth);
			a_vm->RegisterFunction("SetPhonemeModifier", "MfgConsoleFunc", SetPhonemeModifier);
			a_vm->RegisterFunction("GetPhonemeModifier", "MfgConsoleFunc", GetPhonemeModifier);
			a_vm->RegisterFunction("ResetMFGSmooth", "MfgConsoleFuncExt", ResetMFGSmooth);
			a_vm->RegisterFunction("ApplyExpressionPreset", "MfgConsoleFuncExt", ApplyExpressionPreset);
			a_vm->RegisterFunction("GetPlayerSpeechTarget", "MfgConsoleFuncExt", GetPlayerSpeechTarget);

			return true;
		});
	}
}