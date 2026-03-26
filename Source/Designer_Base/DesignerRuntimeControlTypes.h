#pragma once

#include "CoreMinimal.h"
#include "TemplateAuthoringTypes.h"
#include "DesignerRuntimeControlTypes.generated.h"

USTRUCT(BlueprintType)
struct DESIGNER_BASE_API FDesignerControlCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FString RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FString Command;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FString TemplateCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	TMap<FName, FString> Fields;
};

USTRUCT(BlueprintType)
struct DESIGNER_BASE_API FDesignerRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	bool bHasActiveTemplate = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	bool bIsOnAir = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FString ActiveTemplateCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FText ActiveTemplateName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	TMap<FName, FString> ActiveFields;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	TArray<FDesignerTemplateDescriptor> AvailableTemplates;
};

USTRUCT(BlueprintType)
struct DESIGNER_BASE_API FDesignerControlResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FString RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FString Command;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FDesignerRuntimeState State;
};

USTRUCT(BlueprintType)
struct DESIGNER_BASE_API FDesignerEditableFieldValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	EDesignerTemplateParamType Type = EDesignerTemplateParamType::String;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	bool bRequired = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FString Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer|Control")
	FString DefaultValue;
};
