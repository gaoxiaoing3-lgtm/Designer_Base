#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TemplateAuthoringTypes.h"
#include "DesignerTemplateRegistrySubsystem.generated.h"

class ADesignerTemplateActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDesignerTemplateCatalogChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDesignerActiveTemplateChangedSignature, ADesignerTemplateActor*, ActiveTemplateActor);

UCLASS()
class DESIGNER_BASE_API UDesignerTemplateRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Designer|Templates")
	FDesignerTemplateCatalogChangedSignature OnTemplateCatalogChanged;

	UPROPERTY(BlueprintAssignable, Category = "Designer|Templates")
	FDesignerActiveTemplateChangedSignature OnActiveTemplateChanged;

	UFUNCTION(BlueprintCallable, Category = "Designer|Templates")
	void RefreshTemplateCatalog();

	UFUNCTION(BlueprintPure, Category = "Designer|Templates")
	TArray<FDesignerTemplateDescriptor> GetAvailableTemplates() const;

	UFUNCTION(BlueprintPure, Category = "Designer|Templates")
	bool FindTemplateByCode(const FString& TemplateCode, FDesignerTemplateDescriptor& OutDescriptor) const;

	UFUNCTION(BlueprintCallable, Category = "Designer|Templates")
	bool ActivateTemplateByCode(const FString& TemplateCode);

	UFUNCTION(BlueprintCallable, Category = "Designer|Templates")
	bool ActivateTemplateByClassPath(const FSoftClassPath& TemplateClassPath);

	UFUNCTION(BlueprintCallable, Category = "Designer|Templates")
	bool ActivateTemplateActor(ADesignerTemplateActor* InTemplateActor, bool bTakeOwnership = false);

	UFUNCTION(BlueprintCallable, Category = "Designer|Templates")
	void ApplyFieldsToActiveTemplate(const TMap<FName, FString>& InFields);

	UFUNCTION(BlueprintCallable, Category = "Designer|Templates")
	void TakeActiveTemplateIn();

	UFUNCTION(BlueprintCallable, Category = "Designer|Templates")
	void TakeActiveTemplateOut();

	UFUNCTION(BlueprintCallable, Category = "Designer|Templates")
	void ClearActiveTemplate();

	UFUNCTION(BlueprintPure, Category = "Designer|Templates")
	ADesignerTemplateActor* GetActiveTemplateActor() const;

	UFUNCTION(BlueprintPure, Category = "Designer|Templates")
	bool GetActiveTemplateDescriptor(FDesignerTemplateDescriptor& OutDescriptor) const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<ADesignerTemplateActor> ActiveTemplateActor;

	UPROPERTY(Transient)
	bool bOwnsActiveTemplateActor = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Designer|Templates")
	TArray<FDesignerTemplateDescriptor> CachedTemplates;

private:
	void DiscoverNativeTemplateClasses(TArray<UClass*>& OutClasses) const;
	void DiscoverBlueprintTemplateClasses(TArray<UClass*>& OutClasses) const;
	bool TryAddTemplateClass(UClass* TemplateClass, TSet<FString>& SeenCodes);
	FDesignerTemplateDescriptor BuildDescriptorFromClass(const UClass* TemplateClass) const;
	UWorld* ResolveSpawnWorld() const;
	bool ActivateTemplateClass(UClass* TemplateClass);
	ADesignerTemplateActor* FindExistingTemplateActor(UWorld* World, UClass* TemplateClass) const;
};
