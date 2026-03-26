#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DesignerControlGameMode.generated.h"

class ADesignerRuntimePreviewBootstrapper;

UCLASS(Blueprintable)
class DESIGNER_BASE_API ADesignerControlGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADesignerControlGameMode();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	void SetDesignerPlayerControllerClass(TSubclassOf<APlayerController> InPlayerControllerClass);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Designer|Preview")
	TSubclassOf<ADesignerRuntimePreviewBootstrapper> PreviewBootstrapperClass;

	UPROPERTY(Transient)
	TObjectPtr<ADesignerRuntimePreviewBootstrapper> PreviewBootstrapper;
};
