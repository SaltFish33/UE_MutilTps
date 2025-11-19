// Fill out your copyright notice in the Description page of Project Settings.


#include "OverHeadWidget.h"

#include "Components/TextBlock.h"

void UOverHeadWidget::SetDisplayText(const FString& Text)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(Text));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DisplayText is null in UOverHeadWidget::SetDisplayText"));
	}
}

void UOverHeadWidget::ShowPlayerNetRole(APawn* Player)
{
	ENetRole PlayerRole = Player ? Player->GetLocalRole() : ROLE_None;
	FString RoleText;
	switch (PlayerRole)
	{
	case ROLE_Authority:
		RoleText = TEXT("Authority");
		break;
	case ROLE_AutonomousProxy:
		RoleText = TEXT("Autonomous Proxy");
		break;
	case ROLE_SimulatedProxy:
		RoleText = TEXT("Simulated Proxy");
		break;
	case ROLE_None:
	default:
		RoleText = TEXT("None");
		break;
	}

	this->SetDisplayText(RoleText);
}

void UOverHeadWidget::NativeDestruct()
{
	RemoveFromParent();
	Super::NativeDestruct();
}
