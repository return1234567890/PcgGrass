// Copyright Epic Games, Inc. All Rights Reserved.

#include "GrassInstanceData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GrassInstanceData)

static_assert(sizeof(FGrassInstanceData) == 40, "FGrassInstanceData CPU layout (used by HISM custom data / future buffers)");
static_assert(sizeof(FGrassClumpData) == 60, "FGrassClumpData CPU layout (+ FLinearColor ClumpColor + blade dims)");
