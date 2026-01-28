#include "pch.h"
#include "framework.h"
#include "VatsimNAAATS.h"
#include "EuroScopePlugIn.h"
#include "NAAATS.h"
#include "WebServer.h"
#include <gdiplus.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace Gdiplus;

// Token for gdiplus
ULONG_PTR m_gdiplusToken = 0;

BEGIN_MESSAGE_MAP(CVatsimNAAATSApp, CWinApp)
END_MESSAGE_MAP()

CVatsimNAAATSApp::CVatsimNAAATSApp() {}

CVatsimNAAATSApp theApp;
EuroScopePlugIn::CPlugIn* pNAAATS = nullptr;

BOOL CVatsimNAAATSApp::InitInstance()
{
	CWinApp::InitInstance();

	return TRUE;
}

// Plugin Initialisation
void __declspec (dllexport)
EuroScopePlugInInit(EuroScopePlugIn::CPlugIn** ppPlugInInstance)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	// Initialize GDI+
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);
	* ppPlugInInstance = (CPlugIn*)new CNAAATSPlugin();

	// Get DLL path
	GetModuleFileNameA(HINSTANCE(&__ImageBase), CUtils::DllPathFile, sizeof(CUtils::DllPathFile));
	CUtils::DllPath = CUtils::DllPathFile;
	CUtils::DllPath.resize(CUtils::DllPath.size() - strlen("VatsimNAAATS.dll"));

	// Instantiate logger as the very first thing we do
	CLogger::InstantiateLogFile();

	// Set crash handler
	SetUnhandledExceptionFilter(CLogger::UnhandledExceptionHandler);

	// Start Virtual Server (Editable FDD)
	CWebServer::Start(8080);
}

// Plugin exit
void __declspec (dllexport)
EuroScopePlugInExit(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	
	// Stop Virtual Server
	CWebServer::Stop();

	GdiplusShutdown(m_gdiplusToken);
	delete pNAAATS;
}