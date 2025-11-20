// 文件说明：
// AWeaponBase 封装武器物体的视觉 Mesh、拾取 Sphere 以及头顶拾取 Widget。
// 关键点：在服务器（HasAuthority）上启用碰撞/重叠并注册 OnComponentBeginOverlap 回调；Widget 的显示可以在客户端控制（PickUpWidget 仅用于 UI）。

#include "WeaponBase.h"

#include "PlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// WeaponMesh 作为根组件并设置碰撞响应（阻挡大多数通道，但忽略 Pawn），
	// 初始设置为 NoCollision，是为了让武器被拾取或切换状态时再启用/禁用碰撞。
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionResponseToAllChannels(ECR_Block);
	WeaponMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);

	// AreaSphere 用于触发玩家进入拾取范围（Overlap），初始禁用碰撞，BeginPlay 时在服务器上启用
	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	AreaSphere->SetupAttachment(RootComponent);
	AreaSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);

	// 拾取提示 UI 组件
	PickUpWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickUpWidget"));
	PickUpWidget->SetupAttachment(RootComponent);
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	if (PickUpWidget)
	{
		// 初始化时隐藏拾取 Widget，只有在玩家进入范围或特定条件下才显示
		PickUpWidget->SetVisibility(false);
	}
	if (HasAuthority())
	{
		// 仅在服务器上启用 AreaSphere 的 Overlap，这样服务器可以通知/更新玩家的重叠武器状态。
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
		AreaSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		// 在服务器注册重叠回调，回调中会把 this 记录到玩家的 OverlappingWeapon（并通过复制同步到客户端）
		AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &AWeaponBase::OnSphereOverLap);
		AreaSphere->OnComponentEndOverlap.AddDynamic(this, &AWeaponBase::OnSphereEndOverLap);
	}
}

void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWeaponBase::OnSphereOverLap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 当其他 Actor 进入 AreaSphere 时被调用（仅在服务器执行）
	// 如果 OtherActor 是玩家，则将武器设置为该玩家的 OverlappingWeapon（由玩家对象负责显示 UI）
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(OtherActor);
	if (PlayerCharacter)
	{
		PlayerCharacter->SetOverlappingWeapon(this);
	}
}

void AWeaponBase::OnSphereEndOverLap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(OtherActor);
	if (PlayerCharacter)
	{
		PlayerCharacter->SetOverlappingWeapon(nullptr);
	}
}

void AWeaponBase::ShowPickUpWidget(bool bShowWidget)
{
	// 控制拾取 UI 的可见性。虽然 WidgetComponent 可以在服务器上存在，但实际的可视化通常由客户端执行，
	// 这里直接调用 WidgetComponent 的 SetVisibility 可以在客户端上反映状态（如果组件被复制/存在于客户端）。
	if (PickUpWidget)
	{
		PickUpWidget->SetVisibility(bShowWidget);
	}
}
