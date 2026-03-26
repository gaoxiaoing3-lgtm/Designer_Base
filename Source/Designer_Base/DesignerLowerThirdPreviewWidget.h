#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DesignerLowerThirdPreviewWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UTexture2D;

UCLASS()
class DESIGNER_BASE_API UDesignerLowerThirdPreviewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void SetPreviewData(
		const FString& InTitle,
		const FString& InSubtitle,
		const FString& InLogoPath,
		const FLinearColor& InThemeColor,
		bool bInShowLogo,
		bool bInIsOnAir);

	void SetBroadcastMode(bool bInBroadcastMode);

private:
	void BuildLayout();
	void RefreshView();
	void RefreshLogoImage();
	void RefreshAnimatedState();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> AccentBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LogoBorder;

	UPROPERTY(Transient)
	TObjectPtr<UImage> LogoImage;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> MainRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleTextBlock;

	UPROPERTY(Transient)
	FString CurrentTitle = TEXT("Headline");

	UPROPERTY(Transient)
	FString CurrentSubtitle = TEXT("Sub line");

	UPROPERTY(Transient)
	FLinearColor CurrentThemeColor = FLinearColor(1.0f, 0.666f, 0.0f, 1.0f);

	UPROPERTY(Transient)
	FString CurrentLogoPath;

	UPROPERTY(Transient)
	FString LoadedLogoPath;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedLogoTexture;

	UPROPERTY(Transient)
	bool bShowLogo = true;

	UPROPERTY(Transient)
	bool bIsOnAir = false;

	UPROPERTY(Transient)
	bool bBroadcastMode = false;

	UPROPERTY(Transient)
	float AnimationProgress = 0.0f;

	UPROPERTY(Transient)
	float AnimationDuration = 0.35f;
};
