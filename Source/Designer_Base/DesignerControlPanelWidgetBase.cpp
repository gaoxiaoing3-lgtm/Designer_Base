#include "DesignerControlPanelWidgetBase.h"

#include "DesignerControlPanelViewModel.h"

void UDesignerControlPanelWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!ViewModel)
	{
		ViewModel = UDesignerControlPanelViewModel::CreateDesignerControlPanelViewModel(this);
	}

	OnViewModelReady(ViewModel);
}

UDesignerControlPanelViewModel* UDesignerControlPanelWidgetBase::GetViewModel() const
{
	return ViewModel;
}
