#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RayGunPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class URotatingMovementComponent;
class UMaterialInterface;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRayGunCollected);

/**
 * A ray gun lying somewhere in the maze. Picking it up lets the player shoot aliens
 * for Duration seconds; the game mode spawns a new one somewhere else afterwards.
 */
UCLASS()
class COSMOCHOMP_API ARayGunPickup : public AActor
{
	GENERATED_BODY()

public:
	ARayGunPickup();

	/** Seconds the player can shoot after picking this up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ray Gun")
	float Duration = 10.0f;

	/** Length of the floating pickup model */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ray Gun")
	float DisplaySize = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ray Gun")
	TObjectPtr<USoundBase> PickupSound;

	/** Glowing column above the gun so it can be spotted over the maze walls */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ray Gun|Beacon")
	float BeaconHeight = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ray Gun|Beacon")
	float BeaconWidth = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ray Gun|Beacon")
	FLinearColor BeaconColor = FLinearColor(1.0f, 0.1f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ray Gun|Beacon")
	TObjectPtr<UMaterialInterface> BeaconMaterial;

	UPROPERTY(BlueprintAssignable, Category = "Ray Gun")
	FOnRayGunCollected OnCollected;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Ray Gun")
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere, Category = "Ray Gun")
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere, Category = "Ray Gun")
	TObjectPtr<UStaticMeshComponent> GunMesh;

	UPROPERTY(VisibleAnywhere, Category = "Ray Gun")
	TObjectPtr<URotatingMovementComponent> Spinner;

	UPROPERTY(VisibleAnywhere, Category = "Ray Gun|Beacon")
	TObjectPtr<UStaticMeshComponent> Beacon;

	UPROPERTY(VisibleAnywhere, Category = "Ray Gun|Beacon")
	TObjectPtr<class UPointLightComponent> Glow;

	UFUNCTION()
	void OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};

/**
 * Short-lived glowing beam drawn along the (curved) path of a ray gun shot.
 */
UCLASS()
class COSMOCHOMP_API ARayGunBeam : public AActor
{
	GENERATED_BODY()

public:
	ARayGunBeam();

	virtual void Tick(float DeltaTime) override;

	/** Draws straight segments between consecutive points */
	void SetPoints(const TArray<FVector>& Points, const FLinearColor& Color);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
	float BeamWidth = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
	float Lifetime = 0.3f;

	/** Peak brightness of the flashes at the muzzle and where the beam stops */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
	float FlashIntensity = 20000.0f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Beam")
	TObjectPtr<class UInstancedStaticMeshComponent> Segments;

	UPROPERTY(VisibleAnywhere, Category = "Beam")
	TObjectPtr<class UPointLightComponent> MuzzleFlash;

	UPROPERTY(VisibleAnywhere, Category = "Beam")
	TObjectPtr<class UPointLightComponent> ImpactFlash;

	UPROPERTY(EditAnywhere, Category = "Beam")
	TObjectPtr<class UMaterialInterface> BeamMaterial;

	float Age = 0.0f;
};
