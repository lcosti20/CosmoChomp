#include "HexPlanetGrid.h"

namespace
{
	// Angle subtended by one icosahedron edge on its circumscribed sphere
	constexpr double IcosahedronEdgeAngle = 1.1071487177940904;

	int32 FindOrAddVertex(TArray<FVector>& Vertices, const FVector& Point)
	{
		// Vertices on shared icosahedron edges are generated once per adjacent face,
		// so merge points that land on the same spot.
		for (int32 Index = 0; Index < Vertices.Num(); ++Index)
		{
			if (FVector::DistSquared(Vertices[Index], Point) < 1e-10)
			{
				return Index;
			}
		}
		return Vertices.Add(Point);
	}
}

void FHexPlanetGrid::Build(int32 Frequency)
{
	Frequency = FMath::Max(1, Frequency);

	Tiles.Reset();
	Corners.Reset();
	EdgeCorners.Reset();

	// Icosahedron
	// ------------------------------------------------
	const double Phi = (1.0 + FMath::Sqrt(5.0)) * 0.5;
	const FVector IcoVerts[12] = {
		{-1, Phi, 0}, {1, Phi, 0}, {-1, -Phi, 0}, {1, -Phi, 0},
		{0, -1, Phi}, {0, 1, Phi}, {0, -1, -Phi}, {0, 1, -Phi},
		{Phi, 0, -1}, {Phi, 0, 1}, {-Phi, 0, -1}, {-Phi, 0, 1}
	};
	const int32 IcoFaces[20][3] = {
		{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
		{1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
		{3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
		{4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
	};

	// Geodesic subdivision: vertices become tiles, triangles become corners
	// ------------------------------------------------
	TArray<FVector> Vertices;
	TArray<FIntVector> Triangles;

	TArray<int32> FaceGrid;
	auto GridIndex = [Frequency](int32 I, int32 J) { return I * (Frequency + 1) + J; };

	for (const auto& Face : IcoFaces)
	{
		const FVector A = IcoVerts[Face[0]].GetSafeNormal();
		const FVector B = IcoVerts[Face[1]].GetSafeNormal();
		const FVector C = IcoVerts[Face[2]].GetSafeNormal();

		FaceGrid.Init(INDEX_NONE, (Frequency + 1) * (Frequency + 1));
		for (int32 I = 0; I <= Frequency; ++I)
		{
			for (int32 J = 0; J <= Frequency - I; ++J)
			{
				const double U = double(I) / Frequency;
				const double V = double(J) / Frequency;
				const FVector Point = (A * (1.0 - U - V) + B * U + C * V).GetSafeNormal();
				FaceGrid[GridIndex(I, J)] = FindOrAddVertex(Vertices, Point);
			}
		}

		for (int32 I = 0; I < Frequency; ++I)
		{
			for (int32 J = 0; J < Frequency - I; ++J)
			{
				Triangles.Emplace(FaceGrid[GridIndex(I, J)], FaceGrid[GridIndex(I + 1, J)], FaceGrid[GridIndex(I, J + 1)]);
				if (I + J < Frequency - 1)
				{
					Triangles.Emplace(FaceGrid[GridIndex(I + 1, J)], FaceGrid[GridIndex(I + 1, J + 1)], FaceGrid[GridIndex(I, J + 1)]);
				}
			}
		}
	}

	// Tiles, corners and adjacency
	// ------------------------------------------------
	Tiles.SetNum(Vertices.Num());
	for (int32 Index = 0; Index < Vertices.Num(); ++Index)
	{
		Tiles[Index].Center = Vertices[Index];
	}

	TMap<uint64, TArray<int32, TInlineAllocator<2>>> EdgeTriangles;
	for (int32 TriIndex = 0; TriIndex < Triangles.Num(); ++TriIndex)
	{
		const FIntVector& Tri = Triangles[TriIndex];
		Corners.Add((Vertices[Tri.X] + Vertices[Tri.Y] + Vertices[Tri.Z]).GetSafeNormal());

		const int32 TriVerts[3] = { Tri.X, Tri.Y, Tri.Z };
		for (int32 Side = 0; Side < 3; ++Side)
		{
			const int32 V0 = TriVerts[Side];
			const int32 V1 = TriVerts[(Side + 1) % 3];
			Tiles[V0].Corners.AddUnique(TriIndex);
			Tiles[V0].Neighbors.AddUnique(V1);
			Tiles[V1].Neighbors.AddUnique(V0);
			EdgeTriangles.FindOrAdd(EdgeKey(V0, V1)).AddUnique(TriIndex);
		}
	}

	for (const auto& Pair : EdgeTriangles)
	{
		if (Pair.Value.Num() == 2)
		{
			EdgeCorners.Add(Pair.Key, TPair<int32, int32>(Pair.Value[0], Pair.Value[1]));
		}
	}

	// Sort neighbors and corners by angle around each tile so they wind consistently
	for (FTile& Tile : Tiles)
	{
		FVector Tangent, Bitangent;
		Tile.Center.FindBestAxisVectors(Tangent, Bitangent);

		auto AngleOf = [&](const FVector& Point)
		{
			const FVector Offset = Point - Tile.Center;
			return FMath::Atan2(FVector::DotProduct(Offset, Bitangent), FVector::DotProduct(Offset, Tangent));
		};

		Tile.Neighbors.Sort([&](int32 L, int32 R) { return AngleOf(Vertices[L]) < AngleOf(Vertices[R]); });
		Tile.Corners.Sort([&](int32 L, int32 R) { return AngleOf(Corners[L]) < AngleOf(Corners[R]); });
	}
}

bool FHexPlanetGrid::GetEdgeCorners(int32 A, int32 B, int32& OutCornerA, int32& OutCornerB) const
{
	if (const TPair<int32, int32>* Found = EdgeCorners.Find(EdgeKey(A, B)))
	{
		OutCornerA = Found->Key;
		OutCornerB = Found->Value;
		return true;
	}
	return false;
}

double FHexPlanetGrid::TileSpacingRadians(int32 Frequency)
{
	return IcosahedronEdgeAngle / FMath::Max(1, Frequency);
}
