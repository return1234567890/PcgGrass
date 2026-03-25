// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProceduralGrassComponent.h"

#include "Math/RandomStream.h"
#include "DynamicMeshBuilder.h"
#include "Engine/CollisionProfile.h"
#include "GrassBladeMeshBuilder.h"
#include "GrassInstanceData.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshDescription.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProceduralGrassComponent)

#if WITH_EDITOR
namespace PcgGrassMaterialEditorPrivate
{
	static bool MaterialIsInGrassAssignmentChain(UMaterialInterface* Grass, UMaterialInterface* Compiled)
	{
		if (!Grass || !Compiled)
		{
			return false;
		}
		for (UMaterialInterface* M = Grass; M != nullptr; )
		{
			if (M == Compiled)
			{
				return true;
			}
			const UMaterialInstance* MI = Cast<UMaterialInstance>(M);
			if (!MI)
			{
				break;
			}
			M = MI->Parent;
		}
		return false;
	}
} // namespace PcgGrassMaterialEditorPrivate
#endif

namespace PcgGrassHISM
{
	static constexpr int32 NumCustomDataFloats = 11;
	// 0: Height scale, 1: Width scale, 2: Random seed (float bits), 3: Clump index,
	// 4-6: Clump linear RGB (from GrassClumps[ClumpIndex].ClumpColor)
	// 7-8: Clump wind direction XY, 9: Clump wind strength, 10: Clump wind phase01 [0..1]

	static float EncodePhase01(float PhaseRad)
	{
		const float TwoPi = 2.f * UE_PI;
		const float Wrapped = FMath::Fmod(PhaseRad, TwoPi);
		const float Positive = (Wrapped < 0.f) ? (Wrapped + TwoPi) : Wrapped;
		return Positive / TwoPi;
	}

	static FTransform InstanceToLocalTransform(const FGrassInstanceData& I, float BladeH, float BladeW)
	{
		const FVector Loc(I.PositionWS);
		const float YawDeg = FMath::RadiansToDegrees(FMath::Atan2(I.Direction.Y, I.Direction.X));
		const FRotator Rot(0.f, YawDeg, 0.f);
		// Scale matches prior bounds: X/Y blade width, Z height (template mesh expected at BladeBaseWidth x BladeHeight).
		const FVector Scale3D(
			FMath::Max(KINDA_SMALL_NUMBER, BladeW * I.Width),
			FMath::Max(KINDA_SMALL_NUMBER, BladeW * I.Width * 0.4f),
			FMath::Max(KINDA_SMALL_NUMBER, BladeH * I.Height));
		return FTransform(Rot, Loc, Scale3D);
	}
} // namespace PcgGrassHISM

void UProceduralGrassComponent::NormalizeDistributionRect(FVector2f& OutMin, FVector2f& OutMax)
{
	if (OutMin.X > OutMax.X)
	{
		Swap(OutMin.X, OutMax.X);
	}
	if (OutMin.Y > OutMax.Y)
	{
		Swap(OutMin.Y, OutMax.Y);
	}
}

void UProceduralGrassComponent::GenerateGrassDistribution()
{
	if (NumClumps < 1 || NumInstances < 1)
	{
		return;
	}

	FVector2f MinPt = DistributionMin;
	FVector2f MaxPt = DistributionMax;
	NormalizeDistributionRect(MinPt, MaxPt);

	FRandomStream Stream(DistributionRandomSeed);

	GrassClumps.SetNum(NumClumps);
	const float ExtentX = MaxPt.X - MinPt.X;
	const float ExtentY = MaxPt.Y - MinPt.Y;
	const float TypicalRadius = 0.5f * FMath::Min(ExtentX, ExtentY) / FMath::Max(1.f, FMath::Sqrt(static_cast<float>(NumClumps)));
	const float GlobalWindAngle = Stream.FRandRange(-UE_PI, UE_PI);
	const FVector2f GlobalWindDirection(FMath::Cos(GlobalWindAngle), FMath::Sin(GlobalWindAngle));

	for (int32 i = 0; i < NumClumps; ++i)
	{
		FGrassClumpData& C = GrassClumps[i];
		const float X = Stream.FRandRange(MinPt.X, MaxPt.X);
		const float Y = Stream.FRandRange(MinPt.Y, MaxPt.Y);
		C.CenterWS = FVector3f(X, Y, 0.f);
		C.Radius = TypicalRadius;
		C.WindDirection = GlobalWindDirection;
		C.WindStrength = Stream.FRandRange(0.6f, 1.4f);
		C.WindPhase = Stream.FRandRange(0.f, 2.f * UE_PI);
		C.Density = 1.f;
		const uint8 Hue = static_cast<uint8>(Stream.RandRange(0, 255));
		const uint8 Sat = static_cast<uint8>(Stream.RandRange(90, 255));
		const uint8 Val = static_cast<uint8>(Stream.RandRange(90, 255));
		C.ClumpColor = FLinearColor::MakeFromHSV8(Hue, Sat, Val);
	}

	GrassInstances.SetNum(NumInstances);
	for (int32 i = 0; i < NumInstances; ++i)
	{
		FGrassInstanceData& Inst = GrassInstances[i];
		const float X = Stream.FRandRange(MinPt.X, MaxPt.X);
		const float Y = Stream.FRandRange(MinPt.Y, MaxPt.Y);
		Inst.PositionWS = FVector3f(X, Y, 0.f);
		const float Angle = Stream.FRandRange(-UE_PI, UE_PI);
		Inst.Direction = FVector2f(FMath::Cos(Angle), FMath::Sin(Angle));
		Inst.Height = 1.f;
		Inst.Width = 1.f;
		Inst.Padding0 = 0.f;
		Inst.RandomSeed = static_cast<int32>(Stream.GetUnsignedInt());
	}

	for (int32 i = 0; i < NumInstances; ++i)
	{
		const FVector2f P(GrassInstances[i].PositionWS.X, GrassInstances[i].PositionWS.Y);
		int32 BestK = 0;
		float BestD = FLT_MAX;
		for (int32 k = 0; k < NumClumps; ++k)
		{
			const FVector2f C(GrassClumps[k].CenterWS.X, GrassClumps[k].CenterWS.Y);
			const float DX = P.X - C.X;
			const float DY = P.Y - C.Y;
			const float D = DX * DX + DY * DY;
			if (D < BestD)
			{
				BestD = D;
				BestK = k;
			}
		}
		GrassInstances[i].ClumpIndex = BestK;
	}

	if (IsRegistered())
	{
		SyncGrassInstancesToHISM();
	}
}

void UProceduralGrassComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyBladeMeshAndMaterial();
	if (bAutoGenerateOnBeginPlay && GrassInstances.Num() == 0 && NumClumps > 0 && NumInstances > 0)
	{
		GenerateGrassDistribution();
	}
	else
	{
		SyncGrassInstancesToHISM();
	}
}

UProceduralGrassComponent::UProceduralGrassComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	GrassHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("GrassHISM"));
	GrassHISM->SetupAttachment(this);
	ConfigureGrassHISM();
}

void UProceduralGrassComponent::ConfigureGrassHISM()
{
	if (!GrassHISM)
	{
		return;
	}
	GrassHISM->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GrassHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrassHISM->CastShadow = true;
	GrassHISM->bCastDynamicShadow = true;
	GrassHISM->SetCanEverAffectNavigation(false);
}

void UProceduralGrassComponent::BuildRuntimeBladeMeshIfNeeded()
{
	if (BladeMesh)
	{
		return;
	}
	if (BladeHeight <= KINDA_SMALL_NUMBER || BladeBaseWidth <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TArray<FDynamicMeshVertex> DynVerts;
	TArray<uint32> Indices;
	FGrassBladeMeshBuilder::Build(BladeHeight, BladeBaseWidth, RenderGrassLOD, DynVerts, Indices);
	if (DynVerts.Num() == 0 || Indices.Num() < 3)
	{
		return;
	}

	if (!RuntimeBladeMesh)
	{
		RuntimeBladeMesh = NewObject<UStaticMesh>(this, TEXT("RuntimeBladeMesh"), RF_Transient | RF_DuplicateTransient);
	}

	TArray<UStaticMeshDescription*> Descs;
	auto BuildMeshDescForLOD = [this, &Descs](EGrassBladeMeshLOD LOD)
	{
		TArray<FDynamicMeshVertex> LODVerts;
		TArray<uint32> LODIndices;
		FGrassBladeMeshBuilder::Build(BladeHeight, BladeBaseWidth, LOD, LODVerts, LODIndices);
		if (LODVerts.Num() == 0 || LODIndices.Num() < 3)
		{
			return;
		}

		UStaticMeshDescription* MeshDesc = UStaticMesh::CreateStaticMeshDescription(RuntimeBladeMesh);
		FMeshDescriptionBuilder Builder;
		Builder.SetMeshDescription(&MeshDesc->GetMeshDescription());
		Builder.SetNumUVLayers(1);

		const FPolygonGroupID PolyGroup = Builder.AppendPolygonGroup(TEXT("GrassBlade"));

		TArray<FVertexID> VertexIDs;
		VertexIDs.Reserve(LODVerts.Num());
		for (const FDynamicMeshVertex& V : LODVerts)
		{
			VertexIDs.Add(Builder.AppendVertex(FVector(V.Position)));
		}

		FMeshDescription& MeshDescription = MeshDesc->GetMeshDescription();
		for (int32 TriBase = 0; TriBase + 2 < LODIndices.Num(); TriBase += 3)
		{
			const int32 I0 = LODIndices[TriBase];
			const int32 I1 = LODIndices[TriBase + 1];
			const int32 I2 = LODIndices[TriBase + 2];
			if (!VertexIDs.IsValidIndex(I0) || !VertexIDs.IsValidIndex(I1) || !VertexIDs.IsValidIndex(I2))
			{
				continue;
			}

			const FVertexID TriVerts[3] = { VertexIDs[I0], VertexIDs[I1], VertexIDs[I2] };
			const FTriangleID TriID = Builder.AppendTriangle(TriVerts, PolyGroup);

			const FVector3f P0 = LODVerts[I0].Position;
			const FVector3f P1 = LODVerts[I1].Position;
			const FVector3f P2 = LODVerts[I2].Position;
			// Winding from FGrassBladeMeshBuilder faces -Y; flip so the lit side matches +Y (blade in XZ plane).
			FVector3f Nf = FVector3f::CrossProduct(P2 - P0, P1 - P0).GetSafeNormal();
			if (Nf.IsNearlyZero())
			{
				Nf = FVector3f(0.f, 1.f, 0.f);
			}
			const FVector N(Nf);

			TArrayView<const FVertexInstanceID> CornerIDs = MeshDescription.GetTriangleVertexInstances(TriID);
			if (CornerIDs.Num() != 3)
			{
				continue;
			}

			const int32 Idx[3] = { I0, I1, I2 };
			for (int32 j = 0; j < 3; ++j)
			{
				const FDynamicMeshVertex& V = LODVerts[Idx[j]];
				Builder.SetInstanceUV(CornerIDs[j], FVector2D(V.TextureCoordinate[0]), 0);
				FVector TangentX = V.TangentX.ToFVector();
				TangentX = (TangentX - N * FVector::DotProduct(TangentX, N)).GetSafeNormal();
				if (TangentX.IsNearlyZero())
				{
					TangentX = FVector::CrossProduct(N, FVector(0.f, 0.f, 1.f)).GetSafeNormal();
					if (TangentX.IsNearlyZero())
					{
						TangentX = FVector::CrossProduct(N, FVector(1.f, 0.f, 0.f)).GetSafeNormal();
					}
				}
				Builder.SetInstanceTangentSpace(CornerIDs[j], N, TangentX, 1.f);
			}
		}

		Descs.Add(MeshDesc);
	};

	BuildMeshDescForLOD(EGrassBladeMeshLOD::LOD0);
	BuildMeshDescForLOD(EGrassBladeMeshLOD::LOD1);
	BuildMeshDescForLOD(EGrassBladeMeshLOD::LOD2);
	if (Descs.Num() == 0)
	{
		return;
	}

	RuntimeBladeMesh->BuildFromStaticMeshDescriptions(Descs, false, true);
}

void UProceduralGrassComponent::SyncRuntimeBladeMeshStaticMaterial(UMaterialInterface* Material)
{
	if (!RuntimeBladeMesh || !Material)
	{
		return;
	}
	TArray<FStaticMaterial>& Mats = RuntimeBladeMesh->GetStaticMaterials();
#if WITH_EDITOR
	RuntimeBladeMesh->Modify();
#endif
	if (Mats.Num() == 0)
	{
		// Fast build can leave no static material entries; add slot 0 so mesh + verification stay consistent.
		FStaticMaterial Slot;
		Slot.MaterialInterface = Material;
		Slot.MaterialSlotName = TEXT("GrassBlade");
		Slot.ImportedMaterialSlotName = TEXT("GrassBlade");
		Mats.Add(Slot);
	}
	else
	{
		Mats[0].MaterialInterface = Material;
		if (Mats[0].MaterialSlotName.IsNone())
		{
			Mats[0].MaterialSlotName = TEXT("GrassBlade");
		}
	}
}

void UProceduralGrassComponent::ApplyBladeMeshAndMaterial()
{
	if (!GrassHISM)
	{
		return;
	}

	UMaterialInterface* Mat = GrassMaterial.Get();
	if (!Mat)
	{
		Mat = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	if (BladeMesh)
	{
		GrassHISM->SetStaticMesh(BladeMesh);
		GrassHISM->SetMaterial(0, Mat);
	}
	else
	{
		BuildRuntimeBladeMeshIfNeeded();
		if (RuntimeBladeMesh)
		{
			SyncRuntimeBladeMeshStaticMaterial(Mat);
			GrassHISM->SetStaticMesh(RuntimeBladeMesh);
			GrassHISM->SetMaterial(0, Mat);
		}
		else
		{
			GrassHISM->SetStaticMesh(nullptr);
		}
	}

	GrassHISM->MarkRenderStateDirty();
}

void UProceduralGrassComponent::PostLoad()
{
	Super::PostLoad();
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ApplyBladeMeshAndMaterial();
		SyncGrassInstancesToHISM();
	}
}

void UProceduralGrassComponent::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	UMaterial::OnMaterialCompilationFinished().RemoveAll(this);
	UMaterial::OnMaterialCompilationFinished().AddUObject(this, &UProceduralGrassComponent::OnEditorGrassMaterialCompiled);
#endif
	ApplyBladeMeshAndMaterial();
#if WITH_EDITOR
	if (bEditorGenerateOnRegisterIfEmpty && GrassInstances.Num() == 0 && NumClumps > 0 && NumInstances > 0 && !HasAnyFlags(RF_ClassDefaultObject))
	{
		GenerateGrassDistribution();
	}
	else
#endif
	{
		SyncGrassInstancesToHISM();
	}
}

void UProceduralGrassComponent::OnUnregister()
{
#if WITH_EDITOR
	UMaterial::OnMaterialCompilationFinished().RemoveAll(this);
#endif
	Super::OnUnregister();
}

#if WITH_EDITOR
void UProceduralGrassComponent::OnEditorGrassMaterialCompiled(UMaterialInterface* CompiledMaterial)
{
	if (!GrassHISM || !IsRegistered() || HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	UMaterialInterface* Grass = GrassMaterial.Get();
	if (!PcgGrassMaterialEditorPrivate::MaterialIsInGrassAssignmentChain(Grass, CompiledMaterial))
	{
		return;
	}
	ApplyBladeMeshAndMaterial();
	SyncGrassInstancesToHISM();
	GrassHISM->InvalidateLumenSurfaceCache();
	GrassHISM->MarkRenderStateDirty();
}
#endif

void UProceduralGrassComponent::SyncGrassInstancesToHISM()
{
	if (!GrassHISM || !IsRegistered())
	{
		return;
	}

	if (BladeHeight <= KINDA_SMALL_NUMBER || BladeBaseWidth <= KINDA_SMALL_NUMBER)
	{
		GrassHISM->ClearInstances();
		return;
	}

	TArray<FGrassInstanceData> Source = GrassInstances;
	if (Source.Num() == 0)
	{
		Source.AddDefaulted(1);
	}

	GrassHISM->ClearInstances();
	GrassHISM->SetNumCustomDataFloats(PcgGrassHISM::NumCustomDataFloats);

	TArray<FTransform> Transforms;
	Transforms.Reserve(Source.Num());
	for (const FGrassInstanceData& I : Source)
	{
		Transforms.Add(PcgGrassHISM::InstanceToLocalTransform(I, BladeHeight, BladeBaseWidth));
	}

	GrassHISM->AddInstances(Transforms, false, false, false);

	const int32 Count = GrassHISM->GetInstanceCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FGrassInstanceData& I = Source[Index];
		const float SeedBits = FMath::AsFloat(static_cast<uint32>(I.RandomSeed));
		FLinearColor ClumpRGB = FLinearColor::White;
		FVector2f WindDir(1.f, 0.f);
		float WindStrength = 1.f;
		float WindPhase01 = 0.f;
		if (GrassClumps.IsValidIndex(I.ClumpIndex))
		{
			const FGrassClumpData& Clump = GrassClumps[I.ClumpIndex];
			ClumpRGB = Clump.ClumpColor;
			WindDir = Clump.WindDirection.GetSafeNormal();
			WindStrength = FMath::Max(0.f, Clump.WindStrength);
			WindPhase01 = PcgGrassHISM::EncodePhase01(Clump.WindPhase);
		}
		const float Custom[PcgGrassHISM::NumCustomDataFloats] = {
			I.Height,
			I.Width,
			SeedBits,
			static_cast<float>(I.ClumpIndex),
			ClumpRGB.R,
			ClumpRGB.G,
			ClumpRGB.B,
			WindDir.X,
			WindDir.Y,
			WindStrength,
			WindPhase01,
		};
		const bool bLast = (Index == Count - 1);
		GrassHISM->SetCustomData(Index, MakeArrayView(Custom, UE_ARRAY_COUNT(Custom)), bLast);
	}

	GrassHISM->MarkRenderStateDirty();
}

#if WITH_EDITOR
void UProceduralGrassComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyBladeMeshAndMaterial();
	SyncGrassInstancesToHISM();
}
#endif
