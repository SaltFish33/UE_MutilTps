// 文件说明：
// AWeaponBase 表示场景中的武器 Actor，包含用于检测玩家进入拾取范围的 AreaSphere、武器的可视 Mesh 和头顶提示 Widget。
// 设计与网络原则：
// - 碰撞/重叠检测在服务器上启用以保证权威性，服务器负责告知对应玩家（Owner）哪个武器处于可拾取状态。
// - Widget 的显示为视觉层面的表现，通常由客户端控制显示与隐藏。但为了简化逻辑，服务器仍可通过复制或在 OwnerOnly 条件下更新客户端组件属性。
// - WeaponState 枚举用于管理武器在世界中的生命周期（Init/Equipped/Dropped），其他系统可依据该状态做不同处理。

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
	if (PickUpWidget != nullptr)
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

// BeginPlay 说明：
// - 初始化 PickUpWidget 为隐藏（避免初始帧看到提示）。
// - 仅在 HasAuthority() 时启用 AreaSphere 的碰撞与重叠回调：
//    * 服务器对 Pawn 的 Overlap 事件负责检测并通知 PlayerCharacter，进而服务器再通过 Replicated 变量把 UI 状态同步到拥有者客户端。
//    * 在多客户端环境下，如果在客户端启用重叠检测，会导致权威冲突或不一致性，因此这里只在服务器上注册回调。

void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeaponBase, WeaponState);
}

// OnSphereOverLap 说明：
// 参数与 OnComponentBeginOverlap 相同（UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, ...）
// 行为：
// - 将碰撞到的 Actor 尝试转换为 APlayerCharacter，如果成功则调用 PlayerCharacter->SetOverlappingWeapon(this)。
// - 设计理由：把“谁与武器重叠”这一决定放在服务器，服务器调用角色对象去记录当前的 OverlappingWeapon，从而利用 RepNotify 让拥有者客户端展示 UI。
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

// OnSphereEndOverLap 说明：当角色离开拾取范围时被调用，行为与 OnSphereOverLap 相反，将玩家的 OverlappingWeapon 设为 nullptr。
void AWeaponBase::OnSphereEndOverLap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(OtherActor);
	if (PlayerCharacter)
	{
		PlayerCharacter->SetOverlappingWeapon(nullptr);
	}
}

void AWeaponBase::Fire(const FVector_NetQuantize& HitTarget)
{
	
}

void AWeaponBase::OnRep_WeaponState()
{
	this->SetWeaponState(this->WeaponState);
}

// ShowPickUpWidget 说明：
// 参数 bShowWidget：是否显示头顶拾取提示。
// 说明：WidgetComponent 的 SetVisibility 既可以在服务器上执行（如果组件被复制到客户端），也可以在客户端本地控制以获得更即时的视觉反馈。
// - 推荐做法：使用 OwnerOnly 的复制和 RepNotify 在拥有者客户端控制 Widget 的显示；对于延迟敏感的 UI，可在本地（IsLocallyControlled）侧也进行显示以优化体验。
void AWeaponBase::ShowPickUpWidget(bool bShowWidget)
{
	// 控制拾取 UI 的可见性。虽然 WidgetComponent 可以在服务器上存在，但实际的可视化通常由客户端执行，
	// 这里直接调用 WidgetComponent 的 SetVisibility 可以在客户端上反映状态（如果组件被复制/存在于客户端）。
	if (PickUpWidget != nullptr)
	{
		PickUpWidget->SetVisibility(bShowWidget);
	}
}

void AWeaponBase::SetWeaponState(EWeaponState State)
{
	this->WeaponState = State;
	switch (this->WeaponState)
	{
		case EWeaponState::EWS_Equipped:
			this->ShowPickUpWidget(false);
			this->AreaSphere->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
			break;
		
		default:
			break;
	}
}

