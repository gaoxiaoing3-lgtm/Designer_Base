#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TemplateAuthoringTypes.h"
#include "DesignerTemplateActor.generated.h"

class FJsonObject;

UCLASS(Blueprintable)
class DESIGNER_BASE_API ADesignerTemplateActor : public AActor
{
	GENERATED_BODY()

public:
	ADesignerTemplateActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FString TemplateCode = TEXT("lowerthird_basic");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FText TemplateName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	EDesignerTemplateCategory Category = EDesignerTemplateCategory::LowerThird;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	TArray<FName> SupportedActions = {TEXT("In"), TEXT("Loop"), TEXT("Out")};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	TArray<FDesignerTemplateParamDefinition> Parameters;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Template")
	void ApplyFields(const TMap<FName, FString>& InFields);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Template")
	void TakeIn();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Template")
	void TakeOut();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Template")
	void CollectCurrentFields(TMap<FName, FString>& OutFields) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Template")
	bool IsOnAir() const;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Template")
	bool ExportTemplateDefinition();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void ApplyFields_Implementation(const TMap<FName, FString>& InFields);
	virtual void TakeIn_Implementation();
	virtual void TakeOut_Implementation();
	virtual void CollectCurrentFields_Implementation(TMap<FName, FString>& OutFields) const;
	virtual bool IsOnAir_Implementation() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Template")
	void SyncPreview();

private:
	FString BuildExportPath() const;
	FString GetCategoryString() const;
	TSharedPtr<FJsonObject> BuildTemplateJson() const;
};
