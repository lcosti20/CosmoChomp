#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AlienMonster.generated.h"

class AHexMaze;
class ACosmoChompCharacter;
class UPlanetaryGravityComponent;
class UAnimationAsset;
class USkeletalMesh;
class USoundBase;

UENUM(BlueprintType)
enum class EAlienState : uint8
{
	Wander,
	Chase,
	Flee,
	Attack,
	Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAlienKilled, AAlienMonster*, Alien);

/**
 * One-eyed alien that roams the maze. Navigation runs on the maze's tile graph
 * (navmesh doesn't work on a sphere): it wanders between random tiles, chases the
 * player when they're a few tiles away, and runs from a player holding the ray gun.
 */
UCLASS()
class COSMOCHOMP_API AAlienMonster : public ACharacter
{
	GENERATED_BODY()

public:
	AAlienMonster();

	virtual void Tick(float DeltaTime) override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Alien")
	EAlienState GetState() const { return State; }

	UPROPERTY(BlueprintAssignable, Category = "Alien")
	FOnAlienKilled OnKilled;

	// Behavior
	// ------------------------------------------------

	/** Starts chasing when the player is this many steps away through the maze */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Behavior")
	int32 ChaseTileRange = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Behavior")
	float WanderSpeed = 260.0f;

	/** Slower than the player's 500 so a careful player can outrun them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Behavior")
	float ChaseSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Behavior")
	float FleeSpeed = 300.0f;

	/** Seconds between route updates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Behavior")
	float RepathInterval = 0.35f;

	// Attack
	// ------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Attack")
	float AttackRange = 170.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Attack")
	float AttackDamage = 1.0f;

	/** Delay between starting the swing and the hit landing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Attack")
	float AttackWindup = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Attack")
	float AttackCooldown = 1.6f;

	// Presentation
	// ------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Visuals")
	TObjectPtr<USkeletalMesh> AlienMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Visuals")
	TObjectPtr<UAnimationAsset> IdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Visuals")
	TObjectPtr<UAnimationAsset> WalkAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Visuals")
	TObjectPtr<UAnimationAsset> AttackAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Visuals")
	float MeshHeight = 190.0f;

	/** The model faces +Y; rotate it to face the actor's +X */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Visuals")
	float MeshYaw = -90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Visuals")
	TObjectPtr<USoundBase> AttackSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien|Visuals")
	TObjectPtr<USoundBase> DeathSound;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planetary Gravity")
	TObjectPtr<UPlanetaryGravityComponent> PlanetaryGravity;

	UPROPERTY()
	TObjectPtr<AHexMaze> Maze;

	EAlienState State = EAlienState::Wander;

	/** Tile the alien is standing on, and the adjacent tile it is walking to */
	int32 CurrentTile = INDEX_NONE;
	int32 NextTile = INDEX_NONE;

	/** Destination while wandering */
	int32 WanderGoal = INDEX_NONE;

	float RepathTimer = 0.0f;
	float AttackCooldownRemaining = 0.0f;
	float AttackPhase = 0.0f;
	float DeathTimer = 0.0f;
	bool bAttackLanded = false;

	UPROPERTY()
	TObjectPtr<UAnimationAsset> CurrentAnimation;

	ACosmoChompCharacter* GetPlayer() const;
	void UpdateRoute(ACosmoChompCharacter* Player);
	void SteerTowards(const FVector& Target, float Speed);
	void StartAttack();
	void TickAttack(float DeltaTime, ACosmoChompCharacter* Player);
	void UpdateFacing(float DeltaTime, ACosmoChompCharacter* Player);
	void PlayLoopingAnimation(UAnimationAsset* Animation, bool bLoop = true);
	void Die();
};
