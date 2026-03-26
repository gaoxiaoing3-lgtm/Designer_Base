#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DesignerBootstrapAssetsCommandlet.generated.h"

UCLASS()
class DESIGNER_BASE_API UDesignerBootstrapAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDesignerBootstrapAssetsCommandlet();

	virtual int32 Main(const FString& Params) override;
};
