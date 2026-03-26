#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/PlayerController.h"
#include "DesignerControlPlayerController.generated.h"

class UDesignerLowerThirdPreviewWidget;
class UDesignerBroadcastOutputWidget;
class UUserWidget;
class SWindow;

UCLASS(Blueprintable)
class DESIGNER_BASE_API ADesignerControlPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADesignerControlPlayerController();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	void SetControlPanelWidgetClass(TSubclassOf<UUserWidget> InWidgetClass);

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	UUserWidget* EnsureControlPanelWidget();

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	void RemoveControlPanelWidget();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Designer|UI")
	TSubclassOf<UUserWidget> ControlPanelWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Designer|UI")
	TSubclassOf<UDesignerLowerThirdPreviewWidget> ViewportPreviewWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Designer|UI")
	TSubclassOf<UDesignerBroadcastOutputWidget> BroadcastOutputWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Designer|UI")
	bool bOpenBroadcastOutputWindow = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Designer|UI")
	TObjectPtr<UUserWidget> ControlPanelWidget;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Designer|UI")
	TObjectPtr<UDesignerLowerThirdPreviewWidget> ViewportPreviewWidget;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Designer|UI")
	TObjectPtr<UDesignerBroadcastOutputWidget> BroadcastOutputWidget;

	UPROPERTY(Transient)
	FTimerHandle PreviewRefreshTimerHandle;

	TSharedPtr<SWindow> BroadcastOutputWindow;

private:
	void EnsureViewportPreviewWidget();
	void EnsureBroadcastOutputWindow();
	void RefreshViewportPreview();
};
