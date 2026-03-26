#include "DesignerControlGameMode.h"

#include "DesignerControlPlayerController.h"
#include "DesignerRuntimePreviewBootstrapper.h"
#include "Engine/World.h"

ADesignerControlGameMode::ADesignerControlGameMode()
{
	PlayerControllerClass = ADesignerControlPlayerController::StaticClass();
	PreviewBootstrapperClass = ADesignerRuntimePreviewBootstrapper::StaticClass();
}

void ADesignerControlGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!PreviewBootstrapperClass || !GetWorld() || IsValid(PreviewBootstrapper))
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewBootstrapper = GetWorld()->SpawnActor<ADesignerRuntimePreviewBootstrapper>(
		PreviewBootstrapperClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
}

void ADesignerControlGameMode::SetDesignerPlayerControllerClass(TSubclassOf<APlayerController> InPlayerControllerClass)
{
	PlayerControllerClass = InPlayerControllerClass;
}
