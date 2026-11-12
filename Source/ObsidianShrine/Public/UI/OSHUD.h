#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "OSHUD.generated.h"

/**
 * Canvas-drawn HUD with no assets: player bars, currency, lock-on marker, enemy and boss health,
 * interaction prompt and centre messages. Meant to be replaced by real UI in the visual pass.
 */
UCLASS()
class OBSIDIANSHRINE_API AOSHUD : public AHUD
{
	GENERATED_BODY()

public:
	void ShowMessage(const FString& InMessage, float Duration);

	virtual void DrawHUD() override;

private:
	FString CenterMessage;
	float MessageEndTime = 0.f;

	void DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Color);
	void DrawCenteredText(const FString& Text, float Y, float Scale, const FLinearColor& Color);
};
