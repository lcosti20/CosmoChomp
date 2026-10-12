#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CosmoChompHUD.generated.h"

/**
 *  Canvas HUD: health, carried orbs, rocket fuel, ray gun charge and round banners.
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

	void DrawRayGunTimer(const class ACosmoChompCharacter* Player, class UFont* Font, float Scale);
	void DrawBar(float X, float Y, float Width, float Height, float Fill, const FLinearColor& Color);
};
