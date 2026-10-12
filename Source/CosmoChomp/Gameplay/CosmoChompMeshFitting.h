#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

/**
 * The imported Sketchfab models all come in at different scales, so gameplay code
 * sizes them by a target dimension instead of relying on import scale.
 */
namespace CosmoChompMeshFitting
{
	/** Scales a skeletal mesh to Height and stands it on the bottom of the capsule, rotated by Yaw */
	inline void FitSkeletalMeshToCapsule(USkeletalMeshComponent* MeshComponent, const UCapsuleComponent* Capsule, float Height, float Yaw)
	{
		const USkeletalMesh* Mesh = MeshComponent ? MeshComponent->GetSkeletalMeshAsset() : nullptr;
		if (!Mesh || !Capsule)
		{
			return;
		}

		const FBoxSphereBounds Bounds = Mesh->GetImportedBounds();
		const float MeshHeight = FMath::Max(Bounds.BoxExtent.Z * 2.0f, KINDA_SMALL_NUMBER);
		const float Scale = Height / MeshHeight;
		const float MeshBottom = Bounds.Origin.Z - Bounds.BoxExtent.Z;

		MeshComponent->SetRelativeScale3D(FVector(Scale));
		MeshComponent->SetRelativeLocationAndRotation(
			FVector(0.0f, 0.0f, -Capsule->GetUnscaledCapsuleHalfHeight() - MeshBottom * Scale),
			FRotator(0.0f, Yaw, 0.0f));
	}

	/**
	 * Scales a static mesh so its largest dimension equals Size. If bStandOnBase, its
	 * bottom-center sits at the component's parent origin; otherwise its center does.
	 */
	inline void FitStaticMesh(UStaticMeshComponent* MeshComponent, float Size, bool bStandOnBase)
	{
		const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh().Get() : nullptr;
		if (!Mesh)
		{
			return;
		}

		const FBox Bounds = Mesh->GetBoundingBox();
		const float Scale = Size / FMath::Max(Bounds.GetSize().GetMax(), KINDA_SMALL_NUMBER);
		FVector Pivot = Bounds.GetCenter();
		if (bStandOnBase)
		{
			Pivot.Z = Bounds.Min.Z;
		}

		MeshComponent->SetRelativeScale3D(FVector(Scale));
		MeshComponent->SetRelativeLocation(MeshComponent->GetRelativeRotation().RotateVector(-Pivot * Scale));
	}
}
