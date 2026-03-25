// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"

/** Blade mesh LOD matching Docs/ProceduralGrass_NewDesign.md §4.2.1 (local XZ plane, Z up, Y = 0). */
UENUM(BlueprintType)
enum class EGrassBladeMeshLOD : uint8
{
	LOD0 UMETA(DisplayName = "LOD0 (7 verts, 5 tris)"),
	LOD1 UMETA(DisplayName = "LOD1 (5 verts, 3 tris)"),
	LOD2 UMETA(DisplayName = "LOD2 (3 verts, 1 tri)"),
};

struct FGrassBladeMeshBuilder
{
	/** Fills vertices and triangle list indices (uint32) for the selected LOD. UV0: X = height01, Y = 0. */
	static void Build(
		float BladeHeight,
		float BladeBaseWidth,
		EGrassBladeMeshLOD LOD,
		TArray<FDynamicMeshVertex>& OutVertices,
		TArray<uint32>& OutIndices);
};

#include "GrassBladeMeshBuilder.generated.h"
