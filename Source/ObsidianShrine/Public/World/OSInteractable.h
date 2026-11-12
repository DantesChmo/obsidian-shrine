#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "OSInteractable.generated.h"

class AOSPlayerCharacter;
class UStaticMeshComponent;

/** Base for things the player can use with the interact button: checkpoints, pickups, later doors and levers. */
UCLASS(Abstract)
class OBSIDIANSHRINE_API AOSInteractable : public AActor
{
	GENERATED_BODY()

public:
	AOSInteractable();

	/** 0: the game-wide InteractRadius from the game settings. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	float InteractRadius = 0.f;

	float GetInteractRadius() const;

	/** What the object offers, shown after the interact keys. Texts come from the Texts section of the data. */
	virtual FString GetPrompt() const;
	virtual bool CanInteract(const AOSPlayerCharacter* Player) const { return true; }
	virtual void Interact(AOSPlayerCharacter* Player) {}

	/** Nearest usable interactable in range of the player, or null. */
	static AOSInteractable* FindFor(const AOSPlayerCharacter* Player);

protected:
	/** Placeholder visual; no collision. */
	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UStaticMeshComponent> Visual;
};
