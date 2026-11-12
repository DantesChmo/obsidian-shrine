#include "Characters/OSCharacterBase.h"

#include "Animation/AnimSequenceBase.h"
#include "Combat/OSActionComponent.h"
#include "Combat/OSCombatComponent.h"
#include "Combat/OSStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/OSDataSubsystem.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ObsidianShrine.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor InvulnerableColor(0.9f, 0.9f, 1.f);
	const FLinearColor DeadColor(0.05f, 0.05f, 0.05f);
}

AOSCharacterBase::AOSCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	Stats = CreateDefaultSubobject<UOSStatsComponent>(TEXT("Stats"));
	Action = CreateDefaultSubobject<UOSActionComponent>(TEXT("Action"));
	Combat = CreateDefaultSubobject<UOSCombatComponent>(TEXT("Combat"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	auto MakePlaceholder = [this](const TCHAR* Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(GetCapsuleComponent());
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetStaticMesh(Mesh);
		return Component;
	};

	// Basic shapes are 100 cm; the default capsule is 34 x 88.
	PlaceholderBody = MakePlaceholder(TEXT("PlaceholderBody"), CylinderMesh.Object);
	PlaceholderBody->SetRelativeScale3D(FVector(0.65f, 0.65f, 1.76f));

	PlaceholderNose = MakePlaceholder(TEXT("PlaceholderNose"), CubeMesh.Object);
	PlaceholderNose->SetRelativeLocation(FVector(34.f, 0.f, 60.f));
	PlaceholderNose->SetRelativeScale3D(FVector(0.25f, 0.15f, 0.1f));

	WeaponMesh = MakePlaceholder(TEXT("WeaponMesh"), CubeMesh.Object);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	bUseControllerRotationYaw = false;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Speeds, turn rate and acceleration come from the character definition (ApplyDefinition).
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AOSCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	LastGroundedLocation = GetActorLocation();
	Action->SetStats(Stats);
	Combat->Setup(Stats, Action);
	Action->OnActionStarted.AddUObject(this, &AOSCharacterBase::HandleActionStarted);
	Stats->OnDeath.AddUObject(this, &AOSCharacterBase::HandleDeath);

	ApplyDefinition(CharacterId);

	if (UOSDataSubsystem* Data = UOSDataSubsystem::Get())
	{
		Data->OnReloaded.AddUObject(this, &AOSCharacterBase::HandleDataReloaded);
	}
}

void AOSCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UOSDataSubsystem* Data = UOSDataSubsystem::Get())
	{
		Data->OnReloaded.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AOSCharacterBase::HandleDataReloaded()
{
	ApplyDefinition(CharacterId, true);
}

bool AOSCharacterBase::ApplyDefinition(FName InCharacterId, bool bKeepState)
{
	const UOSDataSubsystem* Data = UOSDataSubsystem::Get();
	const FOSCharacterDef* Found = Data ? Data->FindCharacter(InCharacterId) : nullptr;
	if (!Found)
	{
		UE_LOG(LogObsidian, Error, TEXT("%s: unknown character definition '%s'"), *GetName(), *InCharacterId.ToString());
		return false;
	}

	CharacterId = InCharacterId;
	Definition = *Found;
	const FOSAnimationSetDef* FoundSet = Data->FindAnimationSet(Definition.AnimationSet);
	AnimationSet = FoundSet ? *FoundSet : FOSAnimationSetDef();
	if (bKeepState)
	{
		Stats->UpdateFromDef(Definition);
	}
	else
	{
		Stats->InitFromDef(Definition);
	}
	Combat->Faction = Faction;

	TMap<FName, FName> Slots;
	if (const FOSWeaponDef* Weapon = Data->FindWeapon(Definition.Weapon))
	{
		Combat->SetWeapon(*Weapon);
		Slots = Weapon->Actions;
	}
	Slots.Append(Definition.Actions);
	Action->SetSlotMap(Slots);

	SetActorScale3D(FVector(FMath::Max(0.1f, Definition.Scale)));
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = Definition.WalkSpeed;
	Movement->RotationRate = FRotator(0.f, Definition.TurnRate, 0.f);
	Movement->MaxAcceleration = Definition.Acceleration;
	Movement->BrakingDecelerationWalking = Definition.BrakingDeceleration;

	ApplyVisuals();
	return true;
}

void AOSCharacterBase::ApplyVisuals()
{
	bHasSkeletalMesh = false;
	bActionAnimationPlaying = false;
	LoopingAnimation = nullptr;

	USkeletalMesh* SkeletalMesh = Definition.Mesh.IsEmpty() ? nullptr : Cast<USkeletalMesh>(FSoftObjectPath(Definition.Mesh).TryLoad());
	if (!Definition.Mesh.IsEmpty() && !SkeletalMesh)
	{
		UE_LOG(LogObsidian, Warning, TEXT("%s: mesh '%s' not found, using the placeholder"), *CharacterId.ToString(), *Definition.Mesh);
	}

	if (SkeletalMesh)
	{
		bHasSkeletalMesh = true;
		GetMesh()->SetSkeletalMeshAsset(SkeletalMesh);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
		GetMesh()->SetRelativeRotation(FRotator(0.f, Definition.MeshYaw, 0.f));
		// No animation blueprint (a project rule): one animation at a time, chosen in code from the set.
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}

	PlaceholderBody->SetVisibility(!bHasSkeletalMesh);
	PlaceholderNose->SetVisibility(!bHasSkeletalMesh);

	BaseColor = Definition.PlaceholderColor.IsEmpty() ? FLinearColor::Gray : FLinearColor(FColor::FromHex(Definition.PlaceholderColor));
	if (!BodyMaterial)
	{
		BodyMaterial = PlaceholderBody->CreateAndSetMaterialInstanceDynamic(0);
	}
	TintState = -1;

	UStaticMesh* ImportedWeapon = Combat->GetWeapon().Mesh.IsEmpty() ? nullptr : Cast<UStaticMesh>(FSoftObjectPath(Combat->GetWeapon().Mesh).TryLoad());
	if (ImportedWeapon && bHasSkeletalMesh)
	{
		WeaponMesh->SetStaticMesh(ImportedWeapon);
		WeaponMesh->SetRelativeTransform(FTransform::Identity);
		WeaponMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Combat->GetWeapon().AttachSocket);
	}
	// The placeholder weapon bar is only meaningful on the placeholder body.
	WeaponMesh->SetVisibility(!bHasSkeletalMesh || ImportedWeapon != nullptr);
}

bool AOSCharacterBase::IsAlive() const
{
	return !Stats->IsDead();
}

void AOSCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsAlive())
	{
		if (GetCharacterMovement()->IsMovingOnGround())
		{
			LastGroundedLocation = GetActorLocation();
		}
		if (GetActorLocation().Z < UOSDataSubsystem::Settings().Death.KillHeight)
		{
			Stats->Kill();
		}
	}

	if (IsAlive())
	{
		UpdateMovement(DeltaTime);
		UpdateFacing(DeltaTime);
	}

	if (bHasSkeletalMesh)
	{
		UpdateLocomotionAnimation();
	}
	else
	{
		UpdatePlaceholder();
	}
}

void AOSCharacterBase::UpdateMovement(float DeltaTime)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const bool bBusy = Action->IsBusy();

	Stats->StaminaRegenScale = bBusy ? Action->GetCurrent().StaminaRegenScale : 1.f;

	bSprinting = bWantsSprint && !bBusy && Stats->HasStamina();
	if (bSprinting && Movement->Velocity.SizeSquared2D() > FMath::Square(10.f))
	{
		Stats->SpendStamina(Definition.SprintStaminaPerSecond * DeltaTime);
	}

	const float ActionSpeed = Action->GetMoveSpeedNow();
	if (ActionSpeed > 0.f)
	{
		// The action's own displacement (dodge, lunge) overrides player and AI steering.
		Movement->MaxWalkSpeed = ActionSpeed;
		AddMovementInput(ActionMoveDirection, 1.f);
	}
	else
	{
		const float BaseSpeed = bSprinting ? Definition.SprintSpeed : Definition.WalkSpeed;
		Movement->MaxWalkSpeed = BaseSpeed * (bBusy ? Action->GetCurrent().MoveSpeedScale : 1.f);
	}
}

void AOSCharacterBase::UpdateFacing(float DeltaTime)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const bool bBusy = Action->IsBusy();
	const FOSActionDef& Current = Action->GetCurrent();
	const bool bFreeToTurn = !bBusy || Current.MoveSpeedScale > 0.f;

	const AActor* Target = GetCombatTarget();
	Movement->bOrientRotationToMovement = !Target && bFreeToTurn;

	if (Target && (bFreeToTurn || Action->GetTime() < Current.TrackingEnd))
	{
		FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
		ToTarget.Z = 0.f;
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator Desired(0.f, ToTarget.Rotation().Yaw, 0.f);
			SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Desired, DeltaTime, Definition.TurnRate));
		}
	}
}

void AOSCharacterBase::FaceDirection(const FVector& Direction)
{
	FVector Flat = Direction;
	Flat.Z = 0.f;
	if (!Flat.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.f, Flat.Rotation().Yaw, 0.f));
	}
}

void AOSCharacterBase::HandleActionStarted(const FOSActionDef& Def)
{
	const FVector Desired = GetDesiredActionDirection().GetSafeNormal2D();
	const AActor* Target = GetCombatTarget();

	if (Def.bSnapFacing)
	{
		if (Def.MoveDirection == EOSMoveDirection::Input || !Target)
		{
			FaceDirection(Desired);
		}
		else
		{
			FaceDirection(Target->GetActorLocation() - GetActorLocation());
		}
	}

	switch (Def.MoveDirection)
	{
	case EOSMoveDirection::Backward:
		ActionMoveDirection = -GetActorForwardVector();
		break;
	case EOSMoveDirection::Input:
		ActionMoveDirection = Desired.IsNearlyZero() ? -GetActorForwardVector() : Desired;
		break;
	default:
		ActionMoveDirection = GetActorForwardVector();
		break;
	}

	PlayActionAnimation(Def);
}

void AOSCharacterBase::HandleDeath()
{
	Action->StartSlot(OSSlots::Death, true);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCharacterMovement()->StopMovementImmediately();

	OnDeath();
	OnDied.Broadcast(this);
}

void AOSCharacterBase::FellOutOfWorld(const UDamageType& DamageType)
{
	// Go through the normal death flow instead of being silently destroyed.
	if (IsAlive())
	{
		Stats->Kill();
	}
}

void AOSCharacterBase::UpdatePlaceholder()
{
	// Tint: dead / invulnerable / normal.
	const int32 NewTint = !IsAlive() ? 2 : (Action->IsWindowActive(EOSWindowType::Invulnerable) ? 1 : 0);
	if (NewTint != TintState && BodyMaterial)
	{
		TintState = NewTint;
		const FLinearColor Color = NewTint == 2 ? DeadColor : (NewTint == 1 ? InvulnerableColor : BaseColor);
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}

	// Weapon bar: its length shows the real reach of the hit volume.
	const float Reach = FMath::Max(60.f, Combat->GetWeapon().Reach);
	const FVector BarScale(0.08f, 0.08f, Reach / 100.f);
	FVector Location(10.f, 42.f, 0.f);
	FRotator Rotation = FRotator::ZeroRotator;

	if (!IsAlive())
	{
		Location = FVector(30.f, 42.f, -80.f);
		Rotation = FRotator(-90.f, 0.f, 0.f);
	}
	else if (Action->IsWindowActive(EOSWindowType::Hit))
	{
		// Lying along the hit volume.
		Location = FVector(Reach * 0.5f, 0.f, 10.f);
		Rotation = FRotator(-90.f, 0.f, 0.f);
	}
	else if (Action->IsWindowActive(EOSWindowType::Guard) || Action->IsWindowActive(EOSWindowType::PerfectGuard))
	{
		// Held across the chest.
		Location = FVector(40.f, 0.f, 20.f);
		Rotation = FRotator(0.f, 0.f, 90.f);
	}
	else if (Action->IsBusy() && Action->GetCurrent().Windows.ContainsByPredicate(
		[](const FOSActionWindow& Window) { return Window.Type == EOSWindowType::Hit; }))
	{
		// Wind-up or recovery of an attack: raised.
		Location = FVector(-10.f, 42.f, 60.f);
		Rotation = FRotator(40.f, 0.f, 0.f);
	}

	WeaponMesh->SetRelativeScale3D(BarScale);
	WeaponMesh->SetRelativeLocationAndRotation(Location, Rotation);
}

UAnimSequenceBase* AOSCharacterBase::LoadAnimation(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return nullptr;
	}
	if (const TObjectPtr<UAnimSequenceBase>* Cached = AnimationCache.Find(Path))
	{
		return *Cached;
	}

	UAnimSequenceBase* Animation = Cast<UAnimSequenceBase>(FSoftObjectPath(Path).TryLoad());
	if (!Animation)
	{
		UE_LOG(LogObsidian, Warning, TEXT("%s: animation '%s' not found"), *CharacterId.ToString(), *Path);
	}
	AnimationCache.Add(Path, Animation);
	return Animation;
}

void AOSCharacterBase::PlayActionAnimation(const FOSActionDef& Def)
{
	bActionAnimationPlaying = false;
	if (!bHasSkeletalMesh)
	{
		return;
	}
	const FString* Path = AnimationSet.Actions.Find(Def.Id);
	UAnimSequenceBase* Animation = Path ? LoadAnimation(*Path) : nullptr;
	if (!Animation)
	{
		return;
	}

	const float Length = Animation->GetPlayLength();
	const float Rate = (Def.bFitAnimation && !Def.IsHeld() && Length > 0.f) ? Length / Def.Duration : 1.f;

	LoopingAnimation = nullptr;
	bActionAnimationPlaying = true;
	GetMesh()->PlayAnimation(Animation, Def.IsHeld());
	GetMesh()->SetPlayRate(Rate);
}

void AOSCharacterBase::UpdateLocomotionAnimation()
{
	if (!IsAlive())
	{
		return;
	}
	if (!Action->IsBusy())
	{
		bActionAnimationPlaying = false;
	}
	if (bActionAnimationPlaying)
	{
		return;
	}

	float Rate = 1.f;
	const FString& Path = AnimationSet.PickLocomotion(GetVelocity().Size2D(), bSprinting, Rate);
	UAnimSequenceBase* Wanted = LoadAnimation(Path);
	if (Wanted && Wanted != LoopingAnimation)
	{
		LoopingAnimation = Wanted;
		GetMesh()->PlayAnimation(Wanted, true);
	}
	if (Wanted)
	{
		GetMesh()->SetPlayRate(Rate);
	}
}
