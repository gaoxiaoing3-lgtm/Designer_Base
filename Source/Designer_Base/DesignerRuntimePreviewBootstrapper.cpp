#include "DesignerRuntimePreviewBootstrapper.h"

#include "DesignerLowerThirdTemplateActor.h"
#include "DesignerTemplateRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ADesignerRuntimePreviewBootstrapper::ADesignerRuntimePreviewBootstrapper()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADesignerRuntimePreviewBootstrapper::BeginPlay()
{
	Super::BeginPlay();

	if (bHideExistingTemplateActors)
	{
		HideExistingTemplateActors();
	}

	SpawnAndActivateRuntimeTemplate();
}

void ADesignerRuntimePreviewBootstrapper::HideExistingTemplateActors() const
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<ADesignerTemplateActor> It(GetWorld()); It; ++It)
	{
		ADesignerTemplateActor* TemplateActor = *It;
		if (!IsValid(TemplateActor) || TemplateActor == SpawnedTemplateActor)
		{
			continue;
		}

		TemplateActor->SetActorHiddenInGame(true);
		TemplateActor->SetActorEnableCollision(false);
		TemplateActor->SetActorTickEnabled(false);
	}
}

void ADesignerRuntimePreviewBootstrapper::SpawnAndActivateRuntimeTemplate()
{
	if (!GetWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SpawnedTemplateActor = GetWorld()->SpawnActor<ADesignerLowerThirdTemplateActor>(
		ADesignerLowerThirdTemplateActor::StaticClass(),
		SpawnLocation,
		SpawnRotation,
		SpawnParameters);

	if (!IsValid(SpawnedTemplateActor))
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDesignerTemplateRegistrySubsystem* Registry = GameInstance->GetSubsystem<UDesignerTemplateRegistrySubsystem>())
		{
			Registry->ActivateTemplateActor(SpawnedTemplateActor, true);
		}
	}
}
