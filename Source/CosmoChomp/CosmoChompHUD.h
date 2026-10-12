#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CosmoChompHUD.generated.h"

/**
 *  Canvas HUD: health, carried orbs, rocket fuel, ray gun timer, mini map and round banners.
 */
UCLASS()
class ACosmoChompHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UPROPERTY(EditAnywhere, Category = "HUD")
	FLinearColor TextColor = FLinearColor(0.9f, 0.95f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "HUD")
	FLinearColor FuelColor = FLinearColor(0.15f, 1.0f, 0.55f);

	UPROPERTY(EditAnywhere, Category = "HUD")
	FLinearColor RayGunColor = FLinearColor(1.0f, 0.2f, 0.65f);

	/** The ray gun timer blinks once fewer than this many seconds are left */
	UPROPERTY(EditAnywhere, Category = "HUD")
	float RayGunWarningTime = 3.0f;

	// Mini map: an azimuthal equidistant projection of the planet, centered on the
	// player with the camera's heading pointing up. Distance from the map's center
	// is true great-circle distance; at a range of 180 degrees the whole planet fits
	// in the circle (the point opposite the player is its rim).
	// ------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Mini Map")
	bool bShowMiniMap = true;

	/** Map diameter as a fraction of the screen's shorter side; capped so it stays within one quadrant */
	UPROPERTY(EditAnywhere, Category = "Mini Map", meta = (ClampMin = "0.1", ClampMax = "0.5"))
	float MiniMapSize = 0.34f;

	/** Great-circle distance (degrees) from the player to the map's rim; 180 shows the whole planet */
	UPROPERTY(EditAnywhere, Category = "Mini Map", meta = (ClampMin = "10", ClampMax = "180"))
	float MiniMapRangeDegrees = 75.0f;

	UPROPERTY(EditAnywhere, Category = "Mini Map")
	FLinearColor MiniMapBackgroundColor = FLinearColor(0.01f, 0.02f, 0.06f, 0.78f);

	/** Tile edges that are open passages */
	UPROPERTY(EditAnywhere, Category = "Mini Map")
	FLinearColor MiniMapTileColor = FLinearColor(0.35f, 0.45f, 0.7f, 0.3f);

	UPROPERTY(EditAnywhere, Category = "Mini Map")
	FLinearColor MiniMapWallColor = FLinearColor(0.8f, 0.88f, 1.0f, 0.95f);

	UPROPERTY(EditAnywhere, Category = "Mini Map")
	FLinearColor MiniMapOrbColor = FLinearColor(0.35f, 1.0f, 0.85f);

	UPROPERTY(EditAnywhere, Category = "Mini Map")
	FLinearColor MiniMapRocketColor = FLinearColor(1.0f, 0.12f, 0.1f);

	UPROPERTY(EditAnywhere, Category = "Mini Map")
	FLinearColor MiniMapRayGunColor = FLinearColor(0.2f, 1.0f, 0.25f);

	UPROPERTY(EditAnywhere, Category = "Mini Map")
	FLinearColor MiniMapAlienColor = FLinearColor(1.0f, 0.9f, 0.1f);

	UPROPERTY(EditAnywhere, Category = "Mini Map")
	FLinearColor MiniMapPlayerColor = FLinearColor::White;

	void DrawMiniMap(const class ACosmoChompGameMode* GameMode, const APawn* Player);
	void DrawRayGunTimer(const class ACosmoChompCharacter* Player, class UFont* Font, float Scale);
	void DrawBar(float X, float Y, float Width, float Height, float Fill, const FLinearColor& Color);
};
