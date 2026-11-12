#include "Combat/OSLockOnComponent.h"

#include "Characters/OSCharacterBase.h"
#include "Data/OSDataSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UOSLockOnComponent::UOSLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

float UOSLockOnComponent::ScoreCandidate(const FVector& ViewLocation, const FVector& ViewDirection, const FVector& CandidateLocation,
	float MaxDistance, float MaxAngleDegrees)
{
	const FVector ToCandidate = CandidateLocation - ViewLocation;
	const float Distance = ToCandidate.Size();
	if (Distance > MaxDistance || Distance < KINDA_SMALL_NUMBER)
	{
		return -1.f;
	}

	const float Cosine = FVector::DotProduct(ViewDirection.GetSafeNormal(), ToCandidate / Distance);
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cosine, -1.f, 1.f)));
	if (Angle > MaxAngleDegrees)
	{
		return -1.f;
	}

	// One degree off-centre costs as much as one metre of distance.
	return Angle + Distance / 100.f;
}

bool UOSLockOnComponent::GetView(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}
	Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

bool UOSLockOnComponent::IsCandidate(const AOSCharacterBase* Candidate) const
{
	const AOSCharacterBase* Owner = Cast<AOSCharacterBase>(GetOwner());
	return Candidate && Owner && Candidate != Owner && Candidate->IsAlive() && Candidate->Faction != Owner->Faction;
}

bool UOSLockOnComponent::HasLineOfSight(const FVector& ViewLocation, const AOSCharacterBase* Candidate) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OSLockOnSight), false, GetOwner());
	Params.AddIgnoredActor(Candidate);
	return !GetWorld()->LineTraceTestByChannel(ViewLocation, Candidate->GetActorLocation(), ECC_Visibility, Params);
}

void UOSLockOnComponent::Toggle()
{
	if (Target.IsValid())
	{
		Clear();
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetView(ViewLocation, ViewRotation))
	{
		return;
	}

	const FOSLockOnDef& Settings = UOSDataSubsystem::Settings().LockOn;
	AOSCharacterBase* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<AOSCharacterBase> It(GetWorld()); It; ++It)
	{
		AOSCharacterBase* Candidate = *It;
		if (!IsCandidate(Candidate))
		{
			continue;
		}
		const float Score = ScoreCandidate(ViewLocation, ViewRotation.Vector(), Candidate->GetActorLocation(), Settings.AcquireDistance, Settings.AcquireAngle);
		if (Score >= 0.f && Score < BestScore && HasLineOfSight(ViewLocation, Candidate))
		{
			Best = Candidate;
			BestScore = Score;
		}
	}
	Target = Best;
}

void UOSLockOnComponent::Clear()
{
	Target.Reset();
}

void UOSLockOnComponent::Switch(float Direction)
{
	FVector ViewLocation;
	FRotator ViewRotation;
	const AOSCharacterBase* Current = Target.Get();
	if (!Current || !GetView(ViewLocation, ViewRotation))
	{
		return;
	}

	const FVector Right = FRotationMatrix(FRotator(0.f, ViewRotation.Yaw, 0.f)).GetUnitAxis(EAxis::Y);
	AOSCharacterBase* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AOSCharacterBase> It(GetWorld()); It; ++It)
	{
		AOSCharacterBase* Candidate = *It;
		if (Candidate == Current || !IsCandidate(Candidate))
		{
			continue;
		}
		if (FVector::Dist(GetOwner()->GetActorLocation(), Candidate->GetActorLocation()) > UOSDataSubsystem::Settings().LockOn.AcquireDistance)
		{
			continue;
		}
		const FVector Offset = Candidate->GetActorLocation() - Current->GetActorLocation();
		const float Side = FVector::DotProduct(Right, Offset);
		if (Side * Direction <= 0.f)
		{
			continue;
		}
		if (Offset.Size() < BestDistance && HasLineOfSight(ViewLocation, Candidate))
		{
			Best = Candidate;
			BestDistance = Offset.Size();
		}
	}
	if (Best)
	{
		Target = Best;
	}
}

void UOSLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AOSCharacterBase* Current = Target.Get();
	if (!Current)
	{
		return;
	}

	const FOSLockOnDef& Settings = UOSDataSubsystem::Settings().LockOn;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller || !Current->IsAlive() || FVector::Dist(Pawn->GetActorLocation(), Current->GetActorLocation()) > Settings.BreakDistance)
	{
		Clear();
		return;
	}

	// Aim from above the player's centre so close targets do not pitch the camera wildly.
	const FVector Pivot = Pawn->GetActorLocation() + FVector(0.f, 0.f, Settings.PivotHeight);
	FRotator Desired = (Current->GetActorLocation() - Pivot).Rotation();
	Desired.Pitch = FMath::Clamp(Desired.Pitch + Settings.CameraPitchOffset, Settings.MinPitch, Settings.MaxPitch);
	Desired.Roll = 0.f;
	Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaTime, Settings.CameraSpeed));
}
