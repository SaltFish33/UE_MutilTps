// 文件说明（摘要）：
// AWeaponBase 定义了武器的基础结构：视觉 Mesh、检测玩家的 AreaSphere、用于提示的 Widget，以及武器状态枚举。
// 设计要点：
// - 枚举 EWeaponState 用于标识武器的当前生命周期（Init/Equipped/Dropped），便于其它系统（如拾取、掉落、物理）依据状态采取行为。
// - 重叠检测仅在服务器上启用，并通过 PlayerCharacter 的 Replicated 变量将 UI 状态同步到对应客户端，减少不必要的网络广播。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
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

	// 控制拾取提示显示/隐藏
	void ShowPickUpWidget(bool bShowWidget);

	// 直接设置武器状态（简单的 setter），其它模块会依据 WeaponState 做进一步处理
	void SetWeaponState(EWeaponState State);

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	FORCEINLINE
	TObjectPtr<UStaticMeshComponent> GetWeaponMesh() const { return WeaponMesh; }

	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<UAnimMontage> FireMontage;

	virtual void Fire(const FVector_NetQuantize& HitTarget);

	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<UTexture2D> CrosshairsCenter;

	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<UTexture2D> CrosshairsLeft;

	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<UTexture2D> CrosshairsRight;

	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<UTexture2D> CrosshairsTop;

	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<UTexture2D> CrosshairsBottom;

	// 相机FOV相关属性
	// 默认FOV（正常视角），如果为0则使用CombatComponent的默认值
	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties|Camera", meta=(ClampMin="0.0", ClampMax="170.0"))
	float DefaultFOV = 0.0f;

	// 瞄准时的FOV（更小的FOV提供更窄的视野，类似瞄准镜效果），如果为0则使用CombatComponent的默认值
	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties|Camera", meta=(ClampMin="0.0", ClampMax="170.0"))
	float AimingFOV = 0.0f;
	
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

	// OnSphereEndOverLap: AreaSphere 与 Actor 结束重叠时调用
	UFUNCTION()
	virtual void OnSphereEndOverLap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

private:
	// 武器 Mesh：视觉与物理表现
	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	// AreaSphere：用于检测玩家进入拾取范围（Overlap），仅在服务器上启用以保证权威性
	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<USphereComponent> AreaSphere;

	UPROPERTY(EditDefaultsOnly, Category="Weapon Properties")
	TObjectPtr<UAnimationAsset> FireAnimation;

	// 当前武器状态，供游戏逻辑区分行为
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_WeaponState, Category="Weapon Properties")
	EWeaponState WeaponState;

	UFUNCTION()
	void OnRep_WeaponState();

	// 拾取提示 UI（WidgetComponent），用于在玩家靠近时显示交互提示
	UPROPERTY(VisibleAnywhere, Category="Weapon Properties")
	TObjectPtr<UWidgetComponent> PickUpWidget;
};
