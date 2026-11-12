#include "UI/OSHUD.h"

#include "Characters/OSEnemyCharacter.h"
#include "Characters/OSPlayerCharacter.h"
#include "Combat/OSLockOnComponent.h"
#include "Combat/OSStatsComponent.h"
#include "Core/OSProgress.h"
#include "Data/OSDataSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/OSInteractable.h"

namespace
{
	const FLinearColor BackColor(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor HealthColor(0.75f, 0.1f, 0.1f);
	const FLinearColor StaminaColor(0.2f, 0.7f, 0.25f);
	const FLinearColor EnemyColor(0.85f, 0.35f, 0.1f);
}

void AOSHUD::ShowMessage(const FString& InMessage, float Duration)
{
	CenterMessage = InMessage;
	MessageEndTime = GetWorld()->GetTimeSeconds() + Duration;
}

void AOSHUD::DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Color)
{
	DrawRect(BackColor, X - 2.f, Y - 2.f, Width + 4.f, Height + 4.f);
	DrawRect(Color, X, Y, Width * FMath::Clamp(Fraction, 0.f, 1.f), Height);
}

void AOSHUD::DrawCenteredText(const FString& Text, float Y, float Scale, const FLinearColor& Color)
{
	UFont* Font = GEngine->GetMediumFont();
	float TextWidth = 0.f;
	float TextHeight = 0.f;
	GetTextSize(Text, TextWidth, TextHeight, Font, Scale);
	DrawText(Text, Color, (Canvas->ClipX - TextWidth) * 0.5f, Y, Font, Scale);
}

void AOSHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const float Width = Canvas->ClipX;
	const float Height = Canvas->ClipY;
	UFont* Font = GEngine->GetMediumFont();
	const FOSTextsDef& Texts = UOSDataSubsystem::Texts();

	const AOSPlayerCharacter* Player = Cast<AOSPlayerCharacter>(GetOwningPawn());
	if (Player)
	{
		DrawBar(40.f, 40.f, 3.f * Player->GetStats()->GetMaxHealth(), 16.f, Player->GetStats()->GetHealthFraction(), HealthColor);
		DrawBar(40.f, 66.f, 3.f * Player->GetStats()->GetMaxStamina(), 12.f, Player->GetStats()->GetStaminaFraction(), StaminaColor);

		if (const AOSInteractable* Interactable = Player->GetFocusedInteractable())
		{
			TMap<FString, FString> Values;
			Values.Add(TEXT("Keys"), UOSDataSubsystem::DescribeKeys(UOSDataSubsystem::Bindings().Interact, Texts));
			Values.Add(TEXT("Prompt"), Interactable->GetPrompt());
			DrawCenteredText(UOSDataSubsystem::FormatText(Texts.InteractPrompt, Values), Height * 0.72f, 1.4f, FLinearColor::White);
		}
	}

	if (const UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this))
	{
		TMap<FString, FString> Values;
		Values.Add(TEXT("Amount"), FString::FromInt(Progress->State.Currency));
		DrawText(UOSDataSubsystem::FormatText(Texts.Currency, Values), FLinearColor::White, Width - 220.f, Height - 60.f, Font, 1.4f);
	}

	const AOSCharacterBase* LockTarget = Player ? Player->GetLockOn()->GetTarget() : nullptr;
	const AOSEnemyCharacter* Boss = nullptr;

	for (TActorIterator<AOSEnemyCharacter> It(GetWorld()); It; ++It)
	{
		const AOSEnemyCharacter* Enemy = *It;
		if (!Enemy->IsAlive())
		{
			continue;
		}
		if (Enemy->IsBoss())
		{
			if (Enemy->IsAggressive())
			{
				Boss = Enemy;
			}
			continue;
		}

		// Regular enemies get a floating bar once they are hurt or targeted.
		const float Fraction = Enemy->GetStats()->GetHealthFraction();
		if (Fraction < 1.f || Enemy == LockTarget)
		{
			const FVector Screen = Project(Enemy->GetActorLocation() + FVector(0.f, 0.f, 120.f * Enemy->GetActorScale3D().Z));
			if (Screen.Z > 0.f)
			{
				DrawBar(Screen.X - 50.f, Screen.Y, 100.f, 6.f, Fraction, EnemyColor);
			}
		}
	}

	if (Boss)
	{
		const float BarWidth = Width * 0.5f;
		DrawCenteredText(Boss->GetDefinition().DisplayName, Height - 150.f, 1.3f, FLinearColor::White);
		DrawBar((Width - BarWidth) * 0.5f, Height - 120.f, BarWidth, 14.f, Boss->GetStats()->GetHealthFraction(), HealthColor);
	}

	if (LockTarget)
	{
		const FVector Screen = Project(LockTarget->GetActorLocation() + FVector(0.f, 0.f, 30.f));
		if (Screen.Z > 0.f)
		{
			DrawRect(FLinearColor::White, Screen.X - 5.f, Screen.Y - 5.f, 10.f, 10.f);
		}
	}

	if (!CenterMessage.IsEmpty() && GetWorld()->GetTimeSeconds() < MessageEndTime)
	{
		DrawCenteredText(CenterMessage, Height * 0.4f, 3.f, FLinearColor(0.9f, 0.8f, 0.6f));
	}
}
