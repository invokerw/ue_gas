#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/Core/CombatTypes.h"
#include "CombatMinimapWidget.generated.h"

class ACombatUnitCharacter;
class UImage;
class UTextBlock;
class UTexture2D;

/** 小地图中的静态地形类别；只描述表现，不参与碰撞、导航或玩法判定。 */
UENUM(BlueprintType)
enum class ECombatMinimapTerrainType : uint8
{
	Boundary UMETA(DisplayName="边界"),
	Blocker UMETA(DisplayName="障碍"),
	Landmark UMETA(DisplayName="地标")
};

/** 从关卡真实世界范围投影出的稳定地图特征；动态单位和镜头轮廓不写入此数组。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatMinimapTerrainFeature
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="世界最小值", ToolTip="特征覆盖的最小世界 XY，单位厘米；来源应是关卡中的静态几何范围。", Units="cm"))
	FVector2D WorldMin = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="世界最大值", ToolTip="特征覆盖的最大世界 XY，单位厘米；必须大于世界最小值。", Units="cm"))
	FVector2D WorldMax = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="地形类别", ToolTip="只影响小地图颜色、轮廓和图例，不改变关卡碰撞或单位移动规则。"))
	ECombatMinimapTerrainType Type = ECombatMinimapTerrainType::Blocker;
	UPROPERTY(VisibleAnywhere, Category="Minimap", meta=(DisplayName="文字（已禁用）", ToolTip="为兼容旧配置保留；当前小地图不渲染任何文字。"))
	FText Label;
	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="填充颜色", ToolTip="小地图上的低对比填充色；不应使用高亮颜色冒充动态单位。"))
	FLinearColor FillColor = FLinearColor(0.08f, 0.18f, 0.24f, 0.92f);
	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="轮廓颜色", ToolTip="小地图上的静态地标轮廓色；动态单位颜色由 Widget 统一管理。"))
	FLinearColor OutlineColor = FLinearColor(0.24f, 0.62f, 0.70f, 0.82f);

	/** 检查范围和颜色是否可用于原生绘制。 */
	bool IsValid() const;
};

/** 每个关卡独立的只读底图；世界 +X 为北、+Y 为东，不参与导航或视野裁决。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatMinimapMap
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="地图包路径", ToolTip="完整地图包路径，例如 /Game/Combat/Demo/Maps/L_CombatDemo；精确匹配当前 World，PIE 前缀自动移除。"))
	FName MapPackage;
	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="世界最小值", ToolTip="底图覆盖的最小世界 XY，单位厘米；两轴必须分别小于最大值。", Units="cm"))
	FVector2D WorldMin = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="世界最大值", ToolTip="底图覆盖的最大世界 XY，单位厘米；无效或零面积配置隐藏小地图。", Units="cm"))
	FVector2D WorldMax = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="地面参考高度", ToolTip="镜头视口射线的参考地面平面，单位厘米；移动仍通过 CombatGround 查询真实地面。", Units="cm"))
	float GroundZ = 0;
	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="静态底图", ToolTip="与边界同方向的俯视纹理，顶边朝 +X；空值使用原生暗色地表、网格和关卡几何特征。"))
	TObjectPtr<UTexture2D> Texture;
	UPROPERTY(EditAnywhere, Category="Minimap", meta=(DisplayName="地形几何", ToolTip="由关卡静态几何整理出的只读特征；当前只绘制形状，不绘制任何文字。"))
	TArray<FCombatMinimapTerrainFeature> TerrainFeatures;
	/** 只验证表现配置的有限数值和面积；不会授予地图或单位控制权限。 */
	bool IsValid() const;
};

/**
 * 小地图生成配置的磁盘桥接；编辑器生成器写入项目 Config，运行时只读取匹配地图包的表现数据。
 * 配置缺失或无法解析时调用方必须保留内置回退，不能把 JSON 当作碰撞、导航或移动权威。
 */
namespace CombatMinimapConfig
{
	/** 返回项目级生成配置的稳定路径；不同地图以 mapPackage 字段共存。 */
	COMBAT_API FString GetGeneratedConfigPath();
	/** 将配置编码为可审阅 JSON；不写文件，也不修改输入数组。 */
	COMBAT_API bool Serialize(const TArray<FCombatMinimapMap>& Maps, FString& OutJson, FString* OutError = nullptr);
	/** 解析生成 JSON；任何一张地图结构非法都会拒绝整个文件，调用方再使用内置回退。 */
	COMBAT_API bool Deserialize(const FString& Json, TArray<FCombatMinimapMap>& OutMaps, FString* OutError = nullptr);
	/** 从项目 Config 读取生成配置；文件不存在返回 false，不产生运行时副作用。 */
	COMBAT_API bool Load(TArray<FCombatMinimapMap>& OutMaps, FString* OutError = nullptr);
#if WITH_EDITOR
	/** 编辑器按钮使用的原子写入入口；失败时保留旧文件。 */
	COMBAT_API bool Save(const TArray<FCombatMinimapMap>& Maps, FString* OutError = nullptr);
#endif
}

/** 在玩家屏幕设计坐标中计算出的正方形区域；无足够空间时不显示。 */
struct FCombatMinimapLayout
{
	FVector2D Position = FVector2D::ZeroVector;
	float Size = 0;
	bool bVisible = false;
};

/** 单个公开标记缓存；弱引用与生命号避免销毁/重生后的旧提示。 */
struct FCombatMinimapMarker
{
	TWeakObjectPtr<ACombatUnitCharacter> Unit;
	uint32 LifeGeneration = 0;
	FVector2D UV = FVector2D::ZeroVector;
	ECombatTeamRelation Relation = ECombatTeamRelation::Invalid;
	FText Name;
	bool bPrimary = false;
};

/**
 * 本地小地图：原生绘制提供底图回退/外框/地形几何，Blueprint 可选绑定纹理但不显示文字，C++ 提供投影、公开标记、相机范围与输入。
 * HUD 持有 Widget；缓存仅用弱 Actor 引用，无异步加载、Unit 委托或 gameplay 定时器。
 */
UCLASS(Blueprintable, meta=(DisplayName="战斗小地图", ToolTip="左下角只读空间投影；镜头与 Ping 只影响本地，移动复用现有选中组 Order。"))
class COMBAT_API UCombatMinimapWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UCombatMinimapWidget(const FObjectInitializer& ObjectInitializer);
	/** 使用玩家屏幕的真实 DPI 几何避让主 HUD；由 HUD 调用，隐藏后仍能响应视口恢复。 */
	void ConfigureViewportLayout(float StandardSize, float CompactSize, float InMargin, float InGap);
	void UpdateViewportLayout();
	/** 只命中实际显示的正方形外框，不拦截整个游戏视口。 */
	bool IsScreenPositionOverUI(FVector2D ScreenPosition) const;
	/** 有效 World 坐标转换到北向上的全图 UV；越界返回 false，但输出保留未夹紧值以裁剪镜头。 */
	static bool ProjectWorldToMap(const FVector2D& World, const FVector2D& Min, const FVector2D& Max, FVector2D& UV);
	/** 只接受 [0,1] 内的有限全图 UV；无效输入不写输出。 */
	static bool UnprojectMapToWorld(const FVector2D& UV, const FVector2D& Min, const FVector2D& Max, FVector2D& World);
	/** 按固定缩放把地图视角中心限制在全图范围内；缩放不改变世界事实。 */
	static FVector2D ClampViewCenter(FVector2D CenterUV, float Zoom);
	/** 先尝试 220，再尝试 184；横向不足时抬到主 HUD 上方，极小屏幕隐藏。 */
	static FCombatMinimapLayout ResolveLayout(FVector2D ViewSize, const FSlateRect& HeroRect, bool bHeroVisible,
		float StandardSize = 220.0f, float CompactSize = 184.0f, float Margin = 22.0f, float Gap = 12.0f);
	/** 精确匹配完整包路径；返回 INDEX_NONE 时不能绘制或接受地图输入。 */
	static int32 FindMap(const TArray<FCombatMinimapMap>& Configs, const FString& WorldPackage);
	/** 共享 Widget/联机测试的单次意图入口；边框、释放、双击不走此入口。 */
	bool ActivateMapPoint(FVector2D UV, FKey Button, bool bAlt);
	/** 验证当前 World 与 Designer 绑定，清空旧表现；重复初始化不会保留上一局状态。 */
	void RefreshMapConfiguration();
	/** 读取公开 View 与弱缓存，20 Hz 表现刷新；参数只推进本地 Ping 时钟。 */
	void RefreshPresentation(float DeltaSeconds);
	bool HasMapConfiguration() const { return ActiveMap != INDEX_NONE; }
	const TArray<FCombatMinimapMarker>& GetMarkers() const { return Markers; }
	float GetPingRemaining() const { return PingRemaining; }
	const FCombatMinimapMap* GetActiveMap() const { return Maps.IsValidIndex(ActiveMap) ? &Maps[ActiveMap] : nullptr; }
	/** 只读访问内置与编辑器生成的地图配置，供验证生成结果而不允许运行时改写。 */
	const TArray<FCombatMinimapMap>& GetMapConfigurations() const { return Maps; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& Event) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Minimap", meta=(DisplayName="地图配置", ToolTip="每个地图包仅配置一次；无匹配项或配置重复时隐藏小地图。静态底图不决定移动合法性。", TitleProperty="MapPackage"))
	TArray<FCombatMinimapMap> Maps;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Minimap", meta=(DisplayName="主控颜色", ToolTip="已确认主控的菱形标记颜色；其他玩家不能因高亮获得控制权。"))
	FLinearColor PrimaryColor = FLinearColor(0.12f, 0.94f, 0.86f);
	UPROPERTY(EditDefaultsOnly, Category="Combat|Minimap", meta=(DisplayName="友军颜色", ToolTip="公共 TeamSubsystem 判为友军的圆形标记颜色。"))
	FLinearColor FriendlyColor = FLinearColor(0.12f, 0.46f, 1.0f);
	UPROPERTY(EditDefaultsOnly, Category="Combat|Minimap", meta=(DisplayName="敌方颜色", ToolTip="通过公开目标规则的敌军三角标记颜色；当前没有战争迷雾保密。"))
	FLinearColor HostileColor = FLinearColor(1.0f, 0.12f, 0.17f);
	UPROPERTY(EditDefaultsOnly, Category="Combat|Minimap", meta=(DisplayName="中立与 Ping 颜色", ToolTip="中立方块和本地短暂 Ping 的琥珀色；Ping 不复制给其他玩家。"))
	FLinearColor NeutralColor = FLinearColor(1.0f, 0.62f, 0.12f);
	UPROPERTY(EditDefaultsOnly, Category="Combat|Minimap", meta=(ClampMin="1.0", ClampMax="4.0", DisplayName="地图视角缩放", ToolTip="固定的本地地图视角缩放；小地图拖动只移动本地镜头，不改变底图与标记视角。"))
	float ViewZoom = 1.0f;
	UPROPERTY(meta=(BindWidgetOptional, DisplayName="地图表面", ToolTip="Designer 中可选的纹理 Image；缺少时使用原生暗底网格，不影响地图交互。"))
	TObjectPtr<UImage> MapSurface;
	UPROPERTY(meta=(BindWidgetOptional, DisplayName="地图状态（已禁用）", ToolTip="保留旧 Blueprint 绑定兼容性；当前小地图强制清空并隐藏状态文字。"))
	TObjectPtr<UTextBlock> MapStatus;

private:
	/** 只在构建或 0.5 秒发现节拍枚举 World；每帧绘制不扫描全部 Actor。 */
	void DiscoverUnits();
	/** 使用公共 Targeting 规则与公开 View 过滤；死亡/隐藏/不同生命都不留下旧标记。 */
	bool CanDisplayUnit(ACombatUnitCharacter* Unit) const;
	/** 清空不属于新主控/新生命的提示与 Ping。 */
	void ClearTransientPresentation();
	/** 从真实 MapSurface 几何取局部坐标，排除外框。 */
	bool PointerToUV(FVector2D Position, FVector2D& UV) const;
	/** 将全图 UV 转换到当前可见视口；视口外返回 false。 */
	bool GlobalToViewUV(const FVector2D& GlobalUV, FVector2D& ViewUV) const;
	/** 将当前可见视口 UV 反投影到全图 UV。 */
	bool ViewToGlobalUV(const FVector2D& ViewUV, FVector2D& GlobalUV) const;
	/** 根据拖动产生的屏幕增量平移本地相机；地图底图与标记不改写视角中心。 */
	void UpdateCameraFromDrag(FVector2D ScreenPosition);
	/** 判断按下事件是否应先定位镜头；Alt+左键和中键只保留各自的 Ping/拖动语义。 */
	static bool ShouldRelocateOnPress(FKey Button, bool bAlt);
	/** 释放拖动状态并还原鼠标捕获。 */
	void ResetPointerState();
	/** 使用实际本地玩家视口四角与地面平面求交；失败时省略轮廓，不伪造视野。 */
	void PaintCamera(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer) const;
	/** 绘制当前地图的静态地形几何；只消费配置，不扫描或修改关卡 Actor，也不绘制文字。 */
	void PaintTerrain(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer) const;
	/** 将原始镜头轮廓裁剪到地图表面；不平移或缩放，靠边时只保留实际可见部分。 */
	static bool ClipCameraTrapezoidToMap(TArray<FVector2D>& Points, const FVector2D& Size);
	static void DrawLines(FSlateWindowElementList& Elements, int32 Layer, const FPaintGeometry& Geometry,
		const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness, bool bClosed = false);
	static FVector2D ToLocal(const FVector2D& UV, const FVector2D& Origin, const FVector2D& Size);
	TArray<TWeakObjectPtr<ACombatUnitCharacter>> TrackedUnits;
	TArray<FCombatMinimapMarker> Markers;
	TWeakObjectPtr<ACombatUnitCharacter> LastPrimary;
	TWeakObjectPtr<ACombatUnitCharacter> HoveredUnit;
	int32 LastBinding = INDEX_NONE;
	uint32 LastLife = 0;
	bool bLastReady = false;
	int32 ActiveMap = INDEX_NONE;
	FVector2D PingUV = FVector2D::ZeroVector;
	float PingRemaining = 0;
	float DiscoveryElapsed = 0;
	float PresentationElapsed = 0;
	FCombatMinimapLayout Layout;
	float StandardLayoutSize = 220.0f;
	float CompactLayoutSize = 184.0f;
	float LayoutMargin = 22.0f;
	float LayoutGap = 12.0f;
	FVector2D ViewCenterUV = FVector2D(0.5f, 0.5f);
	FVector2D PointerDownScreen = FVector2D::ZeroVector;
	FVector2D PointerPreviousScreen = FVector2D::ZeroVector;
	FKey PointerButton;
	bool bPointerDown = false;
	bool bDraggingCamera = false;
	bool bPointerAlt = false;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCombatMinimapLifecycleTest;
	friend class FCombatMinimapInputTest;
	friend class FCombatMinimapCameraTrapezoidTest;
	friend class FCombatMinimapTerrainTest;
#endif
};
