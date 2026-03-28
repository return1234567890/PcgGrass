# PcgGrass 插件 README

## 1. 插件介绍

`PcgGrass` 是基于虚幻5.7的程序化草生成插件。提供了一个用于“程序化草地”的运行时组件：根据一组分布参数生成草簇（clumps）与草实例（instances），并把实例转换为 `UHierarchicalInstancedStaticMeshComponent`（HISM）的实例变换与逐实例自定义数据。

当你未显式指定 `BladeMesh` 时，插件会根据 `RenderGrassLOD` 在运行时构建“单位刀片” `UStaticMesh`（并支持 LOD0/1/2）。实际草叶高度/宽度由每个 clump 的 `GrassClumps[].BladeHeight / BladeBaseWidth` 通过实例缩放与 PerInstanceCustomData 传递给材质。

## 2. 演示视频

  ![](resources/showcase/PCGGrass.gif)

## 3. 代码文件说明

以下文件位于 `Plugins/PcgGrass/Source/PcgGrass/`，是插件主要实现：

1. `PcgGrass.uplugin`  
   插件元信息与模块声明（`PcgGrass`，Runtime，`PostConfigInit`）。

2. `Source/PcgGrass/PcgGrass.Build.cs`  
   模块依赖声明；使用到 `MeshConversion`、`MeshDescription`、`StaticMeshDescription` 等用于运行时网格构建。

3. `Source/PcgGrass/Public/PcgGrass.h` 与 `Source/PcgGrass/Private/PcgGrass.cpp`  
   模块入口 `FPcgGrassModule`（目前 `StartupModule/ShutdownModule` 为空，主要用于模块注册）。

4. `Source/PcgGrass/Public/ProceduralGrassComponent.h` 与 `Source/PcgGrass/Private/ProceduralGrassComponent.cpp`  
   核心组件 `UProceduralGrassComponent`：
   - 管理草簇与草实例数据（`GrassClumps` / `GrassInstances`）
   - 在 `OnRegister / BeginPlay / PostLoad` 等阶段为 HISM 准备网格与材质
   - 提供 `GenerateGrassDistribution()`（生成分布）与 `SyncGrassInstancesToHISM()`（把数据送入渲染）

5. `Source/PcgGrass/Public/GrassInstanceData.h` 与 `Source/PcgGrass/Private/GrassInstanceData.cpp`  
   定义草实例与草簇的数据结构：
   - `FGrassInstanceData`：每根草刀片实例的根位置、方向、宽高、clump index、random seed 等
   - `FGrassClumpData`：每个草簇的中心、半径、风方向/强度/相位、颜色等

6. `Source/PcgGrass/Public/GrassBladeMeshBuilder.h` 与 `Source/PcgGrass/Private/GrassBladeMeshBuilder.cpp`  
   负责生成“草刀片网格”的几何数据（LOD0/1/2），由组件在运行时把这些数据转为 `UStaticMesh`。

## 4. 涉及的虚幻系统/组件

插件主要依赖并使用以下虚幻系统：

1. 组件体系（`USceneComponent` / `UActorComponent` 生命周期）
   - `UProceduralGrassComponent` 继承 `USceneComponent`
   - 使用 `OnRegister / OnUnregister / BeginPlay / PostLoad / PostEditChangeProperty`

2. 实例化渲染（HISM）
   - `UHierarchicalInstancedStaticMeshComponent`（`GrassHISM` 子组件）
   - 调用 `ClearInstances / AddInstances / SetCustomDataFloats / SetCustomData / MarkRenderStateDirty`
   - 使用 HISM 的 `PerInstanceCustomData`（本插件固定为 11 个 float）

3. 静态网格与运行时网格构建
   - `UStaticMesh`
   - `UStaticMeshDescription`、`FMeshDescriptionBuilder`、`FStaticMeshDescription` 相关构建流程
   - `FMeshDescription`、`FPolygonGroupID`、`FVertexID`、`FTriangleID` 等网格数据结构

4. 材质与编辑期回调
   - `UMaterialInterface` / `UMaterial` / `UMaterialInstance`
   - 编辑期：监听 `UMaterial::OnMaterialCompilationFinished()`，当草材质编译完成时刷新 HISM 渲染状态，并对 Lumen Surface Cache 做失效处理（`InvalidateLumenSurfaceCache`）

5. 随机与数值工具
   - `FRandomStream`：基于 `DistributionRandomSeed` 可复现生成

6. 运行时网格几何生成
   - `FDynamicMeshVertex` 与 `DynamicMeshBuilder.h`
   - `FGrassBladeMeshBuilder`：输出刀片顶点与三角形索引，再转换为 `UStaticMesh`

## 5. 使用方法

### 4.1 在关卡/Actor 中添加组件

1. 在蓝图或 C++ Actor 中添加 `UProceduralGrassComponent`
2. 设置生成/渲染参数：
   - `GrassClumps[].BladeHeight`：clump 对应的刀片高度（影响实例缩放与自定义数据）
   - `GrassClumps[].BladeBaseWidth`：clump 对应的刀片根部宽度（影响实例缩放与自定义数据）
   - `BladeLOD`（保留字段）：当前代码中用于编辑暴露，但渲染网格选择由 `RenderGrassLOD` 驱动
   - `RenderGrassLOD`：运行时构建刀片网格时选择的 LOD
   - `GrassMaterial`：HISM 材质槽 0 使用的材质（若为空将使用默认表面材质）
   - `BladeMesh`（可选）：若不为空，将直接使用该静态网格作为刀片
   - `GrassInstances` / `GrassClumps`：
     - 若你手动填好它们，可直接在运行时或编辑时同步到渲染
     - 若为空且满足条件，组件可自动生成分布

3. 生成策略开关：
   - `bAutoGenerateOnBeginPlay`：运行开始时若 `GrassInstances` 为空，则调用 `GenerateGrassDistribution()` 生成
   - `bEditorGenerateOnRegisterIfEmpty`：编辑期/注册阶段若 `GrassInstances` 为空，则调用 `GenerateGrassDistribution()` 生成

### 4.2 运行时调用（Blueprint/C++）

常见流程是：

1. 修改 `GrassInstances` / `GrassClumps`（例如外部系统更新数据）
2. 调用 `SyncGrassInstancesToHISM()` 把变换与自定义数据送入渲染

如果你希望基于分布参数重新生成：

1. 调用 `GenerateGrassDistribution()`
2. 该函数在组件已注册时会自动执行 `SyncGrassInstancesToHISM()`（未注册则只更新数据）

### 4.3 逐实例自定义数据（用于草材质/Shader）

`SyncGrassInstancesToHISM()` 会为每个 HISM 实例写入 11 个 float（索引含义如下）：

0. Height（实例高度缩放）
1. Width（实例宽度缩放）
2. RandomSeed（以 float 形式编码的 seed bits）
3. ClumpIndex（草簇索引，float）
4-6. ClumpColor 的 R/G/B
7-8. WindDirection（X/Y）
9. WindStrength
10. WindPhase01（将风相位弧度编码到 0..1）

对应的材质/渲染实现需要在 shader 中读取这些 custom data。

## 6. 组件/插件代码调用流程（初始化 -> runtime -> 销毁）

下面按 UE 生命周期总结 `UProceduralGrassComponent` 的关键路径（不包含 Tick，因为组件默认不启用 Tick）：

### 5.1 初始化阶段（构造与注册）

1. 模块加载：插件模块 `PcgGrass` 在 `PostConfigInit` 加载（`FPcgGrassModule::StartupModule()` 为空）
2. 组件构造（`UProceduralGrassComponent::UProceduralGrassComponent()`）：
   - 创建 `GrassHISM` 子组件并挂到自身
   - 配置 HISM：禁止碰撞与导航影响、开启阴影等

3. 组件注册（`OnRegister()`）：
   - 编辑期：注册 `UMaterial::OnMaterialCompilationFinished()` 回调（保证草材质重编译后能刷新绘制）
   - 调用 `ApplyBladeMeshAndMaterial()`：
     - 若 `BladeMesh` 存在：直接设置到 HISM
     - 若 `BladeMesh` 为空：调用 `BuildRuntimeBladeMeshIfNeeded()` 构建 `RuntimeBladeMesh`
     - 同时给 HISM 设置材质槽 0
   - 编辑期若启用 `bEditorGenerateOnRegisterIfEmpty` 且实例为空：调用 `GenerateGrassDistribution()`
   - 否则：调用 `SyncGrassInstancesToHISM()` 把数据送入渲染

4. 加载后处理（`PostLoad()`，非 CDO）：
   - `ApplyBladeMeshAndMaterial()`
   - `SyncGrassInstancesToHISM()`

### 5.2 Runtime 阶段（数据更新与送入渲染）

1. 开始游戏（`BeginPlay()`）：
   - `ApplyBladeMeshAndMaterial()`
   - 若 `bAutoGenerateOnBeginPlay` 且 `GrassInstances` 为空：调用 `GenerateGrassDistribution()`
   - 否则：调用 `SyncGrassInstancesToHISM()`

2. 数据更新路径（显式调用）
   - 你在运行时修改了 `GrassInstances` 或 `GrassClumps` 后，必须手动调用 `SyncGrassInstancesToHISM()`
   - `SyncGrassInstancesToHISM()` 内部执行：
     - 计算每个实例的本地变换（位置取自 `PositionWS`，朝向由 `Direction` 推导，缩放由宽/高计算）
     - 清空并重建 HISM 实例列表
     - 设置 `PerInstanceCustomData`（固定 11 个 float）
     - `MarkRenderStateDirty()` 触发渲染侧更新

3. 编辑期属性变更
   - `PostEditChangeProperty()`：任意改动会重新 `ApplyBladeMeshAndMaterial()` + `SyncGrassInstancesToHISM()`
   - 材质编译完成回调：若属于草材质链路，则刷新并失效 Lumen Surface Cache

### 5.3 销毁与释放（OnUnregister / 组件生命周期）

1. 组件注销（`OnUnregister()`）：
   - 编辑期：移除 `UMaterial::OnMaterialCompilationFinished()` 绑定
   - 调用 `Super::OnUnregister()`

2. 资源释放说明：
   - `GrassHISM` 是组件构造时作为默认子对象创建的；当 `UProceduralGrassComponent` 销毁时，子对象生命周期随之结束
   - `RuntimeBladeMesh` 是 `Transient` 的运行时对象（当你没有提供 `BladeMesh` 时才会创建）；由 Unreal 的 UObject 生命周期管理并在合适时机释放

