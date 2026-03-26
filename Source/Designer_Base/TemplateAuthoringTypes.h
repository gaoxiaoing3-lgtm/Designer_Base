#pragma once

#include "CoreMinimal.h"
#include "TemplateAuthoringTypes.generated.h"

UENUM(BlueprintType)
enum class EDesignerTemplateCategory : uint8
{
	LowerThird UMETA(DisplayName = "Lower Third"),
	Fullscreen UMETA(DisplayName = "Fullscreen"),
	Bug UMETA(DisplayName = "Bug")
};

UENUM(BlueprintType)
enum class EDesignerTemplateParamType : uint8
{
	String UMETA(DisplayName = "String"),
	Color UMETA(DisplayName = "Color"),
	Image UMETA(DisplayName = "Image"),
	Boolean UMETA(DisplayName = "Boolean")
};

USTRUCT(BlueprintType)
struct DESIGNER_BASE_API FDesignerTemplateParamDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	EDesignerTemplateParamType Type = EDesignerTemplateParamType::String;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	bool bRequired = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FString DefaultValue;
};

USTRUCT(BlueprintType)
struct DESIGNER_BASE_API FDesignerTemplateDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FString TemplateCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FText TemplateName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	EDesignerTemplateCategory Category = EDesignerTemplateCategory::LowerThird;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	TArray<FName> SupportedActions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	TArray<FDesignerTemplateParamDefinition> Parameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FSoftClassPath ControllerClassPath;
};
