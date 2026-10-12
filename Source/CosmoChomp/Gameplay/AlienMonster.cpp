#include "AlienMonster.h"
#include "AIController.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "CosmoChompCharacter.h"
#include "CosmoChompMeshFitting.h"
#include "HexMaze.h"
#include "PlanetaryGravityComponent.h"

AAlienMonster::AAlienMonster()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(50.0f, 90.0f);

	// Rotation is driven from Tick to stay aligned with the planet surface
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	MovementComp->bOrientRotationToMovement = false;
	MovementComp->MaxWalkSpeed = WanderSpeed;
	MovementComp->BrakingDecelerationWalking = 1500.0f;

	// Needs a controller for character movement to run
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PlanetaryGravity = CreateDefaultSubobject<UPlanetaryGravityComponent>(TEXT("PlanetaryGravity"));

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Meshes/Alien/SK_Alien"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Meshes/Alien/SK_AlienArmature_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Walk(TEXT("/Game/Meshes/Alien/SK_AlienArmature_Walk"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Attack(TEXT("/Game/Meshes/Alien/SK_AlienArmature_Running_attack"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Claw(TEXT("/Game/Audio/S_ClawAttack"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Slime(TEXT("/Game/Audio/S_Slime"));

	AlienMesh = Body.Object;
	IdleAnimation = Idle.Object;
	WalkAnimation = Walk.Object;
	AttackAnimation = Attack.Object;
	AttackSound = Claw.Object;
	DeathSound = Slime.Object;
}

void AAlienMonster::BeginPlay()
{
	Super::BeginPlay();

	if (AlienMesh)
	{
		GetMesh()->SetSkeletalMeshAsset(AlienMesh);
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		CosmoChompMeshFitting::FitSkeletalMeshToCapsule(GetMesh(), GetCapsuleComponent(), MeshHeight, MeshYaw);
	}
	PlayLoopingAnimation(IdleAnimation);

	for (TActorIterator<AHexMaze> It(GetWorld()); It; ++It)
	{
		Maze = *It;
		break;
	}

	// Stagger route updates between aliens
	RepathTimer = FMath::FRandRange(0.0f, RepathInterval);
}

ACosmoChompCharacter* AAlienMonster::GetPlayer() const
{
	ACosmoChompCharacter* Player = Cast<ACosmoChompCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	return Player && Player->IsInPlay() ? Player : nullptr;
}

void AAlienMonster::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State == EAlienState::Dead)
	{
		// Shrink away, then disappear
		DeathTimer += DeltaTime;
		const float Alpha = FMath::Clamp(1.0f - DeathTimer / 0.6f, 0.0f, 1.0f);
		SetActorScale3D(FVector(FMath::Max(Alpha, 0.01f)));
		if (Alpha <= 0.0f)
		{
			Destroy();
		}
		return;
	}

	if (!Maze || Maze->GetNumTiles() == 0)
	{
		return;
	}

	ACosmoChompCharacter* Player = GetPlayer();
	CurrentTile = Maze->FindNearestTile(GetActorLocation());
	AttackCooldownRemaining -= DeltaTime;

	if (State == EAlienState::Attack)
	{
		TickAttack(DeltaTime, Player);
		UpdateFacing(DeltaTime, Player);
		return;
	}

	RepathTimer -= DeltaTime;
	if (RepathTimer <= 0.0f)
	{
		UpdateRoute(Player);
		RepathTimer = RepathInterval;
	}

	const float Speed = State == EAlienState::Chase ? ChaseSpeed : State == EAlienState::Flee ? FleeSpeed : WanderSpeed;

	if (State == EAlienState::Chase && Player && Maze->FindNearestTile(Player->GetActorLocation()) == CurrentTile)
	{
		// Same tile: go straight for the player
		SteerTowards(Player->GetActorLocation(), Speed);
	}
	else if (NextTile != INDEX_NONE)
	{
		// Follow the line between tile centers, which passes through the middle of each opening.
		// Heading straight for the next center could clip the posts beside the opening.
		const FVector From = Maze->GetTileLocation(CurrentTile);
		const FVector To = Maze->GetTileLocation(NextTile);
		const FVector Segment = To - From;
		const FVector Position = Maze->ProjectToSurface(GetActorLocation());

		float Along = 1.0f;
		if (!Segment.IsNearlyZero())
		{
			const float Progress = FVector::DotProduct(Position - From, Segment) / Segment.SizeSquared();
			const float Lookahead = 0.35f * Maze->GetTileSpacing() / Segment.Size();
			Along = FMath::Clamp(Progress + Lookahead, 0.0f, 1.0f);
		}
		SteerTowards(From + Segment * Along, Speed);
	}

	if (Player && State != EAlienState::Flee && AttackCooldownRemaining <= 0.0f
		&& FVector::Dist(Player->GetActorLocation(), GetActorLocation()) <= AttackRange)
	{
		StartAttack();
	}

	UpdateFacing(DeltaTime, Player);

	const bool bMoving = GetVelocity().SizeSquared() > FMath::Square(30.0f);
	PlayLoopingAnimation(bMoving ? WalkAnimation : IdleAnimation);
}

void AAlienMonster::UpdateRoute(ACosmoChompCharacter* Player)
{
	TArray<int32> PlayerDistances;
	if (Player)
	{
		PlayerDistances = Maze->ComputeDistances(Maze->FindNearestTile(Player->GetActorLocation()));
	}
	const int32 StepsToPlayer = PlayerDistances.IsValidIndex(CurrentTile) ? PlayerDistances[CurrentTile] : INDEX_NONE;
	const TArray<int32> Exits = Maze->GetOpenNeighbors(CurrentTile);

	if (Player && Player->HasRayGun() && StepsToPlayer != INDEX_NONE)
	{
		// Run for whichever exit leads furthest from the armed player
		State = EAlienState::Flee;
		NextTile = CurrentTile;
		int32 BestDistance = StepsToPlayer;
		for (int32 Exit : Exits)
		{
			if (PlayerDistances[Exit] > BestDistance || (PlayerDistances[Exit] == BestDistance && FMath::RandBool()))
			{
				BestDistance = PlayerDistances[Exit];
				NextTile = Exit;
			}
		}
		WanderGoal = INDEX_NONE;
		return;
	}

	if (StepsToPlayer != INDEX_NONE && StepsToPlayer <= ChaseTileRange)
	{
		// Step downhill on the distance field toward the player
		State = EAlienState::Chase;
		NextTile = CurrentTile;
		for (int32 Exit : Exits)
		{
			if (PlayerDistances[Exit] == StepsToPlayer - 1)
			{
				NextTile = Exit;
				break;
			}
		}
		WanderGoal = INDEX_NONE;
		return;
	}

	State = EAlienState::Wander;
	if (WanderGoal == INDEX_NONE || WanderGoal == CurrentTile)
	{
		WanderGoal = FMath::RandRange(0, Maze->GetNumTiles() - 1);
	}
	const TArray<int32> Path = Maze->FindPath(CurrentTile, WanderGoal);
	NextTile = Path.Num() > 1 ? Path[1] : CurrentTile;
	if (Path.IsEmpty())
	{
		WanderGoal = INDEX_NONE;
	}
}

void AAlienMonster::SteerTowards(const FVector& Target, float Speed)
{
	const FVector Up = PlanetaryGravity->GetCurrentSurfaceUp();
	const FVector Direction = FVector::VectorPlaneProject(Target - GetActorLocation(), Up);
	if (Direction.SizeSquared() < FMath::Square(15.0f))
	{
		return;
	}

	GetCharacterMovement()->MaxWalkSpeed = Speed;
	AddMovementInput(Direction.GetSafeNormal(), 1.0f);
}

void AAlienMonster::StartAttack()
{
	State = EAlienState::Attack;
	AttackPhase = 0.0f;
	bAttackLanded = false;
	PlayLoopingAnimation(AttackAnimation, false);

	if (AttackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttackSound, GetActorLocation());
	}
}

void AAlienMonster::TickAttack(float DeltaTime, ACosmoChompCharacter* Player)
{
	AttackPhase += DeltaTime;

	if (!bAttackLanded && AttackPhase >= AttackWindup)
	{
		bAttackLanded = true;

		// Some leeway, so stepping back at the last moment is needed to dodge
		if (Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) <= AttackRange * 1.3f)
		{
			UGameplayStatics::ApplyDamage(Player, AttackDamage, GetController(), this, UDamageType::StaticClass());
		}
	}

	if (AttackPhase >= AttackWindup + 0.5f)
	{
		State = EAlienState::Chase;
		AttackCooldownRemaining = AttackCooldown;
		RepathTimer = 0.0f;
	}
}

void AAlienMonster::UpdateFacing(float DeltaTime, ACosmoChompCharacter* Player)
{
	const FVector Up = PlanetaryGravity->GetCurrentSurfaceUp();

	FVector Forward = FVector::VectorPlaneProject(GetVelocity(), Up);
	if (State == EAlienState::Attack && Player)
	{
		Forward = FVector::VectorPlaneProject(Player->GetActorLocation() - GetActorLocation(), Up);
	}
	if (Forward.SizeSquared() < FMath::Square(10.0f))
	{
		Forward = FVector::VectorPlaneProject(GetActorForwardVector(), Up);
	}

	PlanetaryGravity->OrientOwnerToSurface(DeltaTime, Forward.GetSafeNormal(), 8.0f);
}

void AAlienMonster::PlayLoopingAnimation(UAnimationAsset* Animation, bool bLoop)
{
	if (Animation && Animation != CurrentAnimation)
	{
		CurrentAnimation = Animation;
		GetMesh()->PlayAnimation(Animation, bLoop);
	}
}

float AAlienMonster::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (State == EAlienState::Dead)
	{
		return 0.0f;
	}

	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Die();
	return DamageAmount;
}

void AAlienMonster::Die()
{
	State = EAlienState::Dead;
	DeathTimer = 0.0f;

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}

	OnKilled.Broadcast(this);
}
