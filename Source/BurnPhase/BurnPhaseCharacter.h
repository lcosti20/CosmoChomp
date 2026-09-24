#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "BurnPhaseCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;

UCLASS(config = Game)
class ABurnPhaseCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MouseLookAction;

public:
	ABurnPhaseCharacter();

	virtual void Tick(float DeltaTime) override;

protected:
	// Accumulated look input, independent of engine ControlRotation
	float LookYaw = 0.0f;
	float LookPitch = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Planetary Gravity")
	float LookYawRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Planetary Gravity")
	float LookPitchRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Planetary Gravity")
	float MaxLookPitch = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planetary Gravity")
	float BodyRotationInterpSpeed = 10.0f;

	/** Always-fresh surface-up vector computed from current position (no caching/lag) */
	FVector GetCurrentSurfaceUp() const;

	void UpdateActorOrientationToSurface(float DeltaTime, const FVector& SurfaceUp);

	// Persistent, incrementally-updated surface-aligned frame (no yaw/pitch baked in)
	FQuat SurfaceOrientation = FQuat::Identity;

	// Cached each tick so DoMove and camera code agree on the same "up"
	FVector CurrentSurfaceUp = FVector::UpVector;

	bool bSurfaceFrameInitialized = false;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void DoMove(float Right, float Forward);
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void DoLook(float Yaw, float Pitch);
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void DoJumpStart();
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void DoJumpEnd();

	/** World location of the current planet center */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planetary Gravity")
	FVector PlanetCenter = FVector::ZeroVector;

	/** Adjusts player controller and gravity vector to align with local surface normal */
	void UpdatePlanetaryFrame(float DeltaTime);

	/** Active planet actor pulling the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planetary Gravity")
	AActor* CurrentPlanet;

	/** Radius around character to search for gravity sources */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planetary Gravity")
	float GravitySearchRadius = 10000.0f; // 100 meters

	/** Utility to find the nearest planet actor in range */
	AActor* FindNearestPlanet();
};