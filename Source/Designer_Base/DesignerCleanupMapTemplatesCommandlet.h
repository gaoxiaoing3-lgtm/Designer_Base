#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DesignerCleanupMapTemplatesCommandlet.generated.h"

UCLASS()
class DESIGNER_BASE_API UDesignerCleanupMapTemplatesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDesignerCleanupMapTemplatesCommandlet();

	virtual int32 Main(const FString& Params) override;

private:
	bool ShouldRemoveActor(const class AActor* Actor) const;
};
