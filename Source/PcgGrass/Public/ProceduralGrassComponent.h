// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "GrassBladeMeshBuilder.h"
#include "GrassInstanceData.h"

#include "Materials/MaterialInterface.h"

#include "ProceduralGrassComponent.generated.h"

/**
 * Procedural grass: holds instance/clump data and syncs rendering to a child HISM (GPUScene / instancing path).
 * If Blade Mesh is unset, a transient UStaticMesh is built at runtime from FGrassBladeMeshBuilder (unit template / RenderGrassLOD).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UProceduralGrassComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UProceduralGrassComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Blade")
	EGrassBladeMeshLOD BladeLOD = EGrassBladeMeshLOD::LOD0;

	/** Reserved for future per-draw LOD selection (StaticMesh LOD or alternate assets). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Blade")
	EGrassBladeMeshLOD RenderGrassLOD = EGrassBladeMeshLOD::LOD0;

	/** Applied to GrassHISM material slot 0. For thin blades, enable Two Sided on the material (not set from code). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Material")
	TObjectPtr<UMaterialInterface> GrassMaterial;

	/** If set, used on GrassHISM; if null, a runtime unit-size blade mesh is built from RenderGrassLOD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Render")
	TObjectPtr<UStaticMesh> BladeMesh;

	/** Runtime-generated blade (when BladeMesh is null). Not serialized. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "GrassPCG|Render")
	TObjectPtr<UStaticMesh> RuntimeBladeMesh;

	/** Serialized for save/load; hidden from Details to avoid editor UI cost on large arrays. */
	UPROPERTY(VisibleInstanceOnly, Category = "GrassPCG|Internal", meta = (HideInDetailPanel))
	TArray<FGrassInstanceData> GrassInstances;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Clumps", meta = (TitleProperty = "CenterWS"))
	TArray<FGrassClumpData> GrassClumps;

	/** Local XY axis-aligned rectangle for procedural generation (component space, cm). Z is ignored; instances sit at Z=0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate")
	FVector2f DistributionMin = FVector2f(-500.f, -500.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate")
	FVector2f DistributionMax = FVector2f(500.f, 500.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate", meta = (ClampMin = "1"))
	int32 NumClumps = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate", meta = (ClampMin = "1"))
	int32 NumInstances = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate")
	int32 DistributionRandomSeed = 1;

	/** Snap generated clump/instance Z to ground by line trace when generating distribution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate|GroundSnap")
	bool bSnapToGroundOnGenerate = true;

	/** Collision channel used by ground snap traces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate|GroundSnap")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

	/** Half-height of vertical trace segment in component local space (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate|GroundSnap", meta = (ClampMin = "1.0"))
	float GroundTraceHalfHeight = 50000.f;

	/** Extra local-space Z offset applied after snapping (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate|GroundSnap")
	float GroundZOffset = 0.f;

	/**
	 * If no ground hit is found (while snapping enabled), keep original generated Z (currently 0),
	 * or use `GroundZOffset` as a fallback.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate|GroundSnap")
	bool bKeepOriginalZWhenNoGroundHit = true;

	/** If true, **BeginPlay** runs **GenerateGrassDistribution** once (runtime). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate")
	bool bAutoGenerateOnBeginPlay = true;

	/** Editor only (ignored at runtime): on **OnRegister**, if **GrassInstances** is empty, run **GenerateGrassDistribution** once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GrassPCG|Generate")
	bool bEditorGenerateOnRegisterIfEmpty = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GrassPCG|Render", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassHISM;

	UFUNCTION(BlueprintCallable, Category = "GrassPCG|Render")
	UHierarchicalInstancedStaticMeshComponent* GetGrassHISM() const { return GrassHISM; }

	/** Push GrassInstances to GrassHISM (transforms + per-instance custom data). Call after changing instances at runtime. */
	UFUNCTION(BlueprintCallable, Category = "GrassPCG|Render")
	void SyncGrassInstancesToHISM();

	/**
	 * Procedural step: fill **GrassClumps** (K random centers) and **GrassInstances** (N random points) in the XY rectangle,
	 * assign each instance to the nearest clump in XY, then sync HISM. Does **not** run when only editing generate params (call explicitly or rely on BeginPlay / editor auto).
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GrassPCG|Generate", meta = (DisplayName = "Generate Grass"))
	void GenerateGrassDistribution();

protected:
	virtual void BeginPlay() override;
	virtual void PostLoad() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	/** Destroy **GrassHISM** before **Super** so **USceneComponent** does not re-parent the child to the attach grandparent (editor + runtime). */
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Editor: when the assigned grass material (or its parent) finishes recompiling, refresh HISM draw state. */
	void OnEditorGrassMaterialCompiled(UMaterialInterface* CompiledMaterial);
#endif

private:
	void ConfigureGrassHISM();
	void ApplyBladeMeshAndMaterial();
	void BuildRuntimeBladeMeshIfNeeded();
	void SyncRuntimeBladeMeshStaticMaterial(UMaterialInterface* Material);
	bool SampleGroundLocalZ(const FVector2f& LocalXY, float& OutLocalZ) const;

	// The blade mesh template dimensions used to convert clump absolute dimensions
	// into per-instance scale (so it works even when `BladeMesh` is preauthored at a non-unit size).
	float TemplateBladeHeight = 1.f;     // Z span in local space (BladeHeight direction)
	float TemplateBladeBaseWidth = 1.f;  // X span in local space (BladeBaseWidth direction)

	static void NormalizeDistributionRect(FVector2f& OutMin, FVector2f& OutMax);
};
