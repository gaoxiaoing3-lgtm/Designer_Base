#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "DesignerTemplateActor.h"
#include "DesignerLowerThirdTemplateActor.generated.h"

class UDesignerLowerThirdPreviewWidget;

UCLASS(Blueprintable)
class DESIGNER_BASE_API ADesignerLowerThirdTemplateActor : public ADesignerTemplateActor
{
	GENERATED_BODY()

public:
	ADesignerLowerThirdTemplateActor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	FString CurrentTitle = TEXT("Headline");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	FString CurrentSubtitle = TEXT("Sub line");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	FLinearColor CurrentThemeColor = FLinearColor(1.0f, 0.666f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	FString CurrentLogoPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	bool bShowLogo = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Playback")
	bool bIsOnAir = false;

	UFUNCTION(BlueprintPure, Category = "Template")
	FString GetFieldValue(const FName FieldKey) const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void ApplyFields_Implementation(const TMap<FName, FString>& InFields) override;
	virtual void TakeIn_Implementation() override;
	virtual void TakeOut_Implementation() override;
	virtual void CollectCurrentFields_Implementation(TMap<FName, FString>& OutFields) const override;
	virtual bool IsOnAir_Implementation() const override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Template")
	void OnFieldsUpdated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Template")
	void OnTakeInStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Template")
	void OnTakeOutStarted();

private:
	bool TryApplyColor(const FString& InValue);
	void ApplyDefaults();
	void RefreshPreviewWidget();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> PreviewWidgetComponent;
};
