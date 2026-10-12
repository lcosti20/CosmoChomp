// Copyright Epic Games, Inc. All Rights Reserved.

#include "CosmoChompGameMode.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "AlienMonster.h"
#include "CosmoChomp.h"
#include "CosmoChompCharacter.h"
#include "CosmoChompHUD.h"
#include "CosmoRocket.h"
#include "EnergyOrb.h"
#include "HexMaze.h"
#include "RayGunPickup.h"

ACosmoChompGameMode::ACosmoChompGameMode()
{
	HUDClass = ACosmoChompHUD::StaticClass();

	RocketClass = ACosmoRocket::StaticClass();
	OrbClass = AEnergyOrb::StaticClass();
	AlienClass = AAlienMonster::StaticClass();
	RayGunClass = ARayGunPickup::StaticClass();
}

void ACosmoChompGameMode::StartPlay()
{
	Super::StartPlay();

	SetupRound();
}

void ACosmoChompGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (NewPlayer)
	{
		PlacePlayer(NewPlayer->GetPawn());
	}
}

// Round flow
// ------------------------------------------------

void ACosmoChompGameMode::SetupRound()
{
	// Levels opt in to a Cosmo Chomp round by placing a HexMaze actor
	for (TActorIterator<AHexMaze> It(GetWorld()); It; ++It)
	{
		Maze = *It;
		break;
	}
	if (!Maze)
	{
		UE_LOG(LogCosmoChomp, Log, TEXT("CosmoChompGameMode: no HexMaze in the level, skipping round setup"));
		return;
	}

	Maze->BuildGrid();

	// Land on the tile closest to wherever the player started
	FVector StartLocation = Maze->GetPlanetCenter() + FVector::UpVector * Maze->GetPlanetRadius();
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController && PlayerController->GetPawn())
	{
		StartLocation = PlayerController->GetPawn()->GetActorLocation();
	}
	else if (AActor* PlayerStart = FindPlayerStart(PlayerController))
	{
		StartLocation = PlayerStart->GetActorLocation();
	}
	StartTile = Maze->FindNearestTile(StartLocation);

	Maze->GenerateMaze(StartTile);

	if (RocketClass)
	{
		Rocket = SpawnOnTile<ACosmoRocket>(RocketClass, StartTile, 0.0f);
		if (Rocket)
		{
			Rocket->OnLaunched.AddDynamic(this, &ACosmoChompGameMode::OnRocketLaunched);
		}
	}

	SpawnOrbs();
	bRoundReady = true;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* Controller = It->Get())
		{
			PlacePlayer(Controller->GetPawn());
		}
	}

	FTimerHandle AlienTimer;
	GetWorldTimerManager().SetTimer(AlienTimer, this, &ACosmoChompGameMode::SpawnAliens, FMath::Max(AlienSpawnDelay, 0.01f), false);

	FTimerHandle RayGunTimer;
	GetWorldTimerManager().SetTimer(RayGunTimer, this, &ACosmoChompGameMode::SpawnRayGun, FMath::Max(RayGunRespawnDelay, 0.01f), false);

	ShowBanner(TEXT("Collect energy orbs and bring them back to your rocket!"), 6.0f);
}

void ACosmoChompGameMode::PlacePlayer(APawn* Pawn) const
{
	if (!bRoundReady || !Maze || !Pawn)
	{
		return;
	}

	// Stand beside the rocket, toward one of the start tile's neighbors
	const TArray<int32> Neighbors = Maze->GetNeighbors(StartTile);
	const FVector TileUp = Maze->GetTileUp(StartTile);
	FVector Outward = Neighbors.IsEmpty() ? FVector::ForwardVector : Maze->GetTileUp(Neighbors[0]) - TileUp;
	Outward = FVector::VectorPlaneProject(Outward, TileUp).GetSafeNormal();

	float HalfHeight = 100.0f;
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}

	// Face the rocket: the camera starts behind the player, and would otherwise be pushed in by the rocket
	const FVector Location = Maze->ProjectToSurface(Maze->GetTileLocation(StartTile) + Outward * Maze->GetTileSpacing() * 0.42f, HalfHeight + 20.0f);
	Pawn->TeleportTo(Location, Maze->GetSurfaceRotation(Location, -Outward), false, true);
}

void ACosmoChompGameMode::SpawnOrbs()
{
	if (!OrbClass)
	{
		return;
	}

	// One orb per tile, like pellets in each cell of the maze
	int32 OrbCount = 0;
	for (int32 Tile = 0; Tile < Maze->GetNumTiles(); ++Tile)
	{
		if (Tile != StartTile && SpawnOnTile<AEnergyOrb>(OrbClass, Tile, OrbHeight))
		{
			++OrbCount;
		}
	}

	if (Rocket)
	{
		Rocket->RequiredOrbs = FMath::Clamp(FMath::CeilToInt(OrbCount * OrbsRequiredFraction), 1, FMath::Max(OrbCount, 1));
	}
}

void ACosmoChompGameMode::ScatterOrbs(const FVector& Location, int32 Count)
{
	if (!Maze || !OrbClass || Count <= 0)
	{
		return;
	}

	// Scatter across the current tile and the tiles open to it, so every orb stays reachable
	const int32 Tile = Maze->FindNearestTile(Location);
	TArray<int32> Tiles = Maze->GetOpenNeighbors(Tile);
	Tiles.Add(Tile);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 DropTile = Tiles[FMath::RandHelper(Tiles.Num())];
		const FVector Up = Maze->GetTileUp(DropTile);
		FVector Tangent, Bitangent;
		Up.FindBestAxisVectors(Tangent, Bitangent);

		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float Distance = FMath::FRandRange(0.0f, 0.28f) * Maze->GetTileSpacing();
		const FVector Offset = (Tangent * FMath::Cos(Angle) + Bitangent * FMath::Sin(Angle)) * Distance;
		const FVector OrbLocation = Maze->ProjectToSurface(Maze->GetTileLocation(DropTile) + Offset, OrbHeight);
		const FTransform Transform(Maze->GetSurfaceRotation(OrbLocation, Tangent), OrbLocation);

		AEnergyOrb* Orb = GetWorld()->SpawnActorDeferred<AEnergyOrb>(OrbClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Orb)
		{
			Orb->InitialPickupDelay = 1.5f;
			Orb->FinishSpawning(Transform);
		}
	}
}

void ACosmoChompGameMode::SpawnAliens()
{
	for (int32 Index = 0; Index < NumAliens; ++Index)
	{
		SpawnAlien();
	}
}

void ACosmoChompGameMode::SpawnAlien()
{
	if (bRoundOver || !Maze || !AlienClass)
	{
		return;
	}

	const float HalfHeight = AlienClass->GetDefaultObject<AAlienMonster>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	AAlienMonster* Alien = SpawnOnTile<AAlienMonster>(AlienClass, PickDistantTile(GetPlayerTile(), 0.5f), HalfHeight + 10.0f);
	if (Alien)
	{
		Alien->OnKilled.AddDynamic(this, &ACosmoChompGameMode::OnAlienKilled);
	}
}

void ACosmoChompGameMode::SpawnRayGun()
{
	if (bRoundOver || !Maze || !RayGunClass)
	{
		return;
	}

	ARayGunPickup* RayGun = SpawnOnTile<ARayGunPickup>(RayGunClass, PickDistantTile(GetPlayerTile(), 0.35f), 90.0f);
	if (RayGun)
	{
		RayGun->OnCollected.AddDynamic(this, &ACosmoChompGameMode::OnRayGunCollected);
	}
}

void ACosmoChompGameMode::OnAlienKilled(AAlienMonster* Alien)
{
	FTimerHandle RespawnTimer;
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ACosmoChompGameMode::SpawnAlien, FMath::Max(AlienRespawnDelay, 0.01f), false);
}

void ACosmoChompGameMode::OnRayGunCollected()
{
	// The next gun shows up a while after this one runs out
	const float Duration = RayGunClass ? RayGunClass->GetDefaultObject<ARayGunPickup>()->Duration : 0.0f;

	FTimerHandle RespawnTimer;
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ACosmoChompGameMode::SpawnRayGun, Duration + FMath::Max(RayGunRespawnDelay, 0.01f), false);
}

void ACosmoChompGameMode::OnRocketLaunched()
{
	EndRound(true);
}

void ACosmoChompGameMode::HandlePlayerDeath(ACosmoChompCharacter* Player)
{
	EndRound(false);
}

void ACosmoChompGameMode::EndRound(bool bEscaped)
{
	if (bRoundOver)
	{
		return;
	}
	bRoundOver = true;

	ShowBanner(bEscaped ? TEXT("Rocket refueled - you escaped the planet!") : TEXT("The aliens got you!"), RestartDelay + 1.0f);
	GetWorldTimerManager().SetTimer(RestartTimer, this, &ACosmoChompGameMode::RestartRound, FMath::Max(RestartDelay, 0.01f), false);
}

void ACosmoChompGameMode::RestartRound()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

// Helpers
// ------------------------------------------------

void ACosmoChompGameMode::ShowBanner(const FString& Text, float Duration)
{
	Banner = Text;
	BannerEndTime = GetWorld()->GetTimeSeconds() + Duration;
}

FString ACosmoChompGameMode::GetBannerText() const
{
	return GetWorld()->GetTimeSeconds() < BannerEndTime ? Banner : FString();
}

int32 ACosmoChompGameMode::GetPlayerTile() const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return Pawn && Maze ? Maze->FindNearestTile(Pawn->GetActorLocation()) : StartTile;
}

int32 ACosmoChompGameMode::PickDistantTile(int32 FromTile, float MinFraction) const
{
	const TArray<int32> Distances = Maze->ComputeDistances(FromTile);
	int32 MaxDistance = 0;
	for (int32 Distance : Distances)
	{
		MaxDistance = FMath::Max(MaxDistance, Distance);
	}

	TArray<int32> Candidates;
	for (int32 Tile = 0; Tile < Distances.Num(); ++Tile)
	{
		if (Tile != StartTile && Distances[Tile] >= MaxDistance * MinFraction)
		{
			Candidates.Add(Tile);
		}
	}

	return Candidates.IsEmpty() ? FMath::RandRange(0, Maze->GetNumTiles() - 1) : Candidates[FMath::RandHelper(Candidates.Num())];
}

template <typename T>
T* ACosmoChompGameMode::SpawnOnTile(TSubclassOf<T> Class, int32 Tile, float Height) const
{
	const FVector Location = Maze->GetTileLocation(Tile, Height);

	// Random heading so props and aliens don't all line up
	const FVector Up = Maze->GetTileUp(Tile);
	FVector Tangent, Bitangent;
	Up.FindBestAxisVectors(Tangent, Bitangent);
	const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
	const FRotator Rotation = Maze->GetSurfaceRotation(Location, Tangent * FMath::Cos(Angle) + Bitangent * FMath::Sin(Angle));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<T>(Class, Location, Rotation, SpawnParams);
}
