#include "DesignerLowerThirdTemplateActor.h"

#include "Components/SceneComponent.h"
#include "DesignerLowerThirdPreviewWidget.h"

ADesignerLowerThirdTemplateActor::ADesignerLowerThirdTemplateActor()
{
	if (!RootComponent)
	{
		USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
		SetRootComponent(SceneRoot);
	}

	PreviewWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PreviewWidgetComponent"));
	PreviewWidgetComponent->SetupAttachment(RootComponent);
	PreviewWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	PreviewWidgetComponent->SetDrawAtDesiredSize(true);
	PreviewWidgetComponent->SetPivot(FVector2D(0.0f, 1.0f));
	PreviewWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	PreviewWidgetComponent->SetWidgetClass(UDesignerLowerThirdPreviewWidget::StaticClass());

	TemplateCode = TEXT("lowerthird_basic");
	TemplateName = FText::FromString(TEXT("Lower Third Basic"));
	Category = EDesignerTemplateCategory::LowerThird;
	SupportedActions = {TEXT("In"), TEXT("Loop"), TEXT("Out")};

	Parameters.Reset();

	FDesignerTemplateParamDefinition TitleParam;
	TitleParam.Key = TEXT("title");
	TitleParam.Label = FText::FromString(TEXT("Title"));
	TitleParam.Type = EDesignerTemplateParamType::String;
	TitleParam.bRequired = true;
	TitleParam.DefaultValue = TEXT("Headline");
	Parameters.Add(TitleParam);

	FDesignerTemplateParamDefinition SubtitleParam;
	SubtitleParam.Key = TEXT("subtitle");
	SubtitleParam.Label = FText::FromString(TEXT("Subtitle"));
	SubtitleParam.Type = EDesignerTemplateParamType::String;
	SubtitleParam.DefaultValue = TEXT("Sub line");
	Parameters.Add(SubtitleParam);

	FDesignerTemplateParamDefinition ThemeColorParam;
	ThemeColorParam.Key = TEXT("themeColor");
	ThemeColorParam.Label = FText::FromString(TEXT("Theme Color"));
	ThemeColorParam.Type = EDesignerTemplateParamType::Color;
	ThemeColorParam.DefaultValue = TEXT("#FFAA00FF");
	Parameters.Add(ThemeColorParam);

	FDesignerTemplateParamDefinition LogoParam;
	LogoParam.Key = TEXT("logo");
	LogoParam.Label = FText::FromString(TEXT("Logo"));
	LogoParam.Type = EDesignerTemplateParamType::Image;
	Parameters.Add(LogoParam);

	FDesignerTemplateParamDefinition ShowLogoParam;
	ShowLogoParam.Key = TEXT("showLogo");
	ShowLogoParam.Label = FText::FromString(TEXT("Show Logo"));
	ShowLogoParam.Type = EDesignerTemplateParamType::Boolean;
	ShowLogoParam.DefaultValue = TEXT("true");
	Parameters.Add(ShowLogoParam);

	ApplyDefaults();
}

void ADesignerLowerThirdTemplateActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshPreviewWidget();
}

void ADesignerLowerThirdTemplateActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshPreviewWidget();
}

FString ADesignerLowerThirdTemplateActor::GetFieldValue(const FName FieldKey) const
{
	if (FieldKey == TEXT("title"))
	{
		return CurrentTitle;
	}

	if (FieldKey == TEXT("subtitle"))
	{
		return CurrentSubtitle;
	}

	if (FieldKey == TEXT("themeColor"))
	{
		return CurrentThemeColor.ToString();
	}

	if (FieldKey == TEXT("logo"))
	{
		return CurrentLogoPath;
	}

	if (FieldKey == TEXT("showLogo"))
	{
		return bShowLogo ? TEXT("true") : TEXT("false");
	}

	return FString();
}

void ADesignerLowerThirdTemplateActor::ApplyFields_Implementation(const TMap<FName, FString>& InFields)
{
	ApplyDefaults();

	if (const FString* TitleValue = InFields.Find(TEXT("title")))
	{
		CurrentTitle = *TitleValue;
	}

	if (const FString* SubtitleValue = InFields.Find(TEXT("subtitle")))
	{
		CurrentSubtitle = *SubtitleValue;
	}

	if (const FString* ColorValue = InFields.Find(TEXT("themeColor")))
	{
		TryApplyColor(*ColorValue);
	}

	if (const FString* LogoValue = InFields.Find(TEXT("logo")))
	{
		CurrentLogoPath = *LogoValue;
	}

	if (const FString* ShowLogoValue = InFields.Find(TEXT("showLogo")))
	{
		bShowLogo = ShowLogoValue->Equals(TEXT("true"), ESearchCase::IgnoreCase) || *ShowLogoValue == TEXT("1");
	}

	RefreshPreviewWidget();
	OnFieldsUpdated();
}

void ADesignerLowerThirdTemplateActor::TakeIn_Implementation()
{
	bIsOnAir = true;
	RefreshPreviewWidget();
	OnTakeInStarted();
}

void ADesignerLowerThirdTemplateActor::TakeOut_Implementation()
{
	bIsOnAir = false;
	RefreshPreviewWidget();
	OnTakeOutStarted();
}

void ADesignerLowerThirdTemplateActor::CollectCurrentFields_Implementation(TMap<FName, FString>& OutFields) const
{
	for (const FDesignerTemplateParamDefinition& Parameter : Parameters)
	{
		if (!Parameter.Key.IsNone())
		{
			OutFields.Add(Parameter.Key, GetFieldValue(Parameter.Key));
		}
	}
}

bool ADesignerLowerThirdTemplateActor::IsOnAir_Implementation() const
{
	return bIsOnAir;
}

bool ADesignerLowerThirdTemplateActor::TryApplyColor(const FString& InValue)
{
	FLinearColor ParsedColor;
	if (ParsedColor.InitFromString(InValue))
	{
		CurrentThemeColor = ParsedColor;
		return true;
	}

	if (FColor::FromHex(InValue).A != 0 || InValue.Equals(TEXT("#00000000")))
	{
		CurrentThemeColor = FLinearColor(FColor::FromHex(InValue));
		return true;
	}

	return false;
}

void ADesignerLowerThirdTemplateActor::ApplyDefaults()
{
	CurrentTitle = TEXT("Headline");
	CurrentSubtitle = TEXT("Sub line");
	CurrentThemeColor = FLinearColor(FColor::FromHex(TEXT("FFAA00FF")));
	CurrentLogoPath.Reset();
	bShowLogo = true;
}

void ADesignerLowerThirdTemplateActor::RefreshPreviewWidget()
{
	if (!PreviewWidgetComponent)
	{
		return;
	}

	PreviewWidgetComponent->InitWidget();
	if (UDesignerLowerThirdPreviewWidget* PreviewWidget = Cast<UDesignerLowerThirdPreviewWidget>(PreviewWidgetComponent->GetUserWidgetObject()))
	{
		PreviewWidget->SetPreviewData(CurrentTitle, CurrentSubtitle, CurrentLogoPath, CurrentThemeColor, bShowLogo, bIsOnAir);
	}
}
