#include <Windows.h>
#include <tchar.h>
#include <vector>
#include <DirectXTex.h>

// DirectX
#include <d3d12.h>
#include <Common/d3dx12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment (lib, "DirectXTex.lib")

using namespace std;
using namespace DirectX;

namespace {
constexpr int window_width = 1280;
constexpr int window_height = 720;
ID3D12Device* _dev = nullptr;
IDXGIFactory4* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapChain = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;
ID3D12DescriptorHeap* rtvHeap = nullptr;
}

// Debug用関数
void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
#endif
}

// ウィンドウの生成
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

#ifdef _DEBUG
int main()
{
#else
int WINAPI WInMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
	ID3D12Debug* debugLayer = nullptr;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	debugLayer->EnableDebugLayer();
	debugLayer->Release();

	DebugOutputFormatString("Show window test\n");

	// ウィンドウクラスの生成&登録
	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック関数の指定
	w.lpszClassName = L"DX12Sample";
	w.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&w); // アプリケーションクラス

	RECT wrc = { 0, 0, window_width, window_height };
	// 関数を使ってウィンドウのサイズを補正する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(
		w.lpszClassName,
		L"DX12テスト",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left, // ウィンドウ幅
		wrc.bottom - wrc.top, // ウィンドウ高
		nullptr,
		nullptr,
		w.hInstance,
		nullptr
	);

	ShowWindow(hwnd, SW_SHOW);

	// DirectX関連
	{
		// ファクトリ作成
		if (CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory)) != S_OK)
		{
			DebugOutputFormatString("ファクトリが生成できません");
			return -1;
		}

		// デバイス作成
		if (D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_dev)) != S_OK)
		{
			DebugOutputFormatString("Deviceが生成できません");
			return -1;
		}

		// コマンドアロケーター
		// コマンドリストを複数入れる
		if (_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(_cmdAllocator), reinterpret_cast<void**>(&_cmdAllocator)) != S_OK)
		{
			DebugOutputFormatString("コマンドアロケータを生成できません");
			return -1;
		}

		// コマンドリスト
		// 命令を一つにまとめる
		if (_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, __uuidof(_cmdList), reinterpret_cast<void**>(&_cmdList)) != S_OK)
		{
			DebugOutputFormatString("コマンドリストを生成できません");
			return -1;
		}

		// コマンドキュー
		// コマンドリストでためた命令セットを実行していくため
		D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
		cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		cmdQueueDesc.NodeMask = 0;
		if (_dev->CreateCommandQueue(&cmdQueueDesc, __uuidof(_cmdQueue), reinterpret_cast<void**>(&_cmdQueue)) != S_OK)
		{
			DebugOutputFormatString("コマンドキューを生成できません");
			return -1;
		}

		// スワップチェーン
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
		swapchainDesc.Width = window_width;
		swapchainDesc.Height = window_height;
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.Stereo = false;
		swapchainDesc.SampleDesc = { 1, 0 };
		swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		swapchainDesc.BufferCount = 2;
		swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		if (auto result = _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue, hwnd, &swapchainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&_swapChain)); result != S_OK)
		{
			DebugOutputFormatString("スワップチェーンを生成できません: %08X", result);
			return -1;
		}

		// ディスクリプタヒープ(RTV用ヒープ)
		// SRV/CBV/UAVなどの情報をヒープに書き込む
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビューRTV
		heapDesc.NumDescriptors = 2;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask = 0;
		if (_dev->CreateDescriptorHeap(&heapDesc, __uuidof(rtvHeap), reinterpret_cast<void**>(&rtvHeap)) != S_OK)
		{
			DebugOutputFormatString("ディスクリプタヒープを生成できません");
			return -1;
		}
	}

	// ディスクリプタヒープとスワップチェーンの紐づけ
	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	if (_swapChain->GetDesc(&swcDesc) != S_OK)
	{
		DebugOutputFormatString("スワップチェーンからディスクリプタを取得できません");
		return -1;
	}
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // ガンマ補正
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount, nullptr);
	for (unsigned int idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		if (_swapChain->GetBuffer(idx, __uuidof(_backBuffers[idx]), reinterpret_cast<void**>(&_backBuffers[idx])) != S_OK)
		{
			DebugOutputFormatString("バッファの取得に失敗しました");
			return -1;
		}
		// レンダーターゲットビューを生成
		D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += idx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		_dev->CreateRenderTargetView(_backBuffers[idx], &rtvDesc, handle);
	}

	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	if (FAILED(_dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence))))
	{
		return -1;
	}

	// ポリゴン描画
	struct Vertex {
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};
	Vertex vertices[] =
	{
		{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}}, //左下
		{{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}}, //左上
		{{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}}, //右下
		{{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}}, //右上
	};

	// GPUにバッファ領域を用意
	//D3D12_HEAP_PROPERTIES heapProp = {};
	//heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	//heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	//heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	//heapProp.CreationNodeMask = 0;
	//heapProp.VisibleNodeMask = 0;

	//// 頂点バッファリソース
	//D3D12_RESOURCE_DESC resdesc = {};
	//resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//resdesc.Width = sizeof(vertices);
	//resdesc.Height = 1;
	//resdesc.DepthOrArraySize = 1;
	//resdesc.MipLevels = 1;
	//resdesc.Format = DXGI_FORMAT_UNKNOWN;
	//resdesc.SampleDesc.Count = 1;
	//resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	//resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* vertBuff = nullptr;
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto heapDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));
	if (FAILED(_dev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &heapDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertBuff))))
	{
		return -1;
	}

	// 仮想アドレスを取得する
	Vertex* vertMap = nullptr;
	if (FAILED(vertBuff->Map(0, nullptr, (void**)&vertMap)))
	{
		return -1;
	}
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	// 頂点バッファビュー(1頂点当たりのバイト数や情報を知らせる)
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress(); // バッファの仮想アドレス
	vbView.SizeInBytes = sizeof(vertices); // 全バイト数
	vbView.StrideInBytes = sizeof(vertices[0]); // 1頂点あたりのバイト数

	// 頂点インデックス
	unsigned short indices[] = {
	0, 1, 2,
	2, 1, 3
	};

	ID3D12Resource* idxBuff = nullptr;
	//resdesc.Width = sizeof(indices);
	heapDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices));
	if (FAILED(_dev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &heapDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&idxBuff))))
	{
		return -1;
	}

	// データをコピー
	unsigned short* mappedIdx = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);

	// インデックスビュー
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices);

	// テクスチャロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	if (FAILED(LoadFromWICFile(L"img/textest.png", WIC_FLAGS_NONE, &metadata, scratchImg)))
	{
		DebugOutputFormatString("テクスチャロードに失敗");
		return -1;
	}

	auto img = scratchImg.GetImage(0, 0, 0);
	struct Image
	{
		size_t width;
		size_t height;
		DXGI_FORMAT format;
		size_t rowPitch;
		size_t slicePitch;
		uint8_t* pixels;
	};
		
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;
	resDesc.Width = metadata.width;
	resDesc.Height = metadata.height;
	resDesc.DepthOrArraySize = metadata.arraySize;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ID3D12Resource* texbuff = nullptr;
	if (FAILED(_dev->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&texbuff))))
	{
		return -1;
	}
	
	if (FAILED(
		texbuff->WriteToSubresource(
		0, 
		nullptr, 
		img->pixels, // 元データアドレス
		img->rowPitch, // 1ラインサイズ
		img->slicePitch // 全サイズ
		)
	))
	{
		return -1;
	}

	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 2; // SRVとCBVの2つ
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	// 生成
	if (FAILED(_dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap))))
	{
		return -1;
	}

	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	// シェーダーリソースビューを作る
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	_dev->CreateShaderResourceView(texbuff, &srvDesc, basicHeapHandle);

	// 定数バッファを作成
	//XMMATRIX matrix = XMMatrixIdentity();
	//matrix.r[0].m128_f32[0] = 2.0f / window_width;
	//matrix.r[1].m128_f32[1] = -2.0f / window_height;
	//matrix.r[3].m128_f32[0] = -1.0f;
	//matrix.r[3].m128_f32[1] = 1.0f;
	auto worldMat = XMMatrixRotationY(XM_PIDIV4);
	XMFLOAT3 eye(0, 0, -5);
	XMFLOAT3 target(0, 0, 0);
	XMFLOAT3 up(0, 1, 0);
	auto viewMat = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	auto projMat = XMMatrixPerspectiveFovLH(XM_PIDIV2, static_cast<float>(window_width) / window_height, 1.0f, 10.f);
	ID3D12Resource* constBuff = nullptr;
	auto type = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto buf = CD3DX12_RESOURCE_DESC::Buffer((sizeof(XMMATRIX) + 0xff) & ~0xff);
	_dev->CreateCommittedResource(
		&type,
		D3D12_HEAP_FLAG_NONE,
		&buf,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuff)
	);

	XMMATRIX* mapMatrix = nullptr;
	if (FAILED(constBuff->Map(0, nullptr, (void**)&mapMatrix)))
	{
		return -1;
	}
	*mapMatrix = worldMat * viewMat * projMat;

	// 定数バッファビュー
	basicHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constBuff->GetDesc().Width;
	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);


	// シェーダーオブジェクト
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;

	{
		ID3DBlob* errorBlob = nullptr;
		if (FAILED(D3DCompileFromFile(
			L"src/BasicVertexShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"BasicVS", "vs_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0, &vsBlob, &errorBlob
		)))
		{
			DebugOutputFormatString("頂点シェーダーオブジェクトの作成に失敗");
			return -1;
		}
	}

	{
		ID3DBlob* errorBlob = nullptr;
		if (FAILED(D3DCompileFromFile(
			L"src/BasicPixelShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"BasicPS", "ps_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0, &psBlob, &errorBlob
		)))
		{
			DebugOutputFormatString("ピクセルシェーダーオブジェクトの作成に失敗");
			return -1;
		}
	}

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = nullptr;
	gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = psBlob->GetBufferSize();

	// サンプルマスク
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.RasterizerState.MultisampleEnable = false;

	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gpipeline.RasterizerState.DepthClipEnable = true;

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBLendDesc = {};
	renderTargetBLendDesc.BlendEnable = false;
	renderTargetBLendDesc.LogicOpEnable = false;
	renderTargetBLendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		
	gpipeline.BlendState.RenderTarget[0] = renderTargetBLendDesc;

	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = sizeof(inputLayout) / sizeof(inputLayout[0]);
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = metadata.format; // 画像のフォーマットとそろえる
	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descTblRange[2] = {};
	// テクスチャ用レジスター0番
	descTblRange[0].NumDescriptors = 1; // テクスチャ1つ
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // テクスチャ
	descTblRange[0].BaseShaderRegister = 0; // 0番スロットから
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 定数用レジスター0番
	descTblRange[1].NumDescriptors = 1;
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[1].BaseShaderRegister = 0; // 0番スロットから
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootparam[2] = {};
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
	rootparam[0].DescriptorTable.NumDescriptorRanges = 1;

	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[1];
	rootparam[1].DescriptorTable.NumDescriptorRanges = 1;

	rootSignatureDesc.pParameters = rootparam; // ルートパラメータの先頭アドレス
	rootSignatureDesc.NumParameters = 2; // ルートパラメータ数

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーから見える
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // リサンプリングしない
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr, *errorBlob = nullptr;
	if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob)))
	{
		DebugOutputFormatString("シリアライズの作成に失敗");
		return -1;
	}

	ID3D12RootSignature* rootsignature = nullptr;
	if (FAILED(_dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootsignature))))
	{
		DebugOutputFormatString("ルートシグネチャの作成に失敗");
		return -1;
	}
	gpipeline.pRootSignature = rootsignature;
	
	ID3D12PipelineState* _pipelinestate = nullptr;
	if (auto result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate)); FAILED(result))
	{

		DebugOutputFormatString("グラフィックスパイプラインの作成に失敗[%x]", result);
		return -1;
	}
	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width;
	viewport.Height = window_height;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	D3D12_RECT scissorrect = {};
	scissorrect.top = 0;
	scissorrect.left = 0;
	scissorrect.right = scissorrect.left + window_width;
	scissorrect.bottom = scissorrect.top + window_height;

	MSG msg = {};
	float angle = 0.f;
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

		angle += 0.01f;
		worldMat = XMMatrixRotationY(angle);
		*mapMatrix = worldMat * viewMat * projMat;

		// レンダーターゲットの設定
		// バックバッファを示す「インデックス」を取得
		auto bbIdx = _swapChain->GetCurrentBackBufferIndex();
		auto rtvH = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		// リソース設定

		//D3D12_RESOURCE_BARRIER barriersDesc = {};
		//barriersDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 遷移
		//barriersDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		//barriersDesc.Transition.pResource = _backBuffers[bbIdx];
		//barriersDesc.Transition.Subresource = 0;
		//barriersDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		//barriersDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		//_cmdList->ResourceBarrier(1, &barriersDesc);
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		_cmdList->ResourceBarrier(1, &barrier);
		_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

		// レンダーターゲットのクリア
		// 画面クリア
		float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		auto heapHandle = basicDescHeap->GetGPUDescriptorHandleForHeapStart();
		_cmdList->SetPipelineState(_pipelinestate);
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		_cmdList->IASetIndexBuffer(&ibView);
		_cmdList->SetGraphicsRootSignature(rootsignature);
		_cmdList->SetDescriptorHeaps(1, &basicDescHeap);
		_cmdList->SetGraphicsRootDescriptorTable(0, heapHandle);
		heapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		_cmdList->SetGraphicsRootDescriptorTable(1, heapHandle);
		_cmdList->DrawInstanced(4, 1, 0, 0);

		//barriersDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		//barriersDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;]
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		_cmdList->ResourceBarrier(1, &barrier);
		//  命令のクローズ
		{
			_cmdList->Close();
		}

		// コマンドリストの実行
		{
			ID3D12CommandList* cmdlists[] = { _cmdList };
			_cmdQueue->ExecuteCommandLists(sizeof(cmdlists)/sizeof(cmdlists[0]), cmdlists);
		}

		// フェンス
		_cmdQueue->Signal(_fence, ++_fenceVal);
		if (_fence->GetCompletedValue() != _fenceVal)
		{
			auto event = CreateEvent(nullptr, false, false, nullptr);

			_fence->SetEventOnCompletion(_fenceVal, event);

			WaitForSingleObject(event, INFINITE);

			// イベントハンドルを閉じる
			CloseHandle(event);
		}

		// コマンドキューのクリア
		if (FAILED(_cmdAllocator->Reset())|| FAILED(_cmdList->Reset(_cmdAllocator, nullptr)))
		{
			DebugOutputFormatString("コマンドキューのクリアに失敗しました");
			return -1;
		}

		// 画面スワップ(フリップ)
		_swapChain->Present(1, 0);
	}

	// もうクラスは使わないので登録解除する
	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}