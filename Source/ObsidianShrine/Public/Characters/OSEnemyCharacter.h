#pragma once

#include "CoreMinimal.h"
#include "Characters/OSCharacterBase.h"

#include "OSEnemyCharacter.generated.h"

class UOSAIBrainComponent;

/** Any enemy, regular or boss. Which one is decided entirely by CharacterId and its AI profile. */
UCLASS()
class OBSIDIANSHRINE_API AOSEnemyCharacter : public AOSCharacterBase
{
	GENERATED_BODY()

public:
	AOSEnemyCharacter();

	/** Set by the spawner. Non-empty for enemies that stay dead once killed (bosses). */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	FName PersistentId;

	UOSAIBrainComponent* GetBrain() const { return Brain; }
	bool IsBoss() const { return GetDefinition().bBoss; }
	bool IsAggressive() const;

	virtual AActor* GetCombatTarget() const override;
	virtual FVector GetDesiredActionDirection() const override;

	virtual void BeginPlay() override;

protected:
	virtual void OnDeath() override;
	virtual void HandleDataReloaded() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UOSAIBrainComponent> Brain;

	/** Gives the brain the AI profile named by the definition. bKeepState: do not restart the fight. */
	void ApplyProfile(bool bKeepState);
};
