#include "EnergyOrb.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "CosmoChompCharacter.h"

AEnergyOrb::AEnergyOrb()
{
	PrimaryActorTick.bCanEverTick = false;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->InitSphereRadius(70.0f);
	PickupSphere->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = PickupSphere;

	OrbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh"));
	OrbMesh->SetupAttachment(PickupSphere);
	OrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrbMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Materials/M_Glow"));
	OrbMesh->SetStaticMesh(SphereMesh.Object);
	GlowMaterial = Glow.Object;
}

void AEnergyOrb::BeginPlay()
{
	Super::BeginPlay();

	// Engine sphere is 100 units across
	OrbMesh->SetRelativeScale3D(FVector(OrbSize / 100.0f));

	if (GlowMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(GlowMaterial, this);
		Material->SetVectorParameterValue(TEXT("Color"), GlowColor);
		OrbMesh->SetMaterial(0, Material);
	}

	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnergyOrb::OnPickupOverlap);

	if (InitialPickupDelay > 0.0f)
	{
		SetPickupDelay(InitialPickupDelay);
	}
}

void AEnergyOrb::SetPickupDelay(float Seconds)
{
	PickupSphere->SetGenerateOverlapEvents(false);
	GetWorldTimerManager().SetTimer(PickupDelayTimer, this, &AEnergyOrb::EnablePickup, Seconds, false);
}

void AEnergyOrb::EnablePickup()
{
	// Refreshing overlaps collects the orb right away if the player is already standing on it
	PickupSphere->SetGenerateOverlapEvents(true);
	PickupSphere->UpdateOverlaps();
}

void AEnergyOrb::OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryCollect(OtherActor);
}

void AEnergyOrb::TryCollect(AActor* OtherActor)
{
	ACosmoChompCharacter* Player = Cast<ACosmoChompCharacter>(OtherActor);
	if (!IsValid(this) || !Player || !Player->IsInPlay())
	{
		return;
	}

	Player->AddOrbs(Value);
	if (CollectSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CollectSound, GetActorLocation());
	}
	Destroy();
}
