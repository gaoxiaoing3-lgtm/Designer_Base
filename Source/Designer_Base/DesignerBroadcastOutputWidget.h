#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DesignerBroadcastOutputWidget.generated.h"

class UCanvasPanel;
class UDesignerLowerThirdPreviewWidget;

UCLASS()
class DESIGNER_BASE_API UDesignerBroadcastOutputWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	void SetOutputData(
		const FString& InTitle,
		const FString& InSubtitle,
		const FString& InLogoPath,
		const FLinearColor& InThemeColor,
		bool bInShowLogo,
		bool bInIsOnAir);

private:
	void BuildLayout();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UDesignerLowerThirdPreviewWidget> PreviewWidget;
};
