#include "DesignerTemplateActor.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

ADesignerTemplateActor::ADesignerTemplateActor()
{
	PrimaryActorTick.bCanEverTick = false;

	if (TemplateName.IsEmpty())
	{
		TemplateName = FText::FromString(TEXT("Lower Third Basic"));
	}
}

void ADesignerTemplateActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SyncPreview();
}

void ADesignerTemplateActor::ApplyFields_Implementation(const TMap<FName, FString>& InFields)
{
}

void ADesignerTemplateActor::TakeIn_Implementation()
{
}

void ADesignerTemplateActor::TakeOut_Implementation()
{
}

void ADesignerTemplateActor::CollectCurrentFields_Implementation(TMap<FName, FString>& OutFields) const
{
}

bool ADesignerTemplateActor::IsOnAir_Implementation() const
{
	return false;
}

bool ADesignerTemplateActor::ExportTemplateDefinition()
{
	const TSharedPtr<FJsonObject> RootObject = BuildTemplateJson();
	if (!RootObject.IsValid())
	{
		return false;
	}

	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		return false;
	}

	const FString ExportPath = BuildExportPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExportPath), true);

	return FFileHelper::SaveStringToFile(OutputString, *ExportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString ADesignerTemplateActor::BuildExportPath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TemplateExports"), TemplateCode + TEXT(".json"));
}

FString ADesignerTemplateActor::GetCategoryString() const
{
	switch (Category)
	{
	case EDesignerTemplateCategory::LowerThird:
		return TEXT("lowerthird");
	case EDesignerTemplateCategory::Fullscreen:
		return TEXT("fullscreen");
	case EDesignerTemplateCategory::Bug:
		return TEXT("bug");
	default:
		return TEXT("unknown");
	}
}

TSharedPtr<FJsonObject> ADesignerTemplateActor::BuildTemplateJson() const
{
	if (TemplateCode.IsEmpty())
	{
		return nullptr;
	}

	const TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("template_code"), TemplateCode);
	RootObject->SetStringField(TEXT("name"), TemplateName.ToString());
	RootObject->SetStringField(TEXT("category"), GetCategoryString());
	RootObject->SetStringField(TEXT("entry_level_path"), GetWorld() ? GetWorld()->GetPathName() : FString());
	RootObject->SetStringField(TEXT("controller_class_path"), GetClass()->GetPathName());

	TArray<TSharedPtr<FJsonValue>> ActionValues;
	for (const FName& ActionName : SupportedActions)
	{
		ActionValues.Add(MakeShared<FJsonValueString>(ActionName.ToString()));
	}
	RootObject->SetArrayField(TEXT("actions"), ActionValues);

	TArray<TSharedPtr<FJsonValue>> ParamValues;
	for (const FDesignerTemplateParamDefinition& Param : Parameters)
	{
		if (Param.Key.IsNone())
		{
			continue;
		}

		const TSharedPtr<FJsonObject> ParamObject = MakeShared<FJsonObject>();
		ParamObject->SetStringField(TEXT("key"), Param.Key.ToString());
		ParamObject->SetStringField(TEXT("label"), Param.Label.ToString());
		ParamObject->SetStringField(TEXT("type"),
			StaticEnum<EDesignerTemplateParamType>()->GetNameStringByValue(static_cast<int64>(Param.Type)).ToLower());
		ParamObject->SetBoolField(TEXT("required"), Param.bRequired);
		ParamObject->SetStringField(TEXT("default"), Param.DefaultValue);
		ParamValues.Add(MakeShared<FJsonValueObject>(ParamObject));
	}
	RootObject->SetArrayField(TEXT("params"), ParamValues);

	return RootObject;
}
