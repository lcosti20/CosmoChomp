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

	InitMovement();
	InitCameraBoom();
	InitFollowCamera();
}

void ABurnPhaseCharacter::InitMovement()
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

void ABurnPhaseCharacter::InitCameraBoom()
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

void ABurnPhaseCharacter::InitFollowCamera()
{
	// Follow Camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ABurnPhaseCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

// Used in blueprints
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

void ABurnPhaseCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

// Used in blueprints
void ABurnPhaseCharacter::DoLook(float Yaw, float Pitch)
{
	LookYaw += Yaw * LookYawRate;
	LookPitch = FMath::Clamp(LookPitch + Pitch * LookPitchRate, -MaxLookPitch, MaxLookPitch);
}

// Used in blueprints
void ABurnPhaseCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

// Used in blueprints
void ABurnPhaseCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
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

	// If we don't have movement or controller, nothing to update this frame.
	if (!MovementComp || !LocalController)
	{
		return;
	}

	// Find and set the nearest planet as the current planetary reference.
	SetPlanet(FindNearestPlanet());

	// Compute the surface "up" vector: from planet center to actor location.
	// This defines the local vertical for the character relative to the planet.
	FVector NewSurfaceUp = (GetActorLocation() - PlanetCenter).GetSafeNormal();

	// If the computed up is degenerate (actor exactly at center or too close),
	// fall back to either the previously initialized surface up or the world up.
	if (NewSurfaceUp.IsNearlyZero())
	{
		NewSurfaceUp = bSurfaceFrameInitialized ? SurfaceOrientation.GetAxisZ() : FVector::UpVector;
	}

	// If this is the first frame we initialize the surface frame from the actor's forward.
	if (!bSurfaceFrameInitialized)
	{
		// Seed the frame once, using current actor forward as the initial reference.
		// Project the forward vector onto the tangent plane (plane perpendicular to up)
		// so that the forward is horizontal relative to the surface.
		FVector InitialForward = FVector::VectorPlaneProject(GetActorForwardVector(), NewSurfaceUp).GetSafeNormal();
		if (InitialForward.IsNearlyZero())
		{
			// If actor forward was degenerate, use world forward projected onto the surface.
			InitialForward = FVector::VectorPlaneProject(FVector::ForwardVector, NewSurfaceUp).GetSafeNormal();
		}

		// Build a rotation (frame) using forward as X and surface up as Z.
		SurfaceOrientation = FRotationMatrix::MakeFromXZ(InitialForward, NewSurfaceUp).ToQuat();
		bSurfaceFrameInitialized = true;
	}
	else
	{
		// For subsequent frames, compute the smallest rotation that takes the old up
		// direction to the new up direction. This preserves the player's yaw (heading)
		// relative to the surface instead of reconstructing the entire frame,
		// which would cause unwanted yaw drift.
		FVector OldSurfaceUp = SurfaceOrientation.GetAxisZ();
		FQuat DeltaUpRot = FQuat::FindBetweenNormals(OldSurfaceUp, NewSurfaceUp);

		// Apply the delta rotation to the existing surface orientation and normalize.
		SurfaceOrientation = DeltaUpRot * SurfaceOrientation;
		SurfaceOrientation.Normalize();
	}

	// Cache the current surface up for other systems and set movement gravity direction
	// to push the character toward the planet (negative of up).
	CurrentSurfaceUp = NewSurfaceUp;
	MovementComp->SetGravityDirection(-NewSurfaceUp);

	// Compose the final control rotation that the player should have:
	// 1) Start with the surface frame (SurfaceOrientation).
	// 2) Apply yaw rotation around the surface up (preserves heading relative to surface).
	// 3) Apply pitch rotation around the resulting right axis.
	//
	// This order ensures yaw is interpreted relative to the surface's up and that
	// pitch rotates the camera/player around the right axis after yaw.
	FQuat YawQuat(SurfaceOrientation.GetAxisZ(), FMath::DegreesToRadians(LookYaw));
	FQuat FrameWithYaw = YawQuat * SurfaceOrientation;
	FQuat PitchQuat(FrameWithYaw.GetAxisY(), FMath::DegreesToRadians(LookPitch));
	FQuat FinalRot = PitchQuat * FrameWithYaw;

	// Apply the composed rotation to the local controller so camera/input aligns with surface.
	LocalController->SetControlRotation(FinalRot.Rotator());

	// Smoothly or immediately orient the actor mesh/rotation to match the surface frame.
	// This typically adjusts the actor's up to match CurrentSurfaceUp over time (DeltaTime).
	UpdateActorOrientationToSurface(DeltaTime, CurrentSurfaceUp);
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

void ABurnPhaseCharacter::SetPlanet(AActor* Planet)
{
	if (Planet)
	{
		CurrentPlanet = Planet;
		PlanetCenter = CurrentPlanet->GetActorLocation();
	}
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
