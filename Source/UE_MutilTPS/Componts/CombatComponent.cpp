// 文件说明：
// CombatComponent 负责角色的战斗相关逻辑（当前简化为装备武器的功能）。
// - 本组件绑定到玩家角色（PlayerCharacter）上，保存当前已装备武器指针（EquippedWeapon）。
// - 设计原则：武器的“装备/卸下”逻辑需要处理视觉 Attach（Socket）、状态切换、以及 UI（拾取提示）变化。
// - 注意：实际的网络同步（例如谁可以调用 EquipWeapon）在调用端（如 PlayerCharacter）应由权威性（HasAuthority）控制；本组件本身标记为可复制，但 EquipWeapon 的调用位置决定了是否需要额外的 RPC。

#include "CombatComponent.h"

#include "PlayerCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "UE_MutilTPS/Weapon/WeaponBase.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, bIsAiming);
}


void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	
}

void UCombatComponent::OnRep_EquippedWeapon()
{
	if (this->EquippedWeapon && PlayerCharacter)
	{
		PlayerCharacter->GetCharacterMovement()->bOrientRotationToMovement = false;
		PlayerCharacter->bUseControllerRotationYaw = true;
	}
}


void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

// EquipWeapon 说明：
// 参数：Weapon - 要装备的武器指针（AWeaponBase*），外部（通常是服务器）传入。
// 行为：
// 1. 如果已有武器，则先将其状态标记为 Dropped（EWS_Dropped），以便在游戏逻辑/外观上区分“已卸下”的武器。
// 2. 将成员 EquippedWeapon 指向新武器，并设置状态为 Equipped（EWS_Equipped）。
// 3. 查找角色 Mesh 上的名为 "RightHandSocket" 的插槽，并把 Weapon Actor 以该插槽为父附着，使武器随骨骼动画移动。
// 4. 调用 SetOwner 将武器的 Owner 指向角色（这有助于权限判断及后续的复制/销毁行为）。
// 5. 隐藏武器上的拾取提示 Widget（因为装备后不应再提示可拾取）。
// 设计理由与注意事项：
// - AttachActor 使用 Socket 可以保证武器与骨骼动画同步（例如手部握持），而不是简单移动 Actor 的世界位置。
// - SetWeaponState 的转换用于在其它逻辑中判断武器当前行为（可被拾取、在手中、掉落物理存在等）。
// - Widget 的显示仅作视觉提示，通常由客户端展示，但在服务器驱动的流程中，服务器修改状态并通过复制/通知使客户端更新 UI。
// - 为避免空指针崩溃，函数入口对 PlayerCharacter 与 Weapon 做了空指针校验。
// - 如果需要支持客户端请求装备（比如按键触发），应通过 Server RPC 在服务器端执行 EquipWeapon，从而保证服务器为权威来源。
void UCombatComponent::EquipWeapon(AWeaponBase* Weapon)
{
	if (PlayerCharacter == nullptr || Weapon == nullptr)
	{
		return;
	}

	if (this->EquippedWeapon)
	{
		this->EquippedWeapon->SetWeaponState(EWeaponState::EWS_Dropped);
	}
	this->EquippedWeapon = Weapon;
	this->EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	const USkeletalMeshSocket* RightHandSocket = this->PlayerCharacter->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (RightHandSocket)
	{
		RightHandSocket->AttachActor(EquippedWeapon, this->PlayerCharacter->GetMesh());
	}
	this->EquippedWeapon->SetOwner(PlayerCharacter);
	if (PlayerCharacter->IsLocallyControlled())
	{
		PlayerCharacter->GetCharacterMovement()->bOrientRotationToMovement = false;
		PlayerCharacter->bUseControllerRotationYaw = true;
	}
}

void UCombatComponent::SetAiming(bool IsAiming)
{
	this->bIsAiming = IsAiming;
	if (!PlayerCharacter->HasAuthority())
	{
		this->ServerSetAiming(IsAiming);
	}
}

void UCombatComponent::ServerSetAiming_Implementation(bool IsAiming)
{
	this->bIsAiming = IsAiming;
}


