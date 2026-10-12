#include "CosmoRocket.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "CosmoChompCharacter.h"
#include "CosmoChompMeshFitting.h"

ACosmoRocket::ACosmoRocket()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	RocketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RocketMesh"));
	RocketMesh->SetupAttachment(Root);
	RocketMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	FlameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlameMesh"));
	FlameMesh->SetupAttachment(Root);
	FlameMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlameMesh->SetCastShadow(false);
	FlameMesh->SetHiddenInGame(true);
	// Engine cone is 100 units tall with its pivot at the center; point it down
	FlameMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -40.0f), FRotator(180.0f, 0.0f, 0.0f));
	FlameMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 2.0f));

	DepositZone = CreateDefaultSubobject<USphereComponent>(TEXT("DepositZone"));
	DepositZone->SetupAttachment(Root);
	DepositZone->InitSphereRadius(420.0f);
	DepositZone->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	DepositZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DepositZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	DepositZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rocket(TEXT("/Game/Meshes/Rocket/SM_Rocket"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Materials/M_Glow"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Deposit(TEXT("/Game/Audio/S_Whoosh"));
	static ConstructorHelpers::FObjectFinder<USoundBase> LaunchSfx(TEXT("/Game/Audio/S_RocketLaunch"));

	RocketMesh->SetStaticMesh(Rocket.Object);
	FlameMesh->SetStaticMesh(Cone.Object);
	FlameMaterial = Glow.Object;
	DepositSound = Deposit.Object;
	LaunchSound = LaunchSfx.Object;
}

void ACosmoRocket::BeginPlay()
{
	Super::BeginPlay();

	CosmoChompMeshFitting::FitStaticMesh(RocketMesh, RocketHeight, true);

	if (FlameMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(FlameMaterial, this);
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.45f, 0.05f));
		Material->SetScalarParameterValue(TEXT("Intensity"), 20.0f);
		FlameMesh->SetMaterial(0, Material);
	}
}

void ACosmoRocket::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bLaunched)
	{
		// Deposit continuously, so a player waiting by the rocket hands over orbs as soon as they have any
		TArray<AActor*> Overlapping;
		DepositZone->GetOverlappingActors(Overlapping, ACosmoChompCharacter::StaticClass());
		for (AActor* Actor : Overlapping)
		{
			DepositFrom(Cast<ACosmoChompCharacter>(Actor));
		}
		return;
	}

	// Blast off along the local up axis (away from the planet)
	LaunchTime += DeltaTime;
	LaunchSpeed += LaunchAcceleration * DeltaTime;
	AddActorWorldOffset(GetActorUpVector() * LaunchSpeed * DeltaTime);
	FlameMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 2.0f + 0.4f * FMath::Sin(LaunchTime * 40.0f)));

	if (LaunchTime >= LaunchDuration)
	{
		SetActorTickEnabled(false);
		SetActorHiddenInGame(true);
	}
}

void ACosmoRocket::DepositFrom(ACosmoChompCharacter* Player)
{
	if (bLaunched || !Player || !Player->IsInPlay() || Player->GetCarriedOrbs() <= 0)
	{
		return;
	}

	StoredOrbs += Player->TakeAllOrbs();
	OnFuelChanged.Broadcast(StoredOrbs, RequiredOrbs);

	if (DepositSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DepositSound, GetActorLocation());
	}

	if (StoredOrbs >= RequiredOrbs)
	{
		Launch(Player);
	}
}

void ACosmoRocket::Launch(ACosmoChompCharacter* Player)
{
	bLaunched = true;
	Player->BoardRocket();

	RocketMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlameMesh->SetHiddenInGame(false);
	if (LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}

	OnLaunched.Broadcast();
}
