#include "StdAfx.h"
#include "PythonApplication.h"
#include "Platform/PlatformWebView.h"

// ZiiNAN: Platform abstraction
bool CPythonApplication::IsWebPageMode() { return Platform::WebView::IsVisible(); }

void CPythonApplication::ShowWebPage(const char* url, const RECT& bounds)
{
    SetCursorMode(CURSOR_MODE_HARDWARE);
    Platform::WebView::Show(GetNativeHandle(), url,
        {int(bounds.left), int(bounds.top), int(bounds.right), int(bounds.bottom)},
        [](long result) { TraceError("failed to get controller for webview, result: 0x%X", result); });
}

void CPythonApplication::MoveWebPage(const RECT& bounds)
{
    Platform::WebView::Move({int(bounds.left), int(bounds.top), int(bounds.right), int(bounds.bottom)});
}

void CPythonApplication::HideWebPage()
{
    Platform::WebView::Hide();
    SetCursorMode(m_pySystem.IsSoftwareCursor() ? CURSOR_MODE_SOFTWARE : CURSOR_MODE_HARDWARE);
}
