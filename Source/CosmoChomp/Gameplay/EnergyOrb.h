#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnergyOrb.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class USoundBase;

/**
 * Glowing energy orb. The player picks it up by walking through it and carries it back
 * to the rocket; orbs knocked loose when the player is hurt are spawned as new orbs.
 */
UCLASS()
class COSMOCHOMP_API AEnergyOrb : public AActor
{
	GENERATED_BODY()

public:
	AEnergyOrb();

	/** How many orbs this pickup is worth */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orb")
	int32 Value = 1;

	/** Diameter of the visible orb */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orb")
	float OrbSize = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orb")
	FLinearColor GlowColor = FLinearColor(0.15f, 1.0f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orb")
	TObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orb")
	TObjectPtr<USoundBase> CollectSound;

	/** If set before BeginPlay (deferred spawn), the orb can't be picked up for this many seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orb", meta = (ExposeOnSpawn = "true"))
	float InitialPickupDelay = 0.0f;

	/** Prevents pickup for a while; used for orbs the player just dropped */
	UFUNCTION(BlueprintCallable, Category = "Orb")
	void SetPickupDelay(float Seconds);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Orb")
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere, Category = "Orb")
	TObjectPtr<UStaticMeshComponent> OrbMesh;

	FTimerHandle PickupDelayTimer;

	UFUNCTION()
	void OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryCollect(AActor* OtherActor);
	void EnablePickup();
};
