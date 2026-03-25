// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"

#include "GrassInstanceData.generated.h"

/**
 * Per-instance grass data (design doc §2.1). PositionWS is blade root in component local space
 * (transformed by primitive LocalToWorld in the vertex factory).
 */
USTRUCT(BlueprintType)
struct FGrassInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	FVector3f PositionWS = FVector3f::ZeroVector;

	/** Matches HLSL float3 padding for StructuredBuffer layout; not used for logic. */
	float Padding0 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	FVector2f Direction = FVector2f(1.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	float Height = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	float Width = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	int32 ClumpIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	int32 RandomSeed = 0;
};

/** Per-clump data (design doc §2.2). Uploaded as StructuredBuffer for VS (wind etc. in later steps). */
USTRUCT(BlueprintType)
struct FGrassClumpData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	FVector3f CenterWS = FVector3f::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	float Radius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	float WindPhase = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	float WindStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	FVector2f WindDirection = FVector2f(1.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	float Density = 1.f;

	/** Shared by all instances whose **ClumpIndex** points to this clump; replicated to HISM PerInstanceCustomData (linear RGB). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
	FLinearColor ClumpColor = FLinearColor::White;
};
