#include "BurnPhaseCharacter.h"
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
#include "BurnPhase.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "Kismet/GameplayStatics.h"

ABurnPhaseCharacter::ABurnPhaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Disable controller-driven character rotation (Orient to movement will handle actor rotation)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

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

	// Camera Boom (Spring Arm)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;

	// Inherit Pawn Control Rotation so mouse/stick controls orbit the player relative to the planet surface
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = true;

	// Follow Camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// We manually align the capsule to the planet surface each tick instead —
// see UpdateActorOrientationToSurface(). Letting CMC do it assumes a fixed world up.
	MovementComp->bOrientRotationToMovement = false;
}

void ABurnPhaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdatePlanetaryFrame(DeltaTime);
}

void ABurnPhaseCharacter::UpdatePlanetaryFrame(float DeltaTime)
{
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	AController* LocalController = GetController();

	if (!MovementComp || !LocalController)
	{
		return;
	}

	AActor* NearestPlanet = FindNearestPlanet();
	if (NearestPlanet)
	{
		CurrentPlanet = NearestPlanet;
		PlanetCenter = CurrentPlanet->GetActorLocation();
	}

	FVector NewSurfaceUp = (GetActorLocation() - PlanetCenter).GetSafeNormal();
	if (NewSurfaceUp.IsNearlyZero())
	{
		NewSurfaceUp = bSurfaceFrameInitialized ? SurfaceOrientation.GetAxisZ() : FVector::UpVector;
	}

	if (!bSurfaceFrameInitialized)
	{
		// Seed the frame once, using current actor forward as the initial reference
		FVector InitialForward = FVector::VectorPlaneProject(GetActorForwardVector(), NewSurfaceUp).GetSafeNormal();
		if (InitialForward.IsNearlyZero())
		{
			InitialForward = FVector::VectorPlaneProject(FVector::ForwardVector, NewSurfaceUp).GetSafeNormal();
		}
		SurfaceOrientation = FRotationMatrix::MakeFromXZ(InitialForward, NewSurfaceUp).ToQuat();
		bSurfaceFrameInitialized = true;
	}
	else
	{
		// Rotate the existing frame by the SMALLEST rotation that takes its old "up"
		// to the new "up" — this preserves the player's yaw/forward reference instead
		// of rebuilding it from scratch every frame.
		FVector OldSurfaceUp = SurfaceOrientation.GetAxisZ();
		FQuat DeltaUpRot = FQuat::FindBetweenNormals(OldSurfaceUp, NewSurfaceUp);
		SurfaceOrientation = DeltaUpRot * SurfaceOrientation;
		SurfaceOrientation.Normalize();
	}

	CurrentSurfaceUp = NewSurfaceUp;
	MovementComp->SetGravityDirection(-NewSurfaceUp);

	// Compose final look rotation: surface frame -> yaw around its up -> pitch around resulting right
	FQuat YawQuat(SurfaceOrientation.GetAxisZ(), FMath::DegreesToRadians(LookYaw));
	FQuat FrameWithYaw = YawQuat * SurfaceOrientation;
	FQuat PitchQuat(FrameWithYaw.GetAxisY(), FMath::DegreesToRadians(LookPitch));
	FQuat FinalRot = PitchQuat * FrameWithYaw;

	LocalController->SetControlRotation(FinalRot.Rotator());

	UpdateActorOrientationToSurface(DeltaTime, CurrentSurfaceUp);
}

void ABurnPhaseCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		const FRotator ControlRot = GetController()->GetControlRotation();
		const FVector SurfaceUp = GetCurrentSurfaceUp(); // always current, no per-frame lag

		// Use the controller's forward vector projected onto the local surface plane.
		// Previously we constructed a yaw-only rotator with zero pitch/roll which
		// treated yaw as rotation about world-up; that causes incorrect directions
		// when the local surface up differs from world up (e.g. near the equator)
		// and leads to drifting/locking movement. Projecting the controller's
		// forward vector onto the surface plane preserves the intended heading
		// relative to the surface.
		FVector ForwardDir = FVector::VectorPlaneProject(ControlRot.Vector(), SurfaceUp);
		if (ForwardDir.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			ForwardDir = FVector::VectorPlaneProject(GetActorForwardVector(), SurfaceUp);
		}
		ForwardDir = ForwardDir.GetSafeNormal();

		FVector RightDir = FVector::CrossProduct(SurfaceUp, ForwardDir).GetSafeNormal();

		AddMovementInput(ForwardDir, Forward);
		AddMovementInput(RightDir, Right);
	}
}

void ABurnPhaseCharacter::DoLook(float Yaw, float Pitch)
{
	LookYaw += Yaw * LookYawRate;
	LookPitch = FMath::Clamp(LookPitch + Pitch * LookPitchRate, -MaxLookPitch, MaxLookPitch);
}

void ABurnPhaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABurnPhaseCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ABurnPhaseCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABurnPhaseCharacter::Look);
	}
}

void ABurnPhaseCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void ABurnPhaseCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

AActor* ABurnPhaseCharacter::FindNearestPlanet()
{
	// Use gameplay utilities to find actors tagged as "Planet" within the level.
	TArray<AActor*> FoundPlanets;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Planet"), FoundPlanets);

	AActor* ClosestPlanet = nullptr;
	float MinDistanceSq = FLT_MAX;

	for (AActor* PlanetActor : FoundPlanets)
	{
		if (!PlanetActor)
		{
			continue;
		}

		float DistSq = FVector::DistSquared(GetActorLocation(), PlanetActor->GetActorLocation());
		if (DistSq <= GravitySearchRadius * GravitySearchRadius && DistSq < MinDistanceSq)
		{
			MinDistanceSq = DistSq;
			ClosestPlanet = PlanetActor;
		}
	}

	return ClosestPlanet;
}

FVector ABurnPhaseCharacter::GetCurrentSurfaceUp() const
{
	FVector Up = (GetActorLocation() - PlanetCenter).GetSafeNormal();
	if (Up.IsNearlyZero())
	{
		return bSurfaceFrameInitialized ? SurfaceOrientation.GetAxisZ() : FVector::UpVector;
	}
	return Up;
}

void ABurnPhaseCharacter::UpdateActorOrientationToSurface(float DeltaTime, const FVector& SurfaceUp)
{
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	// Face direction of travel when moving with meaningful speed; otherwise keep
	// current facing, just re-projected onto the (possibly changed) tangent plane
	// so standing still doesn't cause the body to lean into the ground.
	FVector DesiredForward;
	const FVector PlanarVelocity = FVector::VectorPlaneProject(MovementComp->Velocity, SurfaceUp);
	if (PlanarVelocity.SizeSquared() > FMath::Square(10.0f))
	{
		DesiredForward = PlanarVelocity.GetSafeNormal();
	}
	else
	{
		DesiredForward = FVector::VectorPlaneProject(GetActorForwardVector(), SurfaceUp).GetSafeNormal();
		if (DesiredForward.IsNearlyZero())
		{
			DesiredForward = FVector::VectorPlaneProject(FVector::ForwardVector, SurfaceUp).GetSafeNormal();
		}
	}

	const FQuat TargetQuat = FRotationMatrix::MakeFromXZ(DesiredForward, SurfaceUp).ToQuat();
	const FQuat NewQuat = FMath::QInterpTo(GetActorQuat(), TargetQuat, DeltaTime, BodyRotationInterpSpeed);

	// Non-sweeping: we trust our own surface-normal math and don't want collision
	// deflection from this — that sweep was the source of the drift.
	SetActorRotation(NewQuat);
}

void ABurnPhaseCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void ABurnPhaseCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}