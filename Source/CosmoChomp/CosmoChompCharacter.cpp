#include "CosmoChompCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "CosmoChomp.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "Kismet/GameplayStatics.h"
#include "PlanetaryGravityComponent.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimationAsset.h"
#include "GameFramework/DamageType.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "CosmoChompGameMode.h"
#include "CosmoChompMeshFitting.h"
#include "AlienMonster.h"
#include "RayGunPickup.h"

ACosmoChompCharacter::ACosmoChompCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Disable controller-driven character rotation (Orient to movement will handle actor rotation)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	PlanetaryGravity = CreateDefaultSubobject<UPlanetaryGravityComponent>(TEXT("PlanetaryGravity"));

	InitMovement();
	InitCameraBoom();
	InitFollowCamera();
	InitGameplayComponents();
}

void ACosmoChompCharacter::InitGameplayComponents()
{
	RayGunHolder = CreateDefaultSubobject<USceneComponent>(TEXT("RayGunHolder"));
	RayGunHolder->SetupAttachment(RootComponent);
	RayGunHolder->SetRelativeLocation(FVector(45.0f, 38.0f, -5.0f));

	RayGunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RayGunMesh"));
	RayGunMesh->SetupAttachment(RayGunHolder);
	RayGunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RayGunMesh->SetHiddenInGame(true);

	FootstepAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("FootstepAudio"));
	FootstepAudio->SetupAttachment(RootComponent);
	FootstepAudio->bAutoActivate = false;
	FootstepAudio->SetVolumeMultiplier(0.6f);

	// Imported by Tools/AssetPipeline
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Astronaut(TEXT("/Game/Meshes/Astronaut/SK_Astronaut"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> AstronautIdle(TEXT("/Game/Meshes/Astronaut/SK_AstronautIdle"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> AstronautWalk(TEXT("/Game/Meshes/Astronaut/SK_AstronautWalk"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Gun(TEXT("/Game/Meshes/RayGun/SM_RayGun"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Fire(TEXT("/Game/Input/Actions/IA_Fire"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Footsteps(TEXT("/Game/Audio/S_Footsteps"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Ough(TEXT("/Game/Audio/S_Ough"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Bang(TEXT("/Game/Audio/S_Bang"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Shoot(TEXT("/Game/Audio/S_GunShoot"));

	AstronautMesh = Astronaut.Object;
	IdleAnimation = AstronautIdle.Object;
	WalkAnimation = AstronautWalk.Object;
	RayGunMesh->SetStaticMesh(Gun.Object);
	FireAction = Fire.Object;
	FootstepAudio->SetSound(Footsteps.Object);
	HurtSound = Ough.Object;
	DropOrbsSound = Bang.Object;
	ShootSound = Shoot.Object;
}

void ACosmoChompCharacter::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	ApplyAstronautMesh();
	CosmoChompMeshFitting::FitStaticMesh(RayGunMesh, RayGunHeldSize, false);
}

void ACosmoChompCharacter::ApplyAstronautMesh()
{
	if (AstronautMesh)
	{
		// The astronaut has its own skeleton, so it plays its own animation instead of the mannequin anim blueprint
		GetMesh()->SetSkeletalMeshAsset(AstronautMesh);
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		PlayAstronautAnimation(IdleAnimation);
		CosmoChompMeshFitting::FitSkeletalMeshToCapsule(GetMesh(), GetCapsuleComponent(), AstronautHeight, AstronautYaw);
	}

	MeshBaseLocation = GetMesh()->GetRelativeLocation();
	MeshBaseRotation = GetMesh()->GetRelativeRotation();
}

void ACosmoChompCharacter::InitMovement()
{
	// Movement settings
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	MovementComp->bOrientRotationToMovement = true;
	MovementComp->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	MovementComp->JumpZVelocity = 500.f;
	MovementComp->AirControl = 0.35f;
	MovementComp->MaxWalkSpeed = 500.f;
	MovementComp->MinAnalogWalkSpeed = 20.f;
	MovementComp->BrakingDecelerationWalking = 2000.f;
	MovementComp->BrakingDecelerationFalling = 1500.0f;
	MovementComp->bOrientRotationToMovement = false;
}

void ACosmoChompCharacter::InitCameraBoom()
{
	// Camera Boom (Spring Arm)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;

	// Inherit Pawn Control Rotation so mouse/stick controls orbit the player relative to the planet surface
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = true;
}

void ACosmoChompCharacter::InitFollowCamera()
{
	// Follow Camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ACosmoChompCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACosmoChompCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ACosmoChompCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACosmoChompCharacter::Look);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &ACosmoChompCharacter::Fire);
	}
}

void ACosmoChompCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

// Used in blueprints
void ACosmoChompCharacter::DoMove(float Right, float Forward)
{
	AController* LocalController = GetController();
	if (!LocalController || !PlanetaryGravity || !IsInPlay())
	{
		return;
	}

	const FRotator ControlRot = GetController()->GetControlRotation();
	CurrentSurfaceUp = PlanetaryGravity->GetCurrentSurfaceUp();

	FVector ForwardDir = FVector::VectorPlaneProject(ControlRot.Vector(), CurrentSurfaceUp);
	if (ForwardDir.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		ForwardDir = FVector::VectorPlaneProject(GetActorForwardVector(), CurrentSurfaceUp);
	}
	ForwardDir = ForwardDir.GetSafeNormal();

	FVector RightDir = FVector::CrossProduct(CurrentSurfaceUp, ForwardDir).GetSafeNormal();

	AddMovementInput(ForwardDir, Forward);
	AddMovementInput(RightDir, Right);
}

void ACosmoChompCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

// Used in blueprints
void ACosmoChompCharacter::DoLook(float Yaw, float Pitch)
{
	LookYaw += Yaw * LookYawRate;
	LookPitch = FMath::Clamp(LookPitch + Pitch * LookPitchRate, -MaxLookPitch, MaxLookPitch);
}

// Used in blueprints
void ACosmoChompCharacter::DoJumpStart()
{
	if (!IsInPlay())
	{
		return;
	}

	// signal the character to jump
	Jump();
}

// Used in blueprints
void ACosmoChompCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void ACosmoChompCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateGameplay(DeltaTime);

	// Update planetary frame
	// ------------------------------------------------
	AController* LocalController = GetController();
	if (!LocalController || !PlanetaryGravity)
	{
		return;
	}

	CurrentSurfaceUp = PlanetaryGravity->GetCurrentSurfaceUp();
	SurfaceOrientation = PlanetaryGravity->GetSurfaceOrientation();

	FQuat YawQuat(SurfaceOrientation.GetAxisZ(), FMath::DegreesToRadians(LookYaw));
	FQuat FrameWithYaw = YawQuat * SurfaceOrientation;
	FQuat PitchQuat(FrameWithYaw.GetAxisY(), FMath::DegreesToRadians(LookPitch));
	FQuat FinalRot = PitchQuat * FrameWithYaw;

	LocalController->SetControlRotation(FinalRot.Rotator());

	// Update actor orientation to surface
	// ------------------------------------------------
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	// Face direction of travel when moving with meaningful speed; otherwise keep
	// current facing, just re-projected onto the (possibly changed) tangent plane
	// so standing still doesn't cause the body to lean into the ground.
	FVector DesiredForward;
	const FVector PlanarVelocity = FVector::VectorPlaneProject(MovementComp->Velocity, CurrentSurfaceUp);
	if (PlanarVelocity.SizeSquared() > FMath::Square(10.0f))
	{
		DesiredForward = PlanarVelocity.GetSafeNormal();
	}
	else
	{
		DesiredForward = FVector::VectorPlaneProject(GetActorForwardVector(), CurrentSurfaceUp).GetSafeNormal();
		if (DesiredForward.IsNearlyZero())
		{
			DesiredForward = FVector::VectorPlaneProject(FVector::ForwardVector, CurrentSurfaceUp).GetSafeNormal();
		}
	}

	const FQuat TargetQuat = FRotationMatrix::MakeFromXZ(DesiredForward, CurrentSurfaceUp).ToQuat();
	const FQuat NewQuat = FMath::QInterpTo(GetActorQuat(), TargetQuat, DeltaTime, BodyRotationInterpSpeed);

	// Non-sweeping: we trust our own surface-normal math and don't want collision
	// deflection from this — that sweep was the source of the drift.
	SetActorRotation(NewQuat);
}


// Gameplay
// ------------------------------------------------

void ACosmoChompCharacter::UpdateGameplay(float DeltaTime)
{
	FireCooldownRemaining -= DeltaTime;
	InvulnerabilityRemaining = FMath::Max(InvulnerabilityRemaining - DeltaTime, 0.0f);

	if (RayGunTimeRemaining > 0.0f)
	{
		RayGunTimeRemaining -= DeltaTime;
		if (RayGunTimeRemaining <= 0.0f)
		{
			RayGunTimeRemaining = 0.0f;
			RayGunMesh->SetHiddenInGame(true);
		}
	}

	UpdateMeshEffects(DeltaTime);
	UpdateFootsteps();
}

void ACosmoChompCharacter::UpdateMeshEffects(float DeltaTime)
{
	if (!IsInPlay())
	{
		return;
	}
	USkeletalMeshComponent* MeshComp = GetMesh();

	// Blink while invulnerable
	const bool bBlinkHidden = InvulnerabilityRemaining > 0.0f && FMath::Fmod(InvulnerabilityRemaining, 0.2f) < 0.1f;
	MeshComp->SetVisibility(!bBlinkHidden, true);

	// Step while moving along the ground, idle otherwise; the walk speeds up with movement speed
	const UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	const float GroundSpeed = FVector::VectorPlaneProject(GetVelocity(), CurrentSurfaceUp).Size();
	if (MovementComp->IsMovingOnGround() && GroundSpeed > WalkSpeedThreshold)
	{
		PlayAstronautAnimation(WalkAnimation);
		MeshComp->SetPlayRate(WalkPlayRate * FMath::Clamp(GroundSpeed / MovementComp->MaxWalkSpeed, 0.4f, 1.0f));
	}
	else
	{
		PlayAstronautAnimation(IdleAnimation);
		MeshComp->SetPlayRate(1.0f);
	}
}

void ACosmoChompCharacter::PlayAstronautAnimation(UAnimationAsset* Animation)
{
	if (Animation && Animation != CurrentAnimation)
	{
		CurrentAnimation = Animation;
		GetMesh()->PlayAnimation(Animation, true);
	}
}

void ACosmoChompCharacter::UpdateFootsteps()
{
	const UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	const bool bWalking = IsInPlay() && MovementComp->IsMovingOnGround()
		&& FVector::VectorPlaneProject(GetVelocity(), CurrentSurfaceUp).SizeSquared() > FMath::Square(150.0f);

	if (bWalking && !FootstepAudio->IsPlaying())
	{
		FootstepAudio->Play();
	}
	else if (!bWalking && FootstepAudio->IsPlaying())
	{
		FootstepAudio->Stop();
	}
}

void ACosmoChompCharacter::AddOrbs(int32 Count)
{
	CarriedOrbs += FMath::Max(Count, 0);
}

int32 ACosmoChompCharacter::TakeAllOrbs()
{
	const int32 Count = CarriedOrbs;
	CarriedOrbs = 0;
	return Count;
}

void ACosmoChompCharacter::EquipRayGun(float Duration)
{
	RayGunTimeRemaining = FMath::Max(RayGunTimeRemaining, Duration);
	RayGunDuration = RayGunTimeRemaining;
	RayGunMesh->SetHiddenInGame(false);
}

float ACosmoChompCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!IsInPlay() || InvulnerabilityRemaining > 0.0f || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	Health = FMath::Max(Health - FMath::Max(FMath::RoundToInt(DamageAmount), 1), 0);
	InvulnerabilityRemaining = InvulnerabilityDuration;

	if (HurtSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HurtSound, GetActorLocation());
	}

	// Getting hit knocks every carried orb loose
	if (CarriedOrbs > 0)
	{
		if (ACosmoChompGameMode* GameMode = GetWorld()->GetAuthGameMode<ACosmoChompGameMode>())
		{
			GameMode->ScatterOrbs(GetActorLocation(), CarriedOrbs);
		}
		CarriedOrbs = 0;

		if (DropOrbsSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, DropOrbsSound, GetActorLocation());
		}
	}

	if (Health <= 0)
	{
		Die();
	}

	return DamageAmount;
}

void ACosmoChompCharacter::Die()
{
	bDead = true;
	RayGunTimeRemaining = 0.0f;
	RayGunMesh->SetHiddenInGame(true);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// Topple over
	GetMesh()->SetVisibility(true, true);
	GetMesh()->SetRelativeLocationAndRotation(MeshBaseLocation, MeshBaseRotation + FRotator(0.0f, 0.0f, 90.0f));

	if (ACosmoChompGameMode* GameMode = GetWorld()->GetAuthGameMode<ACosmoChompGameMode>())
	{
		GameMode->HandlePlayerDeath(this);
	}
}

void ACosmoChompCharacter::BoardRocket()
{
	bInRocket = true;
	RayGunTimeRemaining = 0.0f;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
}

void ACosmoChompCharacter::Fire(const FInputActionValue& Value)
{
	DoFire();
}

// Used in blueprints
void ACosmoChompCharacter::DoFire()
{
	AController* LocalController = GetController();
	if (!IsInPlay() || !HasRayGun() || FireCooldownRemaining > 0.0f || !LocalController || !PlanetaryGravity)
	{
		return;
	}
	FireCooldownRemaining = FireInterval;

	// Aim where the camera looks, flattened onto the surface, and turn to face it
	const FVector Up = PlanetaryGravity->GetCurrentSurfaceUp();
	FVector Aim = FVector::VectorPlaneProject(LocalController->GetControlRotation().Vector(), Up).GetSafeNormal();
	if (Aim.IsNearlyZero())
	{
		Aim = GetActorForwardVector();
	}
	SetActorRotation(FRotationMatrix::MakeFromXZ(Aim, Up).ToQuat());

	// A straight trace would climb away from the curved ground, so the shot follows
	// a great circle around the planet at muzzle height, swept one segment at a time.
	const FVector Center = PlanetaryGravity->GetPlanetCenter();
	const FVector Muzzle = RayGunHolder->GetComponentLocation() + Aim * RayGunHeldSize * 0.5f;
	const FVector Radial = Muzzle - Center;
	const double ArcRadius = FMath::Max(Radial.Size(), 1.0);
	const FVector ArcUp = Radial / ArcRadius;
	const FVector ArcForward = FVector::VectorPlaneProject(Aim, ArcUp).GetSafeNormal();

	// The Pawn channel is blocked by walls and characters but only overlapped by pickups
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RayGun), false, this);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(20.0f);

	TArray<FVector> BeamPoints = { Muzzle };
	AActor* HitActor = nullptr;
	const int32 NumSegments = FMath::Max(FMath::CeilToInt(FireRange / 150.0f), 1);
	for (int32 Segment = 1; Segment <= NumSegments; ++Segment)
	{
		const double Angle = FireRange * Segment / NumSegments / ArcRadius;
		const FVector Point = Center + (ArcUp * FMath::Cos(Angle) + ArcForward * FMath::Sin(Angle)) * ArcRadius;

		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, BeamPoints.Last(), Point, FQuat::Identity, ECC_Pawn, Shape, Params))
		{
			BeamPoints.Add(Hit.Location);
			HitActor = Hit.GetActor();
			break;
		}
		BeamPoints.Add(Point);
	}

	if (AAlienMonster* Alien = Cast<AAlienMonster>(HitActor))
	{
		UGameplayStatics::ApplyDamage(Alien, FireDamage, LocalController, this, UDamageType::StaticClass());
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ARayGunBeam* Beam = GetWorld()->SpawnActor<ARayGunBeam>(ARayGunBeam::StaticClass(), FTransform::Identity, SpawnParams))
	{
		Beam->SetPoints(BeamPoints, BeamColor);
	}

	if (ShootSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ShootSound, Muzzle);
	}
}
