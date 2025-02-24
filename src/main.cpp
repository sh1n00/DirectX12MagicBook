#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#ifdef _DEBUG
#include <iostream>
#endif // _DEBUG

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace std;

namespace {
const unsigned int window_width = 1280;
const unsigned int window_height = 720;
ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapChain = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
IDXGISwapChain4* _swapchain = nullptr;
}

// @brief コンソール画面にフォーマット付き文字列を表示
// @param formatフォーマット（%dとか%fとかの）
// @param 可変長引数
// @remarks この関数はデバッグ用です。デバッグ時にしか動作しません
void DebugOutputFromString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif
}

// お決まりの関数
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// ウィンドウ破棄で呼ばれる
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0); // OSにアプリ終了を伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // 既定の処理を行う
}

void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	debugLayer->EnableDebugLayer();
	debugLayer->Release();
}

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
	EnableDebugLayer();

	// ウィンドウクラスの生成/登録
	WNDCLASSEX w = {};

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = static_cast<WNDPROC>(WindowProcedure); // コールバック関数の指定
	w.lpszClassName = L"DX12Sample";
	w.hInstance = GetModuleHandle(nullptr); // ハンドル取得

	RegisterClassEx(&w); // アプリケーションクラス(ウィンドウクラスの指定をOSに伝える)

	RECT wrc = { 0, 0, window_width, window_height }; // ウィンドウサイズ
	// 関数を使ってウィンドウのサイズを補正する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(
		w.lpszClassName	//クラス名指定
		,L"DX12テスト"
		,WS_OVERLAPPEDWINDOW // タイトルバーと境界線があるウィンドウ
		,CW_USEDEFAULT // 表示x座標はOSに任せる
		,CW_USEDEFAULT // 表示y座標はOSに任せる
		,wrc.right - wrc.left // ウィンドウ幅
		,wrc.bottom - wrc.top // ウィンドウ高
		,nullptr // 親ウィンドウハンドル
		,nullptr // メニューハンドル
		,w.hInstance // 呼び出しアプリケーションハンドル
		,nullptr // 追加パラメータ
	);

	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	// Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (auto lv : levels)
	{
		if (D3D12CreateDevice(nullptr, lv, IID_PPV_ARGS(&_dev)) == S_OK)
		{
			featureLevel = lv;
			break;//生成可能なバージョンが見つかった
		}
	}

#ifdef _DEBUG
	auto result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif
	if (result != S_OK)
	{
		return -1;
	}

	// アダプターの列挙用
	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		std::wcout << strDesc << std::endl;
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adpt;
			break;
		}
	}

	// コマンドアロケータ
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	if (result != S_OK)
	{
		return -1;
	}

	// コマンドリスト
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));
	if (result != S_OK)
	{
		return -1;
	}
	
	ID3D12CommandQueue* _cmdQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};

	// タイムアウトなし
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	// アダプターを1つしか使わないときは0
	cmdQueueDesc.NodeMask = 0;
	// プライオリティは特になし
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	// コマンドリストと合わせる
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	// キュー生成
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));
	if (result != S_OK)
	{
		return -1;
	}

	// スワップチェーン
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH; // バックバッファーは伸び縮み可能
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後は速やかに破棄
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // 指定なし
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // ウィンドウ<->フルスクリーン切り替え可能
	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapchain
	);
	if (result != S_OK)
	{
		return -1;
	}

	// RTV(レンダーターゲットビュー)
	// ディスクリプタヒープを作成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // 表裏の2つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // 特になし

	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));
	if (result != S_OK)
	{
		return -1;
	}

	// スワップチェーンとメモリの紐づけ
	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapchain->GetDesc(&swcDesc);
	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);
	for (int idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		if (result = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx])), result == S_OK)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
			handle.ptr += idx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			_dev->CreateRenderTargetView(_backBuffers[idx], nullptr, handle);
		}
	}

	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	// ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

	MSG msg = {};
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// アプリケーションが終わるときにmessageがWM_QUITになる
		if (msg.message == WM_QUIT)
		{
			break;
		}

		auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

		// バックバッファをレンダーターゲットに設定
		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
		BarrierDesc.Transition.Subresource = 0;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		_cmdList->ResourceBarrier(1, &BarrierDesc);

		// レンダーターゲットに設定
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

		// 画面クリア
		float clearColor[] = { 1.0f,1.0f,0.0f,1.0f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		_cmdList->ResourceBarrier(1, &BarrierDesc);

		// 命令の実行
		_cmdList->Close();
		ID3D12CommandList* cmdlists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(sizeof(cmdlists) / sizeof(cmdlists[0]), cmdlists);
		// 待ち
		_cmdQueue->Signal(_fence, ++_fenceVal);
		if (_fence->GetCompletedValue() != _fenceVal)
		{
			// イベントハンドルの取得
			auto event = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceVal, event);

			// イベントが発生するまで待ち続ける
			WaitForSingleObject(event, INFINITE);

			// イベントハンドルを閉じる
			CloseHandle(event);
		}

		_cmdAllocator->Reset();
		_cmdList->Reset(_cmdAllocator, nullptr);

		// フリップ
		_swapchain->Present(1, 0);
	}

	// もうクラスは使わないので登録解除する
	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}
