#include "HexMaze.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "CosmoChomp.h"

AHexMaze::AHexMaze()
{
	PrimaryActorTick.bCanEverTick = false;

	// Static so the (static) wall instance components can attach to it
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	// Pieces split out of "Wall Ready For Construction" (see Tools/AssetPipeline)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WallA(TEXT("/Game/Meshes/Walls/SM_Wall_04"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WallB(TEXT("/Game/Meshes/Walls/SM_Wall_05"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Post(TEXT("/Game/Meshes/Walls/SM_Wall_03"));

	if (WallA.Succeeded())
	{
		WallMeshes.Add(WallA.Object);
	}
	if (WallB.Succeeded())
	{
		WallMeshes.Add(WallB.Object);
	}
	PostMesh = Post.Object;
}

void AHexMaze::BeginPlay()
{
	Super::BeginPlay();

	// Drop any editor preview; the game mode generates a fresh layout for the round
	ClearWalls();
}

// Generation
// ------------------------------------------------

void AHexMaze::BuildGrid()
{
	PlanetCenter = GetActorLocation();
	Radius = PlanetRadius;

	if (bAutoDetectPlanet)
	{
		TArray<AActor*> Planets;
		UGameplayStatics::GetAllActorsWithTag(this, FName("Planet"), Planets);

		AActor* Nearest = nullptr;
		for (AActor* Planet : Planets)
		{
			if (!Nearest || FVector::DistSquared(Planet->GetActorLocation(), GetActorLocation()) < FVector::DistSquared(Nearest->GetActorLocation(), GetActorLocation()))
			{
				Nearest = Planet;
			}
		}

		// Measure only what characters stand on; BP_Planet also has a much larger overlap-only GravityWell sphere
		float SurfaceRadius = 0.0f;
		if (Nearest)
		{
			Nearest->ForEachComponent<UPrimitiveComponent>(false, [&SurfaceRadius](const UPrimitiveComponent* Component)
			{
				if (Component->IsCollisionEnabled() && Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
				{
					SurfaceRadius = FMath::Max(SurfaceRadius, float(Component->Bounds.BoxExtent.GetMin()));
				}
			});
		}

		if (SurfaceRadius > 0.0f)
		{
			PlanetCenter = Nearest->GetActorLocation();
			Radius = SurfaceRadius;
		}
		else
		{
			UE_LOG(LogCosmoChomp, Warning, TEXT("HexMaze: no blocking actor tagged 'Planet' found, using PlanetRadius %.0f"), PlanetRadius);
		}
	}

	Frequency = FMath::Clamp(FMath::RoundToInt(Radius * FHexPlanetGrid::TileSpacingRadians(1) / TargetTileSize), 1, 24);
	Grid.Build(Frequency);
	Passages.Reset();

	UE_LOG(LogCosmoChomp, Log, TEXT("HexMaze: radius %.0f, frequency %d, %d tiles, spacing %.0f"), Radius, Frequency, Grid.Tiles.Num(), GetTileSpacing());
}

void AHexMaze::GenerateMaze(int32 StartTile)
{
	if (Grid.Tiles.IsEmpty())
	{
		BuildGrid();
	}
	StartTile = Grid.Tiles.IsValidIndex(StartTile) ? StartTile : 0;

	FRandomStream Stream(Seed != 0 ? Seed : FMath::Rand());

	Passages.Reset();
	CarvePassages(StartTile, Stream);
	BraidDeadEnds(Stream);

	// Open the start tile up completely so there's room for the rocket and the player
	for (int32 Neighbor : Grid.Tiles[StartTile].Neighbors)
	{
		Passages.Add(FHexPlanetGrid::EdgeKey(StartTile, Neighbor));
	}

	BuildWalls(Stream);
}

void AHexMaze::CarvePassages(int32 StartTile, FRandomStream& Stream)
{
	// Randomized depth-first search: produces long, winding corridors
	TArray<bool> Visited;
	Visited.Init(false, Grid.Tiles.Num());

	TArray<int32> Stack;
	Stack.Push(StartTile);
	Visited[StartTile] = true;

	TArray<int32> Candidates;
	while (!Stack.IsEmpty())
	{
		const int32 Current = Stack.Last();

		Candidates.Reset();
		for (int32 Neighbor : Grid.Tiles[Current].Neighbors)
		{
			if (!Visited[Neighbor])
			{
				Candidates.Add(Neighbor);
			}
		}

		if (Candidates.IsEmpty())
		{
			Stack.Pop();
			continue;
		}

		const int32 Next = Candidates[Stream.RandHelper(Candidates.Num())];
		Passages.Add(FHexPlanetGrid::EdgeKey(Current, Next));
		Visited[Next] = true;
		Stack.Push(Next);
	}
}

void AHexMaze::BraidDeadEnds(FRandomStream& Stream)
{
	// A perfect maze has exactly one route between tiles, which makes aliens impossible
	// to escape. Knocking out dead ends adds the loops a Pac-Man style chase needs.
	TArray<int32> Order;
	for (int32 Tile = 0; Tile < Grid.Tiles.Num(); ++Tile)
	{
		Order.Add(Tile);
	}
	for (int32 Index = Order.Num() - 1; Index > 0; --Index)
	{
		Order.Swap(Index, Stream.RandHelper(Index + 1));
	}

	TArray<int32> Closed;
	TArray<int32> ClosedDeadEnds;
	for (int32 Tile : Order)
	{
		if (GetOpenNeighbors(Tile).Num() != 1 || Stream.FRand() >= BraidChance)
		{
			continue;
		}

		Closed.Reset();
		ClosedDeadEnds.Reset();
		for (int32 Neighbor : Grid.Tiles[Tile].Neighbors)
		{
			if (!IsPassage(Tile, Neighbor))
			{
				Closed.Add(Neighbor);
				if (GetOpenNeighbors(Neighbor).Num() == 1)
				{
					ClosedDeadEnds.Add(Neighbor);
				}
			}
		}

		// Prefer joining two dead ends, which removes both with a single wall
		const TArray<int32>& Choices = ClosedDeadEnds.IsEmpty() ? Closed : ClosedDeadEnds;
		if (!Choices.IsEmpty())
		{
			Passages.Add(FHexPlanetGrid::EdgeKey(Tile, Choices[Stream.RandHelper(Choices.Num())]));
		}
	}
}

void AHexMaze::BuildWalls(FRandomStream& Stream)
{
	ClearWalls();

	TArray<UStaticMesh*> ValidWallMeshes;
	for (UStaticMesh* Mesh : WallMeshes)
	{
		if (Mesh)
		{
			ValidWallMeshes.Add(Mesh);
		}
	}
	if (ValidWallMeshes.IsEmpty())
	{
		UE_LOG(LogCosmoChomp, Warning, TEXT("HexMaze: no wall meshes set"));
		return;
	}

	for (UStaticMesh* Mesh : ValidWallMeshes)
	{
		WallComponents.Add(CreateInstanceComponent(Mesh));
	}
	if (PostMesh)
	{
		PostComponent = CreateInstanceComponent(PostMesh);
	}

	TArray<bool> CornerHasWall;
	CornerHasWall.Init(false, Grid.Corners.Num());

	for (int32 Tile = 0; Tile < Grid.Tiles.Num(); ++Tile)
	{
		for (int32 Neighbor : Grid.Tiles[Tile].Neighbors)
		{
			// Visit each edge once, and only the ones that are walls
			int32 CornerA, CornerB;
			if (Neighbor < Tile || IsPassage(Tile, Neighbor) || !Grid.GetEdgeCorners(Tile, Neighbor, CornerA, CornerB))
			{
				continue;
			}
			CornerHasWall[CornerA] = true;
			CornerHasWall[CornerB] = true;

			const FVector PointA = Grid.Corners[CornerA] * Radius;
			const FVector PointB = Grid.Corners[CornerB] * Radius;
			const FVector Up = (PointA + PointB).GetSafeNormal();
			const FVector Along = FVector::VectorPlaneProject(PointB - PointA, Up).GetSafeNormal();
			const FQuat Rotation = FRotationMatrix::MakeFromXZ(Along, Up).ToQuat();
			const FVector Base = PlanetCenter + Up * (Radius - WallSink);
			const FVector Size(FVector::Dist(PointA, PointB), WallThickness, WallHeight + WallSink);

			const int32 MeshIndex = Stream.RandHelper(ValidWallMeshes.Num());
			WallComponents[MeshIndex]->AddInstance(FitMeshTransform(ValidWallMeshes[MeshIndex], Base, Rotation, Size), true);
		}
	}

	if (PostComponent)
	{
		const FVector PostSize(PostWidth, PostWidth, WallHeight + WallSink + 15.0f);
		for (int32 Corner = 0; Corner < Grid.Corners.Num(); ++Corner)
		{
			if (CornerHasWall[Corner])
			{
				const FVector Up = Grid.Corners[Corner];
				const FQuat Rotation = FRotationMatrix::MakeFromZ(Up).ToQuat();
				PostComponent->AddInstance(FitMeshTransform(PostMesh, PlanetCenter + Up * (Radius - WallSink), Rotation, PostSize), true);
			}
		}
	}
}

UInstancedStaticMeshComponent* AHexMaze::CreateInstanceComponent(UStaticMesh* Mesh)
{
	// Transient so editor previews are never saved into the level
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
	Component->SetStaticMesh(Mesh);
	Component->SetMobility(EComponentMobility::Static);
	Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Component->SetupAttachment(Root);
	Component->RegisterComponent();
	return Component;
}

void AHexMaze::ClearWalls()
{
	for (UInstancedStaticMeshComponent* Component : WallComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	WallComponents.Reset();

	if (PostComponent)
	{
		PostComponent->DestroyComponent();
		PostComponent = nullptr;
	}
}

void AHexMaze::PreviewInEditor()
{
	BuildGrid();
	GenerateMaze(FindNearestTile(PlanetCenter + FVector::UpVector * Radius));
}

void AHexMaze::ClearPreview()
{
	ClearWalls();
}

FTransform AHexMaze::FitMeshTransform(const UStaticMesh* Mesh, const FVector& Base, const FQuat& Rotation, const FVector& Size)
{
	const FBox Bounds = Mesh->GetBoundingBox();
	const FVector MeshSize = Bounds.GetSize().ComponentMax(FVector(KINDA_SMALL_NUMBER));
	const FVector Scale = Size / MeshSize;
	const FVector BottomCenter(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);

	return FTransform(Rotation, Base - Rotation.RotateVector(BottomCenter * Scale), Scale);
}

// Queries
// ------------------------------------------------

float AHexMaze::GetTileSpacing() const
{
	return Radius * FHexPlanetGrid::TileSpacingRadians(Frequency);
}

FVector AHexMaze::GetTileUp(int32 Tile) const
{
	return Grid.Tiles.IsValidIndex(Tile) ? Grid.Tiles[Tile].Center : FVector::UpVector;
}

FVector AHexMaze::GetTileLocation(int32 Tile, float Height) const
{
	return PlanetCenter + GetTileUp(Tile) * (Radius + Height);
}

FVector AHexMaze::ProjectToSurface(const FVector& WorldLocation, float Height) const
{
	FVector Up = (WorldLocation - PlanetCenter).GetSafeNormal();
	if (Up.IsNearlyZero())
	{
		Up = FVector::UpVector;
	}
	return PlanetCenter + Up * (Radius + Height);
}

FRotator AHexMaze::GetSurfaceRotation(const FVector& WorldLocation, const FVector& Forward) const
{
	const FVector Up = (WorldLocation - PlanetCenter).GetSafeNormal();
	FVector Tangent = FVector::VectorPlaneProject(Forward, Up).GetSafeNormal();
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector::VectorPlaneProject(FVector::ForwardVector, Up).GetSafeNormal();
	}
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector::VectorPlaneProject(FVector::RightVector, Up).GetSafeNormal();
	}
	return FRotationMatrix::MakeFromXZ(Tangent, Up).Rotator();
}

int32 AHexMaze::FindNearestTile(const FVector& WorldLocation) const
{
	const FVector Direction = (WorldLocation - PlanetCenter).GetSafeNormal();

	int32 Best = INDEX_NONE;
	double BestDot = -2.0;
	for (int32 Tile = 0; Tile < Grid.Tiles.Num(); ++Tile)
	{
		const double Dot = FVector::DotProduct(Grid.Tiles[Tile].Center, Direction);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			Best = Tile;
		}
	}
	return Best;
}

TArray<int32> AHexMaze::GetNeighbors(int32 Tile) const
{
	return Grid.Tiles.IsValidIndex(Tile) ? Grid.Tiles[Tile].Neighbors : TArray<int32>();
}

TArray<int32> AHexMaze::GetOpenNeighbors(int32 Tile) const
{
	TArray<int32> Result;
	if (Grid.Tiles.IsValidIndex(Tile))
	{
		for (int32 Neighbor : Grid.Tiles[Tile].Neighbors)
		{
			if (IsPassage(Tile, Neighbor))
			{
				Result.Add(Neighbor);
			}
		}
	}
	return Result;
}

bool AHexMaze::IsPassage(int32 A, int32 B) const
{
	return Passages.Contains(FHexPlanetGrid::EdgeKey(A, B));
}

TArray<int32> AHexMaze::ComputeDistances(int32 FromTile) const
{
	TArray<int32> Distances;
	Distances.Init(INDEX_NONE, Grid.Tiles.Num());
	if (!Grid.Tiles.IsValidIndex(FromTile))
	{
		return Distances;
	}

	// Breadth-first search; every step through the maze costs the same
	TArray<int32> Queue;
	Queue.Add(FromTile);
	Distances[FromTile] = 0;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 Current = Queue[Head];
		for (int32 Neighbor : Grid.Tiles[Current].Neighbors)
		{
			if (Distances[Neighbor] == INDEX_NONE && IsPassage(Current, Neighbor))
			{
				Distances[Neighbor] = Distances[Current] + 1;
				Queue.Add(Neighbor);
			}
		}
	}
	return Distances;
}

TArray<int32> AHexMaze::FindPath(int32 FromTile, int32 ToTile) const
{
	TArray<int32> Path;
	const TArray<int32> Distances = ComputeDistances(ToTile);
	if (!Distances.IsValidIndex(FromTile) || Distances[FromTile] == INDEX_NONE)
	{
		return Path;
	}

	// Walk downhill on the distance field from the destination
	int32 Current = FromTile;
	Path.Add(Current);
	while (Current != ToTile)
	{
		for (int32 Neighbor : Grid.Tiles[Current].Neighbors)
		{
			if (IsPassage(Current, Neighbor) && Distances[Neighbor] == Distances[Current] - 1)
			{
				Current = Neighbor;
				break;
			}
		}
		Path.Add(Current);
	}
	return Path;
}
