#include "Platform/PlatformWebView.h"
#include <windows.h>

#undef C8
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include <filesystem>
#include <locale>
#include <codecvt>

static wil::com_ptr<ICoreWebView2> gs_webView;
static wil::com_ptr<ICoreWebView2Controller> gs_webViewController;

using convert_type = std::codecvt_utf8<wchar_t>;
static std::wstring_convert<convert_type, wchar_t> converter;

bool Platform::WebView::IsVisible()
{
	return gs_webViewController.get();
}

void Platform::WebView::Show(NativeWindowHandle parent, const char* c_szURL, const Rect& bounds, ErrorHandler onError)
{
	const RECT c_rcWebPage{bounds.left, bounds.top, bounds.right, bounds.bottom};

	std::filesystem::path tempPath = std::filesystem::temp_directory_path();
	tempPath /= "Metin2";
	tempPath /= "WebCache";

	auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
	{
		Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions6> options6;
		if (options.As(&options6) == S_OK) {
			options6->put_AreBrowserExtensionsEnabled(FALSE);
		}
	}

	std::wstring stURL = converter.from_bytes(c_szURL);
	CreateCoreWebView2EnvironmentWithOptions(nullptr, tempPath.c_str(), options.Get(),
		Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[handle = static_cast<HWND>(parent.value), stURL, myRect = c_rcWebPage, onError](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
		if (FAILED(result) || !env || !IsWindow(handle)) {
			if (onError) onError(result);
			return FAILED(result) ? result : E_FAIL;
		}

		// Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
		env->CreateCoreWebView2Controller(handle, Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
			[stURL, myRect, onError](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
			if (controller != nullptr) {
				gs_webViewController = controller;
				gs_webViewController->get_CoreWebView2(&gs_webView);
			}
			if (!gs_webView) {
				if (onError) onError(result);
				return -1;
			}

			// Add a few settings for the webview
			// The demo step is redundant since the values are the default settings
#ifdef _LIVE
			wil::com_ptr<ICoreWebView2Settings> baseSettings;
			gs_webView->get_Settings(&baseSettings);

			auto settings = baseSettings.try_query< ICoreWebView2Settings3>();
			if (settings) {
				settings->put_AreDevToolsEnabled(FALSE);
				settings->put_AreDefaultContextMenusEnabled(FALSE);
				settings->put_AreBrowserAcceleratorKeysEnabled(FALSE);
				settings->put_IsZoomControlEnabled(FALSE);
			}
#endif

			// Resize WebView to fit the bounds of the parent window
			gs_webViewController->put_Bounds(myRect);

			// Schedule an async task to navigate to Bing
			gs_webView->Navigate(stURL.c_str());

			// <NavigationEvents>
			// Step 4 - Navigation events
			// register an ICoreWebView2NavigationStartingEventHandler to cancel any non-https navigation
			EventRegistrationToken token;
			gs_webView->add_NavigationStarting(Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
				[](ICoreWebView2* gs_webView, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
				wil::unique_cotaskmem_string uri;
				args->get_Uri(&uri);
				std::wstring source(uri.get());
				if (source.substr(0, 5) != L"https") {
					args->put_Cancel(TRUE);
				}
				return S_OK;
			}).Get(), &token);
			// </NavigationEvents>

			return S_OK;
		}).Get());
		return S_OK;
	}).Get());
}

void Platform::WebView::Move(const Rect& bounds)
{
	const RECT c_rcWebPage{bounds.left, bounds.top, bounds.right, bounds.bottom};
	if (gs_webViewController)
		gs_webViewController->put_Bounds(c_rcWebPage);
}

void Platform::WebView::Hide()
{
	if (gs_webView)
		gs_webView.reset();

	if (gs_webViewController)
		gs_webViewController.reset();

}
