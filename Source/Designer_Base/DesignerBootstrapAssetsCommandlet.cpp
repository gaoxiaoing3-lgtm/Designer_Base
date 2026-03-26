#include "DesignerBootstrapAssetsCommandlet.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/UserWidget.h"
#include "Misc/ConfigCacheIni.h"
#include "DesignerControlGameMode.h"
#include "DesignerControlPlayerController.h"
#include "DesignerRuntimeConsoleWidget.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

namespace
{
	template <typename TAssetType, typename TFactoryType>
	TAssetType* CreateOrLoadBlueprintAsset(const FString& PackagePath, const FString& AssetName, TFactoryType* Factory)
	{
		const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
		if (TAssetType* ExistingAsset = LoadObject<TAssetType>(nullptr, *ObjectPath))
		{
			return ExistingAsset;
		}

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		return Cast<TAssetType>(AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, TAssetType::StaticClass(), Factory));
	}

	bool SaveAssetPackage(UObject* Asset)
	{
		if (!Asset)
		{
			return false;
		}

		UPackage* Package = Asset->GetOutermost();
		if (!Package)
		{
			return false;
		}

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;

		const FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
	}
}

#endif

UDesignerBootstrapAssetsCommandlet::UDesignerBootstrapAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UDesignerBootstrapAssetsCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	return 1;
#else
	const FString PackagePath = TEXT("/Game/DesignerGenerated");
	const FString WidgetAssetName = TEXT("WBP_DesignerRuntimeConsole");
	const FString PlayerControllerAssetName = TEXT("BP_DesignerControlPlayerController");
	const FString GameModeAssetName = TEXT("BP_DesignerControlGameMode");

	UWidgetBlueprintFactory* WidgetFactory = NewObject<UWidgetBlueprintFactory>();
	WidgetFactory->ParentClass = UDesignerRuntimeConsoleWidget::StaticClass();

	UWidgetBlueprint* WidgetBlueprint = CreateOrLoadBlueprintAsset<UWidgetBlueprint>(PackagePath, WidgetAssetName, WidgetFactory);
	if (!WidgetBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create widget blueprint asset."));
		return 2;
	}
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	UBlueprintFactory* PlayerControllerFactory = NewObject<UBlueprintFactory>();
	PlayerControllerFactory->ParentClass = ADesignerControlPlayerController::StaticClass();
	UBlueprint* PlayerControllerBlueprint = CreateOrLoadBlueprintAsset<UBlueprint>(PackagePath, PlayerControllerAssetName, PlayerControllerFactory);
	if (!PlayerControllerBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create player controller blueprint asset."));
		return 3;
	}
	FKismetEditorUtilities::CompileBlueprint(PlayerControllerBlueprint);

	if (ADesignerControlPlayerController* PlayerControllerCDO = Cast<ADesignerControlPlayerController>(PlayerControllerBlueprint->GeneratedClass->GetDefaultObject()))
	{
		PlayerControllerCDO->SetControlPanelWidgetClass(TSubclassOf<UUserWidget>(WidgetBlueprint->GeneratedClass));
		PlayerControllerBlueprint->Modify();
		FKismetEditorUtilities::CompileBlueprint(PlayerControllerBlueprint);
	}

	UBlueprintFactory* GameModeFactory = NewObject<UBlueprintFactory>();
	GameModeFactory->ParentClass = ADesignerControlGameMode::StaticClass();
	UBlueprint* GameModeBlueprint = CreateOrLoadBlueprintAsset<UBlueprint>(PackagePath, GameModeAssetName, GameModeFactory);
	if (!GameModeBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create game mode blueprint asset."));
		return 4;
	}
	FKismetEditorUtilities::CompileBlueprint(GameModeBlueprint);

	if (ADesignerControlGameMode* GameModeCDO = Cast<ADesignerControlGameMode>(GameModeBlueprint->GeneratedClass->GetDefaultObject()))
	{
		GameModeCDO->SetDesignerPlayerControllerClass(TSubclassOf<APlayerController>(PlayerControllerBlueprint->GeneratedClass));
		GameModeBlueprint->Modify();
		FKismetEditorUtilities::CompileBlueprint(GameModeBlueprint);
	}

	WidgetBlueprint->Modify();
	PlayerControllerBlueprint->Modify();
	GameModeBlueprint->Modify();

	SaveAssetPackage(WidgetBlueprint);
	SaveAssetPackage(PlayerControllerBlueprint);
	SaveAssetPackage(GameModeBlueprint);

	GConfig->SetString(
		TEXT("/Script/EngineSettings.GameMapsSettings"),
		TEXT("GlobalDefaultGameMode"),
		TEXT("/Game/DesignerGenerated/BP_DesignerControlGameMode.BP_DesignerControlGameMode_C"),
		GEngineIni);
	GConfig->SetString(
		TEXT("/Script/EngineSettings.GameMapsSettings"),
		TEXT("GlobalDefaultServerGameMode"),
		TEXT("/Game/DesignerGenerated/BP_DesignerControlGameMode.BP_DesignerControlGameMode_C"),
		GEngineIni);
	GConfig->Flush(false, GEngineIni);

	UE_LOG(LogTemp, Display, TEXT("Generated test assets in %s"), *PackagePath);
	return 0;
#endif
}
