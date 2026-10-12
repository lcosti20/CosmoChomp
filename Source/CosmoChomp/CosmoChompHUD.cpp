#include "CosmoChompHUD.h"
#include "CanvasItem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GlobalRenderResources.h"
#include "AlienMonster.h"
#include "CosmoChompCharacter.h"
#include "CosmoChompGameMode.h"
#include "CosmoRocket.h"
#include "EnergyOrb.h"
#include "HexMaze.h"
#include "RayGunPickup.h"

namespace
{
	/** Where the mini map sits on screen and how the planet is oriented on it */
	struct FMiniMapView
	{
		FVector PlanetCenter;
		FVector Up;       // unit direction from the planet center to the player (the map's center)
		FVector Forward;  // camera heading along the surface (map up)
		FVector Right;    // map right
		FVector2D Origin; // screen position of the map's center
		double Radius;    // screen radius of the map
		double Range;     // great-circle angle (radians) at the map's rim

		/** Azimuthal equidistant projection of a unit direction from the planet center */
		/** If bClampToRim, things beyond the map's range are pinned to its rim (along their bearing) instead of hidden */
		bool Project(const FVector& Direction, FVector2D& OutScreen, bool bClampToRim = false) const
		{
			const double Cos = FMath::Clamp(FVector::DotProduct(Up, Direction), -1.0, 1.0);
			const double Angle = FMath::Acos(Cos);
			if (Angle > Range && !bClampToRim)
			{
				return false;
			}
			// Bearing from the player, measured in the tangent plane at the player
			const FVector Tangent = Direction - Up * Cos;
			const FVector2D Bearing = FVector2D(FVector::DotProduct(Tangent, Right), FVector::DotProduct(Tangent, Forward)).GetSafeNormal();
			OutScreen = Origin + FVector2D(Bearing.X, -Bearing.Y) * (FMath::Min(Angle / Range, 1.0) * Radius);
			return true;
		}

		bool ProjectLocation(const FVector& WorldLocation, FVector2D& OutScreen, bool bClampToRim = false) const
		{
			return Project((WorldLocation - PlanetCenter).GetSafeNormal(), OutScreen, bClampToRim);
		}

		/** Screen direction for a world direction along the surface at the player */
		FVector2D ScreenDirection(const FVector& WorldDirection) const
		{
			return FVector2D(FVector::DotProduct(WorldDirection, Right), -FVector::DotProduct(WorldDirection, Forward)).GetSafeNormal();
		}
	};

	/** Everything on the map is batched into one list of flat-colored triangles */
	struct FMiniMapBatch
	{
		TArray<FCanvasUVTri> Triangles;

		void Triangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
		{
			FCanvasUVTri& Tri = Triangles.AddDefaulted_GetRef();
			Tri.V0_Pos = A;
			Tri.V1_Pos = B;
			Tri.V2_Pos = C;
			Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
		}

		void Line(const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Color)
		{
			const FVector2D Side = (B - A).GetSafeNormal().GetRotated(90.0f) * Thickness * 0.5f;
			if (Side.IsNearlyZero())
			{
				return;
			}
			Triangle(A + Side, B + Side, B - Side, Color);
			Triangle(A + Side, B - Side, A - Side, Color);
		}

		void Disc(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Sides = 10)
		{
			for (int32 Index = 0; Index < Sides; ++Index)
			{
				const float A0 = 2.0f * PI * Index / Sides;
				const float A1 = 2.0f * PI * (Index + 1) / Sides;
				Triangle(Center, Center + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius, Center + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius, Color);
			}
		}

		void Ring(const FVector2D& Center, float Radius, float Thickness, const FLinearColor& Color, int32 Sides = 64)
		{
			for (int32 Index = 0; Index < Sides; ++Index)
			{
				const float A0 = 2.0f * PI * Index / Sides;
				const float A1 = 2.0f * PI * (Index + 1) / Sides;
				Line(Center + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius, Center + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius, Thickness, Color);
			}
		}

		void Square(const FVector2D& Center, float HalfSize, const FLinearColor& Color)
		{
			const FVector2D X(HalfSize, 0.0f), Y(0.0f, HalfSize);
			Triangle(Center - X - Y, Center + X - Y, Center + X + Y, Color);
			Triangle(Center - X - Y, Center + X + Y, Center - X + Y, Color);
		}

		/** Isosceles triangle pointing along Direction (a unit screen vector) */
		void Arrow(const FVector2D& Center, const FVector2D& Direction, float Size, const FLinearColor& Color)
		{
			const FVector2D Side = Direction.GetRotated(90.0f);
			Triangle(Center + Direction * Size, Center - Direction * Size * 0.7f + Side * Size * 0.8f, Center - Direction * Size * 0.7f - Side * Size * 0.8f, Color);
		}
	};
}

void ACosmoChompHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !GEngine)
	{
		return;
	}

	UFont* Font = GEngine->GetLargeFont();
	const float Scale = FMath::Max(Canvas->ClipY / 1080.0f, 0.5f) * 1.4f;
	const float LineHeight = 34.0f * Scale;
	const float Left = 40.0f * Scale;
	float Y = 32.0f * Scale;

	// Only levels running a Cosmo Chomp round (with a maze) get the HUD
	const ACosmoChompGameMode* GameMode = GetWorld()->GetAuthGameMode<ACosmoChompGameMode>();
	if (!GameMode || !GameMode->GetMaze())
	{
		return;
	}

	const ACosmoChompCharacter* Player = Cast<ACosmoChompCharacter>(GetOwningPawn());
	const ACosmoRocket* Rocket = GameMode->GetRocket();

	if (bShowMiniMap)
	{
		DrawMiniMap(GameMode, GetOwningPawn());
	}

	if (Player)
	{
		DrawText(FString::Printf(TEXT("Health  %d / %d"), Player->GetHealth(), Player->GetMaxHealth()), TextColor, Left, Y, Font, Scale);
		Y += LineHeight;
		DrawText(FString::Printf(TEXT("Carrying  %d orbs"), Player->GetCarriedOrbs()), TextColor, Left, Y, Font, Scale);
		Y += LineHeight;
	}

	if (Rocket)
	{
		const int32 Required = FMath::Max(Rocket->RequiredOrbs, 1);
		DrawText(FString::Printf(TEXT("Rocket fuel  %d / %d"), Rocket->GetStoredOrbs(), Required), TextColor, Left, Y, Font, Scale);
		Y += LineHeight;
		DrawBar(Left, Y, 300.0f * Scale, 12.0f * Scale, float(Rocket->GetStoredOrbs()) / Required, FuelColor);
		Y += LineHeight;
	}

	if (Player && Player->HasRayGun())
	{
		DrawRayGunTimer(Player, Font, Scale);
	}

	const FString Banner = GameMode->GetBannerText();
	if (!Banner.IsEmpty())
	{
		const float BannerScale = Scale * 1.4f;
		float Width, Height;
		GetTextSize(Banner, Width, Height, Font, BannerScale);
		const float X = (Canvas->ClipX - Width) * 0.5f;
		const float BannerY = Canvas->ClipY * 0.22f;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), X - 24.0f, BannerY - 12.0f, Width + 48.0f, Height + 24.0f);
		DrawText(Banner, TextColor, X, BannerY, Font, BannerScale);
	}
}

void ACosmoChompHUD::DrawMiniMap(const ACosmoChompGameMode* GameMode, const APawn* Player)
{
	const AHexMaze* Maze = GameMode->GetMaze();
	if (!Player || !Maze || Maze->GetNumTiles() == 0)
	{
		return;
	}

	FMiniMapView View;
	View.PlanetCenter = Maze->GetPlanetCenter();
	View.Up = (Player->GetActorLocation() - View.PlanetCenter).GetSafeNormal();
	if (View.Up.IsNearlyZero())
	{
		return;
	}

	// Map up is where the camera looks, flattened onto the surface; looking straight
	// down, the camera's own up takes over
	FVector Heading = Player->GetActorForwardVector();
	if (PlayerOwner && PlayerOwner->PlayerCameraManager)
	{
		const FRotationMatrix Camera(PlayerOwner->PlayerCameraManager->GetCameraRotation());
		Heading = Camera.GetUnitAxis(EAxis::X);
		if (FVector::VectorPlaneProject(Heading, View.Up).SizeSquared() < 0.01)
		{
			Heading = Camera.GetUnitAxis(EAxis::Z);
		}
	}
	View.Forward = FVector::VectorPlaneProject(Heading, View.Up).GetSafeNormal();
	if (View.Forward.IsNearlyZero())
	{
		View.Forward = FVector::VectorPlaneProject(Player->GetActorForwardVector(), View.Up).GetSafeNormal();
	}
	View.Right = FVector::CrossProduct(View.Up, View.Forward);

	// Top-right corner, never more than a quarter of the screen
	const float Shorter = FMath::Min(Canvas->ClipX, Canvas->ClipY);
	const float Margin = 0.03f * Shorter;
	View.Radius = FMath::Min(MiniMapSize * Shorter, 0.5f * Shorter - 2.0f * Margin) * 0.5f;
	View.Origin = FVector2D(Canvas->ClipX - Margin - View.Radius, Margin + View.Radius);
	View.Range = FMath::DegreesToRadians(FMath::Clamp(MiniMapRangeDegrees, 10.0f, 180.0f));

	const float Unit = View.Radius / 40.0f;
	FMiniMapBatch Batch;
	Batch.Disc(View.Origin, View.Radius, MiniMapBackgroundColor, 64);

	// Tile edges: faint where open, bright where walled. Each edge is split up because
	// great circles bend under the projection, more so toward the rim.
	const FHexPlanetGrid& Grid = Maze->GetGrid();
	constexpr int32 EdgeSteps = 4;
	const float TileThickness = FMath::Max(Unit * 0.3f, 1.0f);
	const float WallThickness = FMath::Max(Unit * 0.6f, 1.5f);
	for (int32 Tile = 0; Tile < Grid.Tiles.Num(); ++Tile)
	{
		for (const int32 Neighbor : Grid.Tiles[Tile].Neighbors)
		{
			int32 CornerA, CornerB;
			if (Neighbor < Tile || !Grid.GetEdgeCorners(Tile, Neighbor, CornerA, CornerB))
			{
				continue;
			}

			const bool bWall = !Maze->IsPassage(Tile, Neighbor);
			const FVector& A = Grid.Corners[CornerA];
			const FVector& B = Grid.Corners[CornerB];
			FVector2D Previous;
			bool bHasPrevious = View.Project(A, Previous);
			for (int32 Step = 1; Step <= EdgeSteps; ++Step)
			{
				FVector2D Point;
				const bool bVisible = View.Project(FMath::Lerp(A, B, float(Step) / EdgeSteps).GetSafeNormal(), Point);
				// Near the rim the far side of the planet wraps around the circle; skip
				// pieces that would jump across it
				if (bVisible && bHasPrevious && FVector2D::Distance(Previous, Point) < View.Radius * 0.3)
				{
					Batch.Line(Previous, Point, bWall ? WallThickness : TileThickness, bWall ? MiniMapWallColor : MiniMapTileColor);
				}
				Previous = Point;
				bHasPrevious = bVisible;
			}
		}
	}

	FVector2D Screen;
	for (TActorIterator<AEnergyOrb> It(GetWorld()); It; ++It)
	{
		if (View.ProjectLocation(It->GetActorLocation(), Screen))
		{
			Batch.Disc(Screen, Unit * 0.45f, MiniMapOrbColor, 8);
		}
	}

	for (TActorIterator<AAlienMonster> It(GetWorld()); It; ++It)
	{
		if (It->GetState() != EAlienState::Dead && View.ProjectLocation(It->GetActorLocation(), Screen))
		{
			Batch.Square(Screen, Unit * 1.0f, MiniMapAlienColor);
		}
	}

	// The rocket and ray gun stay on the rim when out of range, pointing the way to them
	for (TActorIterator<ARayGunPickup> It(GetWorld()); It; ++It)
	{
		if (View.ProjectLocation(It->GetActorLocation(), Screen, true))
		{
			Batch.Arrow(Screen, FVector2D(1.0f, 0.0f), Unit * 1.6f, MiniMapRayGunColor);
		}
	}

	if (const ACosmoRocket* Rocket = GameMode->GetRocket())
	{
		if (View.ProjectLocation(Rocket->GetActorLocation(), Screen, true))
		{
			Batch.Arrow(Screen, FVector2D(0.0f, -1.0f), Unit * 1.9f, MiniMapRocketColor);
		}
	}

	// The player, pointing where the astronaut faces
	Batch.Arrow(View.Origin, View.ScreenDirection(Player->GetActorForwardVector()), Unit * 1.5f, MiniMapPlayerColor);

	Batch.Ring(View.Origin, View.Radius, FMath::Max(Unit * 0.5f, 1.5f), MiniMapWallColor);

	FCanvasTriangleItem Item(Batch.Triangles, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ACosmoChompHUD::DrawRayGunTimer(const ACosmoChompCharacter* Player, UFont* Font, float Scale)
{
	// Centered near the bottom: seconds left and a bar that drains with them
	const float Remaining = Player->GetRayGunTimeRemaining();
	const float Fill = Remaining / FMath::Max(Player->GetRayGunDuration(), KINDA_SMALL_NUMBER);
	const FString Label = FString::Printf(TEXT("RAY GUN  %ds"), FMath::CeilToInt(Remaining));
	const FString Hint = TEXT("Click / F / RT to fire");

	const float BarWidth = 420.0f * Scale;
	const float BarHeight = 16.0f * Scale;
	const float X = (Canvas->ClipX - BarWidth) * 0.5f;
	float Y = Canvas->ClipY - 130.0f * Scale;

	// Blink for the last few seconds so running out isn't a surprise
	const bool bRunningOut = Remaining < RayGunWarningTime;
	const bool bBlinkOff = bRunningOut && FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 0.4f) < 0.2f;
	const FLinearColor Color = bBlinkOff ? TextColor : RayGunColor;

	float LabelWidth, LabelHeight;
	GetTextSize(Label, LabelWidth, LabelHeight, Font, Scale);
	const float Padding = 12.0f * Scale;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), X - Padding, Y - Padding, BarWidth + Padding * 2.0f, LabelHeight + 8.0f * Scale + BarHeight + Padding * 2.0f);
	DrawText(Label, Color, X, Y, Font, Scale);

	const float HintScale = Scale * 0.7f;
	float HintWidth, HintHeight;
	GetTextSize(Hint, HintWidth, HintHeight, Font, HintScale);
	DrawText(Hint, TextColor, X + BarWidth - HintWidth, Y + (LabelHeight - HintHeight) * 0.5f, Font, HintScale);

	DrawBar(X, Y + LabelHeight + 8.0f * Scale, BarWidth, BarHeight, Fill, Color);
}

void ACosmoChompHUD::DrawBar(float X, float Y, float Width, float Height, float Fill, const FLinearColor& Color)
{
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), X, Y, Width, Height);
	DrawRect(Color, X, Y, Width * FMath::Clamp(Fill, 0.0f, 1.0f), Height);
}
