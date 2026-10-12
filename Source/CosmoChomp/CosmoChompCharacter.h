#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "CosmoChompCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UPlanetaryGravityComponent;
class UStaticMeshComponent;
class UAudioComponent;
class USkeletalMesh;
class UAnimationAsset;
class USoundBase;

UCLASS(config = Game)
class ACosmoChompCharacter : public ACharacter
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* FireAction;

	/** Positions the held ray gun next to the astronaut's right hand */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ray Gun", meta = (AllowPrivateAccess = "true"))
	USceneComponent* RayGunHolder;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ray Gun", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* RayGunMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio", meta = (AllowPrivateAccess = "true"))
	UAudioComponent* FootstepAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planetary Gravity", meta = (AllowPrivateAccess = "true"))
	UPlanetaryGravityComponent* PlanetaryGravity;

	void InitMovement();
	void InitCameraBoom();
	void InitFollowCamera();
	void InitGameplayComponents();

public:
	ACosmoChompCharacter();

	virtual void Tick(float DeltaTime) override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** False once the player has died or boarded the rocket */
	UFUNCTION(BlueprintPure, Category = "Cosmo Chomp")
	bool IsInPlay() const { return !bDead && !bInRocket; }

	UFUNCTION(BlueprintPure, Category = "Cosmo Chomp")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Cosmo Chomp")
	int32 GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Cosmo Chomp")
	int32 GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Cosmo Chomp")
	int32 GetCarriedOrbs() const { return CarriedOrbs; }

	UFUNCTION(BlueprintPure, Category = "Cosmo Chomp")
	bool HasRayGun() const { return RayGunTimeRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Cosmo Chomp")
	float GetRayGunTimeRemaining() const { return RayGunTimeRemaining; }

	UFUNCTION(BlueprintCallable, Category = "Cosmo Chomp")
	void AddOrbs(int32 Count);

	/** Hands over every carried orb (to the rocket) */
	UFUNCTION(BlueprintCallable, Category = "Cosmo Chomp")
	int32 TakeAllOrbs();

	UFUNCTION(BlueprintCallable, Category = "Cosmo Chomp")
	void EquipRayGun(float Duration);

	/** Hides the player inside the rocket for the launch */
	UFUNCTION(BlueprintCallable, Category = "Cosmo Chomp")
	void BoardRocket();

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

	// Persistent, incrementally-updated surface-aligned frame (no yaw/pitch baked in)
	FQuat SurfaceOrientation = FQuat::Identity;

	// Cached each tick so DoMove and camera code agree on the same "up"
	FVector CurrentSurfaceUp = FVector::UpVector;

	bool bSurfaceFrameInitialized = false;

	// Gameplay
	// ------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp")
	int32 MaxHealth = 3;

	/** Seconds of invulnerability after being hurt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmo Chomp")
	float InvulnerabilityDuration = 2.0f;

	/** Seconds between ray gun shots while fire is held */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ray Gun")
	float FireInterval = 0.25f;

	/** Shots follow the planet's curvature for this distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ray Gun")
	float FireRange = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ray Gun")
	float FireDamage = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ray Gun")
	FLinearColor BeamColor = FLinearColor(1.0f, 0.1f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ray Gun")
	float RayGunHeldSize = 55.0f;

	int32 Health = 3;
	int32 CarriedOrbs = 0;
	float RayGunTimeRemaining = 0.0f;
	float InvulnerabilityRemaining = 0.0f;
	float FireCooldownRemaining = 0.0f;
	bool bDead = false;
	bool bInRocket = false;

	// Presentation
	// ------------------------------------------------

	/** Replaces the Blueprint's mesh at runtime when set */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astronaut")
	USkeletalMesh* AstronautMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astronaut")
	UAnimationAsset* AstronautAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astronaut")
	float AstronautHeight = 175.0f;

	/** The model faces +Y; rotate it to face the actor's +X */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astronaut")
	float AstronautYaw = -90.0f;

	/** Height of the procedural hop while walking (the model only has an idle animation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astronaut")
	float WalkBobHeight = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	USoundBase* HurtSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	USoundBase* DropOrbsSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	USoundBase* ShootSound;

	FVector MeshBaseLocation = FVector::ZeroVector;
	FRotator MeshBaseRotation = FRotator::ZeroRotator;
	float WalkCycle = 0.0f;

	virtual void BeginPlay() override;

	void ApplyAstronautMesh();
	void UpdateGameplay(float DeltaTime);
	void UpdateMeshEffects(float DeltaTime);
	void UpdateFootsteps();
	void Die();

	void Fire(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category = "Ray Gun")
	void DoFire();

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
};