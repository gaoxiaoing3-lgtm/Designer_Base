#include "DesignerTemplateRegistrySubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DesignerTemplateActor.h"
#include "Engine/Blueprint.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

void UDesignerTemplateRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RefreshTemplateCatalog();
}

void UDesignerTemplateRegistrySubsystem::Deinitialize()
{
	ClearActiveTemplate();
	CachedTemplates.Reset();

	Super::Deinitialize();
}

void UDesignerTemplateRegistrySubsystem::RefreshTemplateCatalog()
{
	TArray<UClass*> DiscoveredClasses;
	DiscoverBlueprintTemplateClasses(DiscoveredClasses);
	DiscoverNativeTemplateClasses(DiscoveredClasses);

	CachedTemplates.Reset();

	TSet<FString> SeenCodes;
	for (UClass* TemplateClass : DiscoveredClasses)
	{
		TryAddTemplateClass(TemplateClass, SeenCodes);
	}

	CachedTemplates.Sort([](const FDesignerTemplateDescriptor& A, const FDesignerTemplateDescriptor& B)
	{
		return A.TemplateName.ToString() < B.TemplateName.ToString();
	});

	OnTemplateCatalogChanged.Broadcast();
}

TArray<FDesignerTemplateDescriptor> UDesignerTemplateRegistrySubsystem::GetAvailableTemplates() const
{
	return CachedTemplates;
}

bool UDesignerTemplateRegistrySubsystem::FindTemplateByCode(const FString& TemplateCode, FDesignerTemplateDescriptor& OutDescriptor) const
{
	for (const FDesignerTemplateDescriptor& Descriptor : CachedTemplates)
	{
		if (Descriptor.TemplateCode.Equals(TemplateCode, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}

	return false;
}

bool UDesignerTemplateRegistrySubsystem::ActivateTemplateByCode(const FString& TemplateCode)
{
	FDesignerTemplateDescriptor Descriptor;
	if (!FindTemplateByCode(TemplateCode, Descriptor))
	{
		return false;
	}

	return ActivateTemplateByClassPath(Descriptor.ControllerClassPath);
}

bool UDesignerTemplateRegistrySubsystem::ActivateTemplateByClassPath(const FSoftClassPath& TemplateClassPath)
{
	UClass* TemplateClass = TemplateClassPath.TryLoadClass<ADesignerTemplateActor>();
	return ActivateTemplateClass(TemplateClass);
}

bool UDesignerTemplateRegistrySubsystem::ActivateTemplateActor(ADesignerTemplateActor* InTemplateActor, bool bTakeOwnership)
{
	if (!IsValid(InTemplateActor))
	{
		return false;
	}

	ClearActiveTemplate();
	ActiveTemplateActor = InTemplateActor;
	bOwnsActiveTemplateActor = bTakeOwnership;
	OnActiveTemplateChanged.Broadcast(ActiveTemplateActor);
	return true;
}

void UDesignerTemplateRegistrySubsystem::ApplyFieldsToActiveTemplate(const TMap<FName, FString>& InFields)
{
	if (ActiveTemplateActor)
	{
		ActiveTemplateActor->ApplyFields(InFields);
	}
}

void UDesignerTemplateRegistrySubsystem::TakeActiveTemplateIn()
{
	if (ActiveTemplateActor)
	{
		ActiveTemplateActor->TakeIn();
	}
}

void UDesignerTemplateRegistrySubsystem::TakeActiveTemplateOut()
{
	if (ActiveTemplateActor)
	{
		ActiveTemplateActor->TakeOut();
	}
}

void UDesignerTemplateRegistrySubsystem::ClearActiveTemplate()
{
	if (IsValid(ActiveTemplateActor))
	{
		if (bOwnsActiveTemplateActor)
		{
			ActiveTemplateActor->Destroy();
		}

		ActiveTemplateActor = nullptr;
		bOwnsActiveTemplateActor = false;
		OnActiveTemplateChanged.Broadcast(nullptr);
	}
}

ADesignerTemplateActor* UDesignerTemplateRegistrySubsystem::GetActiveTemplateActor() const
{
	return ActiveTemplateActor;
}

bool UDesignerTemplateRegistrySubsystem::GetActiveTemplateDescriptor(FDesignerTemplateDescriptor& OutDescriptor) const
{
	if (!ActiveTemplateActor)
	{
		return false;
	}

	OutDescriptor = BuildDescriptorFromClass(ActiveTemplateActor->GetClass());
	return true;
}

void UDesignerTemplateRegistrySubsystem::DiscoverNativeTemplateClasses(TArray<UClass*>& OutClasses) const
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* CurrentClass = *It;
		if (!IsValid(CurrentClass))
		{
			continue;
		}

		if (!CurrentClass->IsChildOf(ADesignerTemplateActor::StaticClass()) || CurrentClass == ADesignerTemplateActor::StaticClass())
		{
			continue;
		}

		if (CurrentClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		if (CurrentClass->ClassGeneratedBy != nullptr)
		{
			continue;
		}

		OutClasses.Add(CurrentClass);
	}
}

void UDesignerTemplateRegistrySubsystem::DiscoverBlueprintTemplateClasses(TArray<UClass*>& OutClasses) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(TEXT("/Game"));

	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.GetAssets(Filter, BlueprintAssets);

	for (const FAssetData& Asset : BlueprintAssets)
	{
		FString GeneratedClassExportPath;
		if (!Asset.GetTagValue(TEXT("GeneratedClass"), GeneratedClassExportPath))
		{
			continue;
		}

		const FString GeneratedClassObjectPath = FPackageName::ExportTextPathToObjectPath(GeneratedClassExportPath);
		UClass* LoadedClass = FSoftClassPath(GeneratedClassObjectPath).TryLoadClass<ADesignerTemplateActor>();
		if (!LoadedClass || !LoadedClass->IsChildOf(ADesignerTemplateActor::StaticClass()))
		{
			continue;
		}

		if (LoadedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		OutClasses.AddUnique(LoadedClass);
	}
}

bool UDesignerTemplateRegistrySubsystem::TryAddTemplateClass(UClass* TemplateClass, TSet<FString>& SeenCodes)
{
	if (!TemplateClass)
	{
		return false;
	}

	const FDesignerTemplateDescriptor Descriptor = BuildDescriptorFromClass(TemplateClass);
	if (Descriptor.TemplateCode.IsEmpty() || SeenCodes.Contains(Descriptor.TemplateCode))
	{
		return false;
	}

	SeenCodes.Add(Descriptor.TemplateCode);
	CachedTemplates.Add(Descriptor);
	return true;
}

FDesignerTemplateDescriptor UDesignerTemplateRegistrySubsystem::BuildDescriptorFromClass(const UClass* TemplateClass) const
{
	FDesignerTemplateDescriptor Descriptor;
	if (!TemplateClass)
	{
		return Descriptor;
	}

	const ADesignerTemplateActor* TemplateDefaults = Cast<ADesignerTemplateActor>(TemplateClass->GetDefaultObject());
	if (!TemplateDefaults)
	{
		return Descriptor;
	}

	Descriptor.TemplateCode = TemplateDefaults->TemplateCode;
	Descriptor.TemplateName = TemplateDefaults->TemplateName;
	Descriptor.Category = TemplateDefaults->Category;
	Descriptor.SupportedActions = TemplateDefaults->SupportedActions;
	Descriptor.Parameters = TemplateDefaults->Parameters;
	Descriptor.ControllerClassPath = FSoftClassPath(TemplateClass);
	return Descriptor;
}

UWorld* UDesignerTemplateRegistrySubsystem::ResolveSpawnWorld() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetWorld();
	}

	return nullptr;
}

bool UDesignerTemplateRegistrySubsystem::ActivateTemplateClass(UClass* TemplateClass)
{
	if (!TemplateClass || !TemplateClass->IsChildOf(ADesignerTemplateActor::StaticClass()))
	{
		return false;
	}

	UWorld* World = ResolveSpawnWorld();
	if (!World)
	{
		return false;
	}

	if (IsValid(ActiveTemplateActor) && ActiveTemplateActor->IsA(TemplateClass))
	{
		bOwnsActiveTemplateActor = false;
		OnActiveTemplateChanged.Broadcast(ActiveTemplateActor);
		return true;
	}

	if (ADesignerTemplateActor* ExistingActor = FindExistingTemplateActor(World, TemplateClass))
	{
		ClearActiveTemplate();
		ActiveTemplateActor = ExistingActor;
		bOwnsActiveTemplateActor = false;
		OnActiveTemplateChanged.Broadcast(ActiveTemplateActor);
		return true;
	}

	ClearActiveTemplate();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(World, TemplateClass);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveTemplateActor = World->SpawnActor<ADesignerTemplateActor>(TemplateClass, FTransform::Identity, SpawnParameters);
	bOwnsActiveTemplateActor = IsValid(ActiveTemplateActor);
	OnActiveTemplateChanged.Broadcast(ActiveTemplateActor);

	return IsValid(ActiveTemplateActor);
}

ADesignerTemplateActor* UDesignerTemplateRegistrySubsystem::FindExistingTemplateActor(UWorld* World, UClass* TemplateClass) const
{
	if (!World || !TemplateClass)
	{
		return nullptr;
	}

	for (TActorIterator<ADesignerTemplateActor> It(World); It; ++It)
	{
		ADesignerTemplateActor* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->IsA(TemplateClass))
		{
			return Candidate;
		}
	}

	return nullptr;
}
