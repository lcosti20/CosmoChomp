#include "RayGunPickup.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "CosmoChompCharacter.h"
#include "CosmoChompMeshFitting.h"

ARayGunPickup::ARayGunPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->InitSphereRadius(110.0f);
	PickupSphere->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = PickupSphere;

	// The spinner turns this pivot, leaving the gun's fitting offset intact
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(PickupSphere);

	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(Pivot);
	GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Spinner = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spinner"));
	Spinner->RotationRate = FRotator(0.0f, 120.0f, 0.0f);
	Spinner->SetUpdatedComponent(Pivot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Gun(TEXT("/Game/Meshes/RayGun/SM_RayGun"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Pickup(TEXT("/Game/Audio/S_Whoosh"));
	GunMesh->SetStaticMesh(Gun.Object);
	PickupSound = Pickup.Object;
}

void ARayGunPickup::BeginPlay()
{
	Super::BeginPlay();

	CosmoChompMeshFitting::FitStaticMesh(GunMesh, DisplaySize, false);
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ARayGunPickup::OnPickupOverlap);
}

void ARayGunPickup::OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACosmoChompCharacter* Player = Cast<ACosmoChompCharacter>(OtherActor);
	if (!IsValid(this) || !Player || !Player->IsInPlay())
	{
		return;
	}

	Player->EquipRayGun(Duration);
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	OnCollected.Broadcast();
	Destroy();
}

// Beam
// ------------------------------------------------

ARayGunBeam::ARayGunBeam()
{
	PrimaryActorTick.bCanEverTick = true;

	Segments = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Segments"));
	Segments->SetMobility(EComponentMobility::Movable);
	Segments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Segments->SetCastShadow(false);
	RootComponent = Segments;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Materials/M_Glow"));
	Segments->SetStaticMesh(Cylinder.Object);
	BeamMaterial = Glow.Object;
}

void ARayGunBeam::SetPoints(const TArray<FVector>& Points, const FLinearColor& Color)
{
	if (BeamMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BeamMaterial, this);
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetScalarParameterValue(TEXT("Intensity"), 25.0f);
		Segments->SetMaterial(0, Material);
	}

	// Engine cylinder: 100 units tall along Z, 100 across, pivot at its center
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		const FVector Start = Points[Index - 1];
		const FVector End = Points[Index];
		const float Length = FVector::Dist(Start, End);
		if (Length < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FQuat Rotation = FRotationMatrix::MakeFromZ(End - Start).ToQuat();
		const FVector Scale(BeamWidth / 100.0f, BeamWidth / 100.0f, Length / 100.0f);
		Segments->AddInstance(FTransform(Rotation, (Start + End) * 0.5f, Scale), true);
	}
}

void ARayGunBeam::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Thin out, then disappear
	Age += DeltaTime;
	const float Alpha = 1.0f - Age / Lifetime;
	if (Alpha <= 0.0f)
	{
		Destroy();
		return;
	}
	for (int32 Index = 0; Index < Segments->GetInstanceCount(); ++Index)
	{
		FTransform Transform;
		Segments->GetInstanceTransform(Index, Transform, true);
		const FVector Scale = Transform.GetScale3D();
		Transform.SetScale3D(FVector(BeamWidth / 100.0f * Alpha, BeamWidth / 100.0f * Alpha, Scale.Z));
		Segments->UpdateInstanceTransform(Index, Transform, true, Index == Segments->GetInstanceCount() - 1);
	}
}
