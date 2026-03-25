// Copyright Epic Games, Inc. All Rights Reserved.

#include "GrassBladeMeshBuilder.h"

#include "DynamicMeshBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GrassBladeMeshBuilder)

namespace GrassBladeMeshBuilderPrivate
{
	static FDynamicMeshVertex MakeVertex(const FVector3f& Position, float Height01)
	{
		FDynamicMeshVertex V;
		V.Position = Position;
		V.TextureCoordinate[0] = FVector2f(Height01, 0.f);
		V.Color = FColor::White;
		V.TangentX = FVector3f(1.f, 0.f, 0.f);
		V.TangentZ = FVector3f(0.f, 1.f, 0.f);
		V.TangentZ.Vector.W = 127;
		return V;
	}

	static void AppendTriangle(TArray<uint32>& Indices, uint32 A, uint32 B, uint32 C)
	{
		Indices.Add(A);
		Indices.Add(B);
		Indices.Add(C);
	}
}

void FGrassBladeMeshBuilder::Build(
	float BladeHeight,
	float BladeBaseWidth,
	EGrassBladeMeshLOD LOD,
	TArray<FDynamicMeshVertex>& OutVertices,
	TArray<uint32>& OutIndices)
{
	using namespace GrassBladeMeshBuilderPrivate;

	OutVertices.Reset();
	OutIndices.Reset();

	if (BladeHeight <= KINDA_SMALL_NUMBER || BladeBaseWidth <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float H = BladeHeight;
	const float HalfW = 0.5f * BladeBaseWidth;

	switch (LOD)
	{
	case EGrassBladeMeshLOD::LOD0:
	{
		OutVertices.Add(MakeVertex(FVector3f(0.f, 0.f, H), 1.f));
		OutVertices.Add(MakeVertex(FVector3f(-HalfW, 0.f, 2.f * H / 3.f), 2.f / 3.f));
		OutVertices.Add(MakeVertex(FVector3f(HalfW, 0.f, 2.f * H / 3.f), 2.f / 3.f));
		OutVertices.Add(MakeVertex(FVector3f(-HalfW, 0.f, H / 3.f), 1.f / 3.f));
		OutVertices.Add(MakeVertex(FVector3f(HalfW, 0.f, H / 3.f), 1.f / 3.f));
		OutVertices.Add(MakeVertex(FVector3f(-HalfW, 0.f, 0.f), 0.f));
		OutVertices.Add(MakeVertex(FVector3f(HalfW, 0.f, 0.f), 0.f));

		AppendTriangle(OutIndices, 0, 1, 2);
		AppendTriangle(OutIndices, 1, 3, 2);
		AppendTriangle(OutIndices, 2, 3, 4);
		AppendTriangle(OutIndices, 3, 5, 4);
		AppendTriangle(OutIndices, 4, 5, 6);
		break;
	}
	case EGrassBladeMeshLOD::LOD1:
	{
		OutVertices.Add(MakeVertex(FVector3f(0.f, 0.f, H), 1.f));
		OutVertices.Add(MakeVertex(FVector3f(-HalfW, 0.f, H * 0.5f), 0.5f));
		OutVertices.Add(MakeVertex(FVector3f(HalfW, 0.f, H * 0.5f), 0.5f));
		OutVertices.Add(MakeVertex(FVector3f(-HalfW, 0.f, 0.f), 0.f));
		OutVertices.Add(MakeVertex(FVector3f(HalfW, 0.f, 0.f), 0.f));

		AppendTriangle(OutIndices, 0, 1, 2);
		AppendTriangle(OutIndices, 1, 3, 2);
		AppendTriangle(OutIndices, 2, 3, 4);
		break;
	}
	case EGrassBladeMeshLOD::LOD2:
	default:
	{
		OutVertices.Add(MakeVertex(FVector3f(0.f, 0.f, H), 1.f));
		OutVertices.Add(MakeVertex(FVector3f(-HalfW, 0.f, 0.f), 0.f));
		OutVertices.Add(MakeVertex(FVector3f(HalfW, 0.f, 0.f), 0.f));

		AppendTriangle(OutIndices, 0, 1, 2);
		break;
	}
	}
}
