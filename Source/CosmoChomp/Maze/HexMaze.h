#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexPlanetGrid.h"
#include "HexMaze.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Wraps the "Planet"-tagged actor in a randomly generated maze of hexagonal tiles.
 *
 * The planet surface is split into a hex grid sized from TargetTileSize, a maze is
 * carved through it, and wall meshes are stretched along every closed tile edge.
 * Also answers tile/path queries for gameplay (spawning, alien navigation).
 *
 * Generation is driven by ACosmoChompGameMode at the start of each round; use
 * "Preview In Editor" on a placed instance to see a layout without playing.
 */
UCLASS()
class COSMOCHOMP_API AHexMaze : public AActor
{
	GENERATED_BODY()

public:
	AHexMaze();

	/** If true, center on the nearest "Planet"-tagged actor and read the radius from its bounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Planet")
	bool bAutoDetectPlanet = true;

	/** Planet radius used when bAutoDetectPlanet is off (or no planet is found) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Planet", meta = (ClampMin = "100"))
	float PlanetRadius = 3000.0f;

	/** Desired distance between neighboring tile centers; the closest matching subdivision is used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Layout", meta = (ClampMin = "100"))
	float TargetTileSize = 650.0f;

	/** Chance to knock a dead end through to a neighbor, which adds loops so the maze can be escaped around */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Layout", meta = (ClampMin = "0", ClampMax = "1"))
	float BraidChance = 0.85f;

	/** Random seed for the layout. 0 picks a new layout every round. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Layout")
	int32 Seed = 0;

	/** Wall pieces, picked at random per edge and stretched to fit it. Pivot should be at the bottom center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Walls")
	TArray<TObjectPtr<UStaticMesh>> WallMeshes;

	/** Post placed on tile corners where walls meet, hiding the joints between wall pieces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Walls")
	TObjectPtr<UStaticMesh> PostMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Walls", meta = (ClampMin = "10"))
	float WallHeight = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Walls", meta = (ClampMin = "1"))
	float WallThickness = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Walls", meta = (ClampMin = "1"))
	float PostWidth = 75.0f;

	/** How far walls extend below the surface so curvature never leaves a gap at their base */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Walls", meta = (ClampMin = "0"))
	float WallSink = 40.0f;

	/** Builds the hex grid for the current planet. Called automatically by GenerateMaze if needed. */
	UFUNCTION(BlueprintCallable, Category = "Maze")
	void BuildGrid();

	/** Carves a new maze (fully opening StartTile into a plaza) and rebuilds the walls */
	UFUNCTION(BlueprintCallable, Category = "Maze")
	void GenerateMaze(int32 StartTile = 0);

	UFUNCTION(BlueprintCallable, Category = "Maze")
	void ClearWalls();

	UFUNCTION(CallInEditor, Category = "Maze")
	void PreviewInEditor();

	UFUNCTION(CallInEditor, Category = "Maze")
	void ClearPreview();

	// Queries
	// ------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Maze")
	int32 GetNumTiles() const { return Grid.Tiles.Num(); }

	UFUNCTION(BlueprintPure, Category = "Maze")
	FVector GetPlanetCenter() const { return PlanetCenter; }

	UFUNCTION(BlueprintPure, Category = "Maze")
	float GetPlanetRadius() const { return Radius; }

	/** World distance between neighboring tile centers */
	UFUNCTION(BlueprintPure, Category = "Maze")
	float GetTileSpacing() const;

	/** Unit "up" at the tile center */
	UFUNCTION(BlueprintPure, Category = "Maze")
	FVector GetTileUp(int32 Tile) const;

	/** World location at the tile center, Height above the planet surface */
	UFUNCTION(BlueprintPure, Category = "Maze")
	FVector GetTileLocation(int32 Tile, float Height = 0.0f) const;

	/** World location Height above the surface point under WorldLocation */
	UFUNCTION(BlueprintPure, Category = "Maze")
	FVector ProjectToSurface(const FVector& WorldLocation, float Height = 0.0f) const;

	/** Rotation with Z along the surface normal at WorldLocation and X as close to Forward as possible */
	UFUNCTION(BlueprintPure, Category = "Maze")
	FRotator GetSurfaceRotation(const FVector& WorldLocation, const FVector& Forward) const;

	UFUNCTION(BlueprintPure, Category = "Maze")
	int32 FindNearestTile(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Maze")
	TArray<int32> GetNeighbors(int32 Tile) const;

	/** Neighbors reachable from Tile without crossing a wall */
	UFUNCTION(BlueprintPure, Category = "Maze")
	TArray<int32> GetOpenNeighbors(int32 Tile) const;

	/** True if there is no wall between adjacent tiles A and B */
	UFUNCTION(BlueprintPure, Category = "Maze")
	bool IsPassage(int32 A, int32 B) const;

	/** Steps through the maze from FromTile to every tile (INDEX_NONE if unreachable) */
	UFUNCTION(BlueprintCallable, Category = "Maze")
	TArray<int32> ComputeDistances(int32 FromTile) const;

	/** Shortest route through the maze, including both ends; empty if unreachable */
	UFUNCTION(BlueprintCallable, Category = "Maze")
	TArray<int32> FindPath(int32 FromTile, int32 ToTile) const;

	const FHexPlanetGrid& GetGrid() const { return Grid; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Maze")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> WallComponents;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> PostComponent;

	FHexPlanetGrid Grid;

	/** Edges (FHexPlanetGrid::EdgeKey) with no wall */
	TSet<uint64> Passages;

	FVector PlanetCenter = FVector::ZeroVector;
	float Radius = 3000.0f;
	int32 Frequency = 1;

	void CarvePassages(int32 StartTile, FRandomStream& Stream);
	void BraidDeadEnds(FRandomStream& Stream);
	void BuildWalls(FRandomStream& Stream);

	UInstancedStaticMeshComponent* CreateInstanceComponent(UStaticMesh* Mesh);

	/** Transform that sits a mesh's bottom-center at Base, stretched to Size (X = length, Y = thickness, Z = height) */
	static FTransform FitMeshTransform(const UStaticMesh* Mesh, const FVector& Base, const FQuat& Rotation, const FVector& Size);
};
