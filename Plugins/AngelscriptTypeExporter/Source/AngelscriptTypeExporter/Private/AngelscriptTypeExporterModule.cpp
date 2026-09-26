#include "AngelscriptTypeExporterModule.h"
#include "TypeDeclarationExporter.h"

#include "ToolMenus.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "AngelscriptTypeExporter"

void FAngelscriptTypeExporterModule::StartupModule()
{
	if (!IsRunningCommandlet())
	{
		UToolMenus::RegisterStartupCallback(FSimpleDelegate::CreateRaw(this, &FAngelscriptTypeExporterModule::RegisterMenus));
	}
}

void FAngelscriptTypeExporterModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);

	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::UnregisterOwner(this);
	}
}

void FAngelscriptTypeExporterModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->FindOrAddSection("AngelscriptTypeExporter");

	Section.AddMenuEntry(
		"AngelscriptTypeExporter.ExportDeclarations",
		LOCTEXT("ExportTypeDeclarations", "Export Angelscript Type Declarations (.d.as)"),
		LOCTEXT("ExportTypeDeclarationsTooltip", "Export all C++ bound Angelscript types to Saved/AS-Cache/*.d.as for offline use by my-angel-script-lsp."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FAngelscriptTypeExporterModule::ExecuteExport)));
}

void FAngelscriptTypeExporterModule::ExecuteExport()
{
	const FTypeExportResult Result = FTypeDeclarationExporter::ExportAll();

	FNotificationInfo Info(FText::FromString(Result.Summary));
	Info.bUseSuccessFailIcons = true;
	Info.ExpireDuration = 8.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAngelscriptTypeExporterModule, AngelscriptTypeExporter)
