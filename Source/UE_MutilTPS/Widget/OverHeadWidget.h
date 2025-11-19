// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OverHeadWidget.generated.h"

class UTextBlock;
/**
 * 
 */
UCLASS()
class UE_MUTILTPS_API UOverHeadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DisplayText;

	UFUNCTION()
	void SetDisplayText(const FString& Text);

	UFUNCTION(BlueprintCallable)
	void ShowPlayerNetRole(APawn* Player);
protected:
	virtual void NativeDestruct() override;
};
