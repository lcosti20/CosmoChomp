#include "CosmoChompHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CosmoChompCharacter.h"
#include "CosmoChompGameMode.h"
#include "CosmoRocket.h"

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
		DrawText(FString::Printf(TEXT("Ray gun  %.1fs  (click to fire)"), Player->GetRayGunTimeRemaining()), RayGunColor, Left, Y, Font, Scale);
		Y += LineHeight;
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

void ACosmoChompHUD::DrawBar(float X, float Y, float Width, float Height, float Fill, const FLinearColor& Color)
{
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), X, Y, Width, Height);
	DrawRect(Color, X, Y, Width * FMath::Clamp(Fill, 0.0f, 1.0f), Height);
}
