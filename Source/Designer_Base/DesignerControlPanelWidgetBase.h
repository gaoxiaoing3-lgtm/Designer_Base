#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DesignerControlPanelWidgetBase.generated.h"

class UDesignerControlPanelViewModel;

UCLASS(Abstract, Blueprintable)
class DESIGNER_BASE_API UDesignerControlPanelWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintPure, Category = "Designer|UI")
	UDesignerControlPanelViewModel* GetViewModel() const;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Designer|UI")
	void OnViewModelReady(UDesignerControlPanelViewModel* InViewModel);

	UPROPERTY(BlueprintReadOnly, Category = "Designer|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDesignerControlPanelViewModel> ViewModel;
};
