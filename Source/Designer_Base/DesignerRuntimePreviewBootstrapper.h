#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DesignerRuntimePreviewBootstrapper.generated.h"

class ADesignerTemplateActor;

UCLASS()
class DESIGNER_BASE_API ADesignerRuntimePreviewBootstrapper : public AActor
{
	GENERATED_BODY()

public:
	ADesignerRuntimePreviewBootstrapper();

	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Designer|Preview")
	bool bHideExistingTemplateActors = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Designer|Preview")
	FVector SpawnLocation = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Designer|Preview")
	FRotator SpawnRotation = FRotator::ZeroRotator;

private:
	void HideExistingTemplateActors() const;
	void SpawnAndActivateRuntimeTemplate();

	UPROPERTY(Transient)
	TObjectPtr<ADesignerTemplateActor> SpawnedTemplateActor;
};
