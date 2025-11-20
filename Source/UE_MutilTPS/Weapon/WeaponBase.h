// 文件说明：
// 定义武器的基本类型 AWeaponBase，包含 Mesh、Overlap Sphere、拾取 Widget 以及武器状态枚举。
// 设计要点：使用 EWeaponState 管理武器生命周期（Init/Equipped/Dropped），并在服务器控制重叠检测。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class UWidgetComponent;
class USphereComponent;

UENUM(BlueprintType)
enum class EWeaponState: uint8
{
	// 武器状态用于决定武器的行为（例如是否可被拾取、是否显示在手中等）
	EWS_Init UMETA(DisplayName="Init"),
	EWS_Equipped UMETA(DisplayName="Equipped"),
	EWS_Dropped UMETA(DisplayName="Dropped"),

	EWS_Max UMETA(DisplayName="DefaultMAX")
};

UCLASS()
class UE_MUTILTPS_API AWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AWeaponBase();
	virtual void Tick(float DeltaTime) override;

	// ShowPickUpWidget: 控制拾取提示 Widget 的显示
	// 参数 bShowWidget: true 显示，false 隐藏
	void ShowPickUpWidget(bool bShowWidget);
	
protected:
	virtual void BeginPlay() override;

	// OnSphereOverLap: AreaSphere 的重叠回调（在服务器上注册）
	// 参数说明与标准 OnComponentBeginOverlap 一致，用于检测玩家进入拾取范围
	UFUNCTION()
	virtual void OnSphereOverLap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	virtual void OnSphereEndOverLap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

private:
	// 武器 Mesh：视觉部分与碰撞设置（仅作展示 / 物理用）
	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	// 用于检测玩家进入范围的球形碰撞组件
	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<USphereComponent> AreaSphere;

	// 当前武器状态（Init/Equipped/Dropped），用于客户端/服务器逻辑分支
	UPROPERTY(VisibleAnywhere, Category="Weapon Properties")
	EWeaponState WeaponState;

	// 拾取提示 UI（WidgetComponent），在蓝图中可设置要显示的 Widget
	UPROPERTY(VisibleAnywhere, Category="Weapon Properties")
	TObjectPtr<UWidgetComponent> PickUpWidget;
};

