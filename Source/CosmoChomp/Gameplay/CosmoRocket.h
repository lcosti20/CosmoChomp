#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CosmoRocket.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class USoundBase;
class ACosmoChompCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRocketFuelChanged, int32, StoredOrbs, int32, RequiredOrbs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRocketLaunched);

/**
 * The player's crashed rocket. Walking up to it deposits every carried orb as fuel;
 * once enough fuel is stored, the player boards and the rocket launches.
 */
UCLASS()
class COSMOCHOMP_API ACosmoRocket : public AActor
{
	GENERATED_BODY()

public:
	ACosmoRocket();

	virtual void Tick(float DeltaTime) override;

	/** Orbs needed for liftoff. Set by the game mode from the number of orbs in the maze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket")
	int32 RequiredOrbs = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket")
	float RocketHeight = 520.0f;

	/** Seconds of flight before the launch is considered complete */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket")
	float LaunchDuration = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket")
	float LaunchAcceleration = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Effects")
	TObjectPtr<USoundBase> DepositSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Effects")
	TObjectPtr<USoundBase> LaunchSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Effects")
	TObjectPtr<UMaterialInterface> FlameMaterial;

	UPROPERTY(BlueprintAssignable, Category = "Rocket")
	FOnRocketFuelChanged OnFuelChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rocket")
	FOnRocketLaunched OnLaunched;

	UFUNCTION(BlueprintPure, Category = "Rocket")
	int32 GetStoredOrbs() const { return StoredOrbs; }

	UFUNCTION(BlueprintPure, Category = "Rocket")
	bool HasLaunched() const { return bLaunched; }

	/** Takes all of the player's orbs; launches if that's enough fuel */
	UFUNCTION(BlueprintCallable, Category = "Rocket")
	void DepositFrom(ACosmoChompCharacter* Player);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Rocket")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Rocket")
	TObjectPtr<UStaticMeshComponent> RocketMesh;

	/** Exhaust flame, shown during launch */
	UPROPERTY(VisibleAnywhere, Category = "Rocket")
	TObjectPtr<UStaticMeshComponent> FlameMesh;

	/** Standing inside this deposits carried orbs */
	UPROPERTY(VisibleAnywhere, Category = "Rocket")
	TObjectPtr<USphereComponent> DepositZone;

	int32 StoredOrbs = 0;
	bool bLaunched = false;
	float LaunchTime = 0.0f;
	float LaunchSpeed = 0.0f;

	void Launch(ACosmoChompCharacter* Player);
};
