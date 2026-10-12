#pragma once

#include "CoreMinimal.h"

/**
 * A hexagonal tiling of the unit sphere (a Goldberg polyhedron).
 *
 * Built by subdividing an icosahedron into a geodesic sphere: every geodesic vertex
 * becomes a tile center and every geodesic triangle becomes a tile corner. The result
 * is all hexagons except for exactly 12 pentagons (one per icosahedron vertex), which
 * is unavoidable when tiling a sphere.
 *
 * Everything is stored on the unit sphere; scale by the planet radius to place things.
 */
struct FHexPlanetGrid
{
	struct FTile
	{
		/** Unit direction from the planet center to the tile center */
		FVector Center = FVector::UpVector;

		/** Adjacent tiles (6, or 5 for the pentagons) */
		TArray<int32> Neighbors;

		/** Corner indices around this tile */
		TArray<int32> Corners;
	};

	TArray<FTile> Tiles;

	/** Unit directions of every tile corner */
	TArray<FVector> Corners;

	/**
	 * Rebuilds the grid. Frequency is the number of subdivisions per icosahedron edge;
	 * the grid has 10 * Frequency^2 + 2 tiles.
	 */
	void Build(int32 Frequency);

	/** Returns the two corners shared by adjacent tiles A and B (the endpoints of the edge between them) */
	bool GetEdgeCorners(int32 A, int32 B, int32& OutCornerA, int32& OutCornerB) const;

	/** Order-independent key for the edge between two tiles */
	static uint64 EdgeKey(int32 A, int32 B)
	{
		return (uint64(FMath::Min(A, B)) << 32) | uint64(FMath::Max(A, B));
	}

	/** Average angular distance (radians) between neighboring tile centers for a given frequency */
	static double TileSpacingRadians(int32 Frequency);

private:
	/** For each edge between adjacent tiles, the two corners (triangles) that bound it */
	TMap<uint64, TPair<int32, int32>> EdgeCorners;
};
