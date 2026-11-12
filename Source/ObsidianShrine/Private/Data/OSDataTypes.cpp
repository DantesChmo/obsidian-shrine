#include "Data/OSDataTypes.h"

namespace
{
	/** Below this a character counts as standing, cm/s. */
	constexpr float StandingSpeed = 10.f;
}

const FString& FOSAnimationSetDef::PickLocomotion(float Speed, bool bSprinting, float& OutRate) const
{
	OutRate = 1.f;
	if (Speed <= StandingSpeed)
	{
		return Idle;
	}

	const bool bRun = bSprinting && !Run.IsEmpty();
	const FString& Animation = bRun ? Run : Walk;
	if (Animation.IsEmpty())
	{
		return Idle;
	}

	const float AuthoredSpeed = bRun ? RunSpeed : WalkSpeed;
	if (AuthoredSpeed > 0.f)
	{
		OutRate = FMath::Clamp(Speed / AuthoredSpeed, 0.25f, 3.f);
	}
	return Animation;
}
