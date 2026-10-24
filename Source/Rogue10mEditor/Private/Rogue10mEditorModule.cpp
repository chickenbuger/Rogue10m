#include "Modules/ModuleManager.h"

#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "MapMaker/SRogue10mMapMaker.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "Rogue10mEditor"

namespace
{
const FName MapMakerTabId(TEXT("Rogue10m.WorldMapMaker"));
}

class FRogue10mEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet())
		{
			return;
		}
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MapMakerTabId,
			FOnSpawnTab::CreateRaw(this, &FRogue10mEditorModule::SpawnMapMaker))
			.SetDisplayName(LOCTEXT("MapMakerTab", "World Map Maker"))
			.SetTooltipText(LOCTEXT("MapMakerTooltip", "Edit OpenRunMap with World Partition work areas."))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.WorldPartition"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this,
			&FRogue10mEditorModule::RegisterMenus));
		OpenCommand = MakeUnique<FAutoConsoleCommand>(TEXT("Rogue10m.MapMaker.Open"),
			TEXT("Open the editor-only World Map Maker panel. Does not load or save a map."),
			FConsoleCommandDelegate::CreateRaw(this, &FRogue10mEditorModule::OpenMapMaker));
	}

	virtual void ShutdownModule() override
	{
		OpenCommand.Reset();
		if (!IsRunningCommandlet())
		{
			UToolMenus::UnRegisterStartupCallback(this);
			UToolMenus::UnregisterOwner(this);
			if (TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(MapMakerTabId))
			{
				Tab->RequestCloseTab();
			}
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MapMakerTabId);
		}
	}

private:
	void OpenMapMaker()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(MapMakerTabId);
	}

	TSharedRef<SDockTab> SpawnMapMaker(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)
			[SNew(SRogue10mMapMaker)];
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
		FToolMenuSection& Section = WindowMenu->FindOrAddSection(TEXT("Rogue10m"), LOCTEXT("Rogue10mSection", "Rogue10m"));
		Section.AddMenuEntry(TEXT("Rogue10mWorldMapMaker"),
			LOCTEXT("OpenMapMaker", "World Map Maker"),
			LOCTEXT("OpenMapMakerDescription", "Open the OpenRunMap editing workspace."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.WorldPartition"),
			FUIAction(FExecuteAction::CreateRaw(this, &FRogue10mEditorModule::OpenMapMaker)));
	}

	TUniquePtr<FAutoConsoleCommand> OpenCommand;
};

IMPLEMENT_MODULE(FRogue10mEditorModule, Rogue10mEditor)

#undef LOCTEXT_NAMESPACE
