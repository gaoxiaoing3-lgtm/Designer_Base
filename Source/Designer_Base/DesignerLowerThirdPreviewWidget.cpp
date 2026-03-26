#include "DesignerLowerThirdPreviewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"

namespace
{
	FSlateFontInfo MakePreviewFontInfo(const int32 Size)
	{
		return FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"), Size);
	}
}

void UDesignerLowerThirdPreviewWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BuildLayout();
	RefreshView();
}

void UDesignerLowerThirdPreviewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBroadcastMode)
	{
		return;
	}

	const float TargetProgress = bIsOnAir ? 1.0f : 0.0f;
	AnimationProgress = FMath::FInterpConstantTo(AnimationProgress, TargetProgress, InDeltaTime, 1.0f / FMath::Max(AnimationDuration, KINDA_SMALL_NUMBER));
	AnimationProgress = FMath::Clamp(AnimationProgress, 0.0f, 1.0f);
	RefreshAnimatedState();
}

void UDesignerLowerThirdPreviewWidget::SetPreviewData(
	const FString& InTitle,
	const FString& InSubtitle,
	const FString& InLogoPath,
	const FLinearColor& InThemeColor,
	bool bInShowLogo,
	bool bInIsOnAir)
{
	CurrentTitle = InTitle;
	CurrentSubtitle = InSubtitle;
	CurrentLogoPath = InLogoPath;
	CurrentThemeColor = InThemeColor;
	bShowLogo = bInShowLogo;
	bIsOnAir = bInIsOnAir;
	if (!bBroadcastMode)
	{
		AnimationProgress = 1.0f;
	}
	RefreshView();
}

void UDesignerLowerThirdPreviewWidget::SetBroadcastMode(bool bInBroadcastMode)
{
	bBroadcastMode = bInBroadcastMode;
	AnimationProgress = bBroadcastMode ? (bIsOnAir ? 1.0f : 0.0f) : 1.0f;
	RefreshAnimatedState();
}

void UDesignerLowerThirdPreviewWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RootBorder"));
	WidgetTree->RootWidget = RootBorder;
	RootBorder->SetPadding(FMargin(18.0f, 12.0f));

	MainRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MainRow"));
	RootBorder->SetContent(MainRow);

	LogoBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LogoBorder"));
	LogoBorder->SetPadding(FMargin(0.0f));
	if (UHorizontalBoxSlot* LogoSlot = MainRow->AddChildToHorizontalBox(LogoBorder))
	{
		LogoSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
	}

	USizeBox* LogoSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LogoSizeBox"));
	LogoSizeBox->SetWidthOverride(88.0f);
	LogoSizeBox->SetHeightOverride(88.0f);
	LogoBorder->SetContent(LogoSizeBox);

	LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LogoImage"));
	LogoSizeBox->SetContent(LogoImage);

	UVerticalBox* TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TextColumn"));
	MainRow->AddChildToHorizontalBox(TextColumn);

	AccentBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AccentBorder"));
	if (UVerticalBoxSlot* AccentSlot = TextColumn->AddChildToVerticalBox(AccentBorder))
	{
		AccentSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	USizeBox* AccentSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AccentSizeBox"));
	AccentSizeBox->SetWidthOverride(320.0f);
	AccentSizeBox->SetHeightOverride(8.0f);
	AccentBorder->SetContent(AccentSizeBox);

	TitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleTextBlock"));
	TitleTextBlock->SetFont(MakePreviewFontInfo(34));
	if (UVerticalBoxSlot* TitleSlot = TextColumn->AddChildToVerticalBox(TitleTextBlock))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	SubtitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SubtitleTextBlock"));
	SubtitleTextBlock->SetFont(MakePreviewFontInfo(24));
	TextColumn->AddChildToVerticalBox(SubtitleTextBlock);
}

void UDesignerLowerThirdPreviewWidget::RefreshView()
{
	if (!RootBorder)
	{
		return;
	}

	FSlateBrush RootBrush;
	RootBrush.TintColor = FSlateColor(FLinearColor(0.02f, 0.02f, 0.02f, bIsOnAir ? 0.86f : 0.66f));
	RootBorder->SetBrush(RootBrush);

	FSlateBrush AccentBrush;
	AccentBrush.TintColor = FSlateColor(CurrentThemeColor);
	AccentBorder->SetBrush(AccentBrush);

	FSlateBrush LogoBrush;
	LogoBrush.TintColor = FSlateColor(FLinearColor(0.94f, 0.94f, 0.94f, 1.0f));
	LogoBorder->SetBrush(LogoBrush);
	LogoBorder->SetVisibility(bShowLogo ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RefreshLogoImage();

	TitleTextBlock->SetText(FText::FromString(CurrentTitle));
	TitleTextBlock->SetColorAndOpacity(FSlateColor(CurrentThemeColor));

	SubtitleTextBlock->SetText(FText::FromString(CurrentSubtitle));
	SubtitleTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.92f, 1.0f)));
	RefreshAnimatedState();
}

void UDesignerLowerThirdPreviewWidget::RefreshLogoImage()
{
	if (!LogoImage)
	{
		return;
	}

	if (!bShowLogo)
	{
		LogoImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (CurrentLogoPath.IsEmpty())
	{
		FSlateBrush PlaceholderBrush;
		PlaceholderBrush.TintColor = FSlateColor(FLinearColor(0.94f, 0.94f, 0.94f, 1.0f));
		LogoImage->SetBrush(PlaceholderBrush);
		LogoImage->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	if (LoadedLogoPath != CurrentLogoPath)
	{
		LoadedLogoTexture = nullptr;
		LoadedLogoPath = CurrentLogoPath;

		if (FPaths::FileExists(CurrentLogoPath))
		{
			LoadedLogoTexture = FImageUtils::ImportFileAsTexture2D(CurrentLogoPath);
		}
	}

	if (LoadedLogoTexture)
	{
		LogoImage->SetBrushFromTexture(LoadedLogoTexture, true);
		LogoImage->SetColorAndOpacity(FLinearColor::White);
		LogoImage->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		FSlateBrush FallbackBrush;
		FallbackBrush.TintColor = FSlateColor(FLinearColor(0.55f, 0.12f, 0.12f, 1.0f));
		LogoImage->SetBrush(FallbackBrush);
		LogoImage->SetVisibility(ESlateVisibility::Visible);
	}
}

void UDesignerLowerThirdPreviewWidget::RefreshAnimatedState()
{
	if (!RootBorder)
	{
		return;
	}

	if (!bBroadcastMode)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RootBorder->SetRenderOpacity(1.0f);
		RootBorder->SetRenderTranslation(FVector2D::ZeroVector);
		return;
	}

	const float EasedProgress = FMath::InterpEaseOut(0.0f, 1.0f, AnimationProgress, 2.0f);
	RootBorder->SetRenderOpacity(EasedProgress);
	RootBorder->SetRenderTranslation(FVector2D(-120.0f * (1.0f - EasedProgress), 24.0f * (1.0f - EasedProgress)));

	if (AnimationProgress <= KINDA_SMALL_NUMBER && !bIsOnAir)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}
