// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CosmoChompGameMode.generated.h"

class AHexMaze;
class ACosmoRocket;
class AEnergyOrb;
class AAlienMonster;
class ARayGunPickup;
class ACosmoChompCharacter;

/**
 *  Runs a round of Cosmo Chomp in any level that contains a HexMaze actor: generates the
 *  maze around the planet, lands the rocket and player on the start tile, fills the maze
 *  with energy orbs, and keeps aliens and the ray gun spawning until the player escapes
 *  or is overrun.
 */
UCLASS(abstract)
class ACosmoChompGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	ACosmoChompGameMode();

	virtual void StartPlay() override;

	virtual void RestartPlayer(AController* NewPlayer) override;

	/** Spawns Count orbs around Location (the player was hurt and dropped them) */
	void ScatterOrbs(const FVector& Location, int32 Count);

	void HandlePlayerDeath(ACosmoChompCharacter* Player);

	UFUNCTION(BlueprintPure, Category = "Cosmo Chomp")
	AHexMaze* GetMaze() const { return Maze; }

	UFUNCTION(BlueprintPure, Category = "Cosmo Chomp")
	ACosmoRocket* GetRocket() const { return Rocket; }

	/** Banner text for the HUD; empty when there's nothing to announce */
	UFUNCTION(BlueprintPure, Category = "Cosmo Chomp")
	FString GetBannerText() const;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Classes")
	TSubclassOf<ACosmoRocket> RocketClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Classes")
	TSubclassOf<AEnergyOrb> OrbClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Classes")
	TSubclassOf<AAlienMonster> AlienClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Classes")
	TSubclassOf<ARayGunPickup> RayGunClass;

	/** Fraction of the maze's orbs that must be delivered to refuel the rocket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Rules", meta = (ClampMin = "0.05", ClampMax = "1"))
	float OrbsRequiredFraction = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Rules", meta = (ClampMin = "0"))
	int32 NumAliens = 4;

	/** Grace period before aliens appear */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Rules", meta = (ClampMin = "0"))
	float AlienSpawnDelay = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Rules", meta = (ClampMin = "0"))
	float AlienRespawnDelay = 8.0f;

	/** Delay before the first ray gun appears, and before a new one appears after it's used up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Rules", meta = (ClampMin = "0"))
	float RayGunRespawnDelay = 15.0f;

	/** Also place a ray gun beside the rocket at the start of the round (handy for testing) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Rules")
	bool bRayGunAtStart = true;

	/** Seconds before the level restarts after winning or losing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Rules", meta = (ClampMin = "0"))
	float RestartDelay = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp|Rules")
	float OrbHeight = 70.0f;

	UPROPERTY()
	TObjectPtr<AHexMaze> Maze;

	UPROPERTY()
	TObjectPtr<ACosmoRocket> Rocket;

	/** The ray gun currently lying in the maze; there's never more than one */
	UPROPERTY()
	TObjectPtr<ARayGunPickup> ActiveRayGun;

	int32 StartTile = INDEX_NONE;
	bool bRoundReady = false;
	bool bRoundOver = false;
	FString Banner;
	float BannerEndTime = 0.0f;

	FTimerHandle RestartTimer;

	void SetupRound();
	void PlacePlayer(APawn* Pawn) const;
	void SpawnOrbs();
	void SpawnAliens();
	void SpawnAlien();
	void SpawnRayGun();
	void SpawnRayGunNearRocket();
	ARayGunPickup* PlaceRayGun(const FVector& Location, const FVector& Forward);
	void ShowBanner(const FString& Text, float Duration);
	void EndRound(bool bEscaped);
	void RestartRound();

	/** Random tile at least MinFraction of the maze's furthest distance away from FromTile */
	int32 PickDistantTile(int32 FromTile, float MinFraction) const;

	int32 GetPlayerTile() const;

	template <typename T>
	T* SpawnOnTile(TSubclassOf<T> Class, int32 Tile, float Height) const;

	UFUNCTION()
	void OnRocketLaunched();

	UFUNCTION()
	void OnAlienKilled(AAlienMonster* Alien);

	UFUNCTION()
	void OnRayGunCollected();
};
