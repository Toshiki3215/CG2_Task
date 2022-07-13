#include "WindowsApp.h"
#include"DirectXInitialize.h"
#include<DirectXMath.h>
#include<DirectXTex.h>

using namespace DirectX;

#include<d3dcompiler.h>

#pragma comment(lib,"dxgi.lib")

#define DIRECTINPUT_VERSION 0x0800
#include<dinput.h>
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

//定数バッファ用データ構造体(マテリアル)
struct ConstBufferDataMaterial
{
	//色(RGBA)
	XMFLOAT4 color;
};

//定数バッファ用データ構造体(3D変換行列)
struct ConstBufferDataTransform
{
	//3D変換行列
	XMMATRIX mat;
};

//3Dオブジェクト型
struct Object3d
{
	//定数バッファ(行列)
	ID3D12Resource* constBuffTransform;

	//定数バッファマップ(行列)
	ConstBufferDataTransform* constMapTransform;

	//アフィン変換情報
	XMFLOAT3 scale = { 1,1,1 };
	XMFLOAT3 rotation = { 0,0,0 };
	XMFLOAT3 position = { 0,0,0 };

	//ワールド変換行列
	XMMATRIX matWorld;

	//親オブジェクトへのポインタ
	Object3d* parent = nullptr;

};

//3Dオブジェクト初期化

void InitializeObject3d(Object3d* object, ID3D12Device* device)
{
	HRESULT result;

	//定数バッファのヒープ設定
	D3D12_HEAP_PROPERTIES heapProp{};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

	//定数バッファのリソース設定
	D3D12_RESOURCE_DESC resdesc{};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = (sizeof(ConstBufferDataTransform) + 0xff) & ~0xff;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.SampleDesc.Count = 1;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	//定数バッファの生成
	result = device->CreateCommittedResource
	(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&object->constBuffTransform)
	);

	assert(SUCCEEDED(result));

	//定数バッファのマッピング
	result = object->constBuffTransform->Map(0, nullptr, (void**)&object->constMapTransform);

	assert(SUCCEEDED(result));
}

void UpdateObject3d(Object3d* object, XMMATRIX& matView, XMMATRIX& matProjection)
{
	XMMATRIX matScale, matRot, matTrans;

	//スケール、回転、平行移動行列
	matScale = XMMatrixScaling(object->scale.x, object->scale.y, object->scale.z);
	matRot = XMMatrixIdentity();
	matRot *= XMMatrixRotationZ(object->rotation.z);
	matRot *= XMMatrixRotationX(object->rotation.x);
	matRot *= XMMatrixRotationY(object->rotation.y);
	matTrans = XMMatrixTranslation(object->position.x, object->position.y, object->position.z);

	//ワールド行列の合成
	//変形をリセット
	object->matWorld = XMMatrixIdentity();

	//ワールド行列にスケーリングを反映
	object->matWorld *= matScale;

	//ワールド行列に回転を反映
	object->matWorld *= matRot;

	//ワールド行列に平行移動を反映
	object->matWorld *= matTrans;

	//親オブジェクトがあれば
	if (object->parent != nullptr)
	{
		//親オブジェクトのワールド行列を掛ける
		object->matWorld *= object->parent->matWorld;
	}

	//定数バッファへデータ転送
	object->constMapTransform->mat = object->matWorld * matView * matProjection;

}

void DrawObject3d(Object3d* object, ID3D12GraphicsCommandList* commandList, D3D12_VERTEX_BUFFER_VIEW& vbView, D3D12_INDEX_BUFFER_VIEW& ibView, UINT numIndeces)
{
	//頂点バッファの設定
	commandList->IASetVertexBuffers(0, 1, &vbView);

	//インデックスバッファの設定
	commandList->IASetIndexBuffer(&ibView);

	//定数バッファビュー(CBV)の設定コマンド
	commandList->SetGraphicsRootConstantBufferView(2, object->constBuffTransform->GetGPUVirtualAddress());

	//描画コマンド
	commandList->DrawIndexedInstanced(numIndeces, 1, 0, 0, 0);

}

//Widowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// ----- ウィンドウクラス ----- //
	WindowsApp winApp;

	winApp.createWin();

	// --- DirectX初期化処理　ここから --- //

#ifdef _DEBUG
//デバッグレイヤーをオンに
	ID3D12Debug* debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		//debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	// ----- DirectX クラス ----- //
	DirectXInitialize DXInit;

	DXInit.createDX(winApp.hwnd);

	//DirectInputの初期化
	IDirectInput8* directInput = nullptr;
	DXInit.result = DirectInput8Create(winApp.w.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(DXInit.result));

	//キーボードデバイスの生成
	IDirectInputDevice8* keyboard = nullptr;
	DXInit.result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(DXInit.result));

	//入力データ形式のセット
	DXInit.result = keyboard->SetDataFormat(&c_dfDIKeyboard); //標準形式
	assert(SUCCEEDED(DXInit.result));

	//排他制御レベルのセット
	DXInit.result = keyboard->SetCooperativeLevel(winApp.hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(DXInit.result));

	// --- DirectX初期化処理　ここまで --- //


	// --- 描画初期化処理　ここから --- //

	//頂点データ構造体
	struct Vertex
	{
		XMFLOAT3 pos;    //x,y,z座標
		XMFLOAT3 normal; //法線ベクトル
		XMFLOAT2 uv;     //u,v座標
	};

	//頂点データ
	Vertex vertices[] =
	{
		//  x     　y     z     法線   u    v
		//前
		{{  -5.0f, -5.0f, -5.0f},{},{0.0f,1.0f}},  //左下
		{{  -5.0f,  5.0f, -5.0f},{},{0.0f,0.0f}},  //左上
		{{   5.0f, -5.0f, -5.0f},{},{1.0f,1.0f}},  //右下
		{{   5.0f,  5.0f, -5.0f},{},{1.0f,0.0f}},  //右上
								
		//後					
		{{  -5.0f, -5.0f,  5.0f},{},{0.0f,1.0f}},  //左下
		{{  -5.0f,  5.0f,  5.0f},{},{0.0f,0.0f}},  //左上
		{{   5.0f, -5.0f,  5.0f},{},{1.0f,1.0f}},  //右下
		{{   5.0f,  5.0f,  5.0f},{},{1.0f,0.0f}},  //右上
								 
		//左					
		{{  -5.0f, -5.0f, -5.0f},{},{0.0f,1.0f}},  //左下
		{{  -5.0f, -5.0f,  5.0f},{},{0.0f,0.0f}},  //左上
		{{  -5.0f,  5.0f, -5.0f},{},{1.0f,1.0f}},  //右下
		{{  -5.0f,  5.0f,  5.0f},{},{1.0f,0.0f}},  //右上
								
		//右					
		{{   5.0f, -5.0f, -5.0f},{},{0.0f,1.0f}},  //左下
		{{   5.0f, -5.0f,  5.0f},{},{0.0f,0.0f}},  //左上
		{{   5.0f,  5.0f, -5.0f},{},{1.0f,1.0f}},  //右下
		{{   5.0f,  5.0f,  5.0f},{},{1.0f,0.0f}},  //右上
								
		//下					
		{{  -5.0f, -5.0f, -5.0f},{},{0.0f,1.0f}},  //左下
		{{  -5.0f, -5.0f,  5.0f},{},{0.0f,1.0f}},  //左上
		{{   5.0f, -5.0f, -5.0f},{},{1.0f,1.0f}},  //右下
		{{   5.0f, -5.0f,  5.0f},{},{1.0f,1.0f}},  //右上
								
		//上					
		{{  -5.0f,  5.0f, -5.0f},{},{0.0f,1.0f}},  //左下
		{{  -5.0f,  5.0f,  5.0f},{},{0.0f,1.0f}},  //左上
		{{   5.0f,  5.0f, -5.0f},{},{1.0f,1.0f}},  //右下
		{{   5.0f,  5.0f,  5.0f},{},{1.0f,1.0f}},  //右上
	};

	//インデックスデータ
	unsigned short indices[] =
	{
		//前
		0,1,2, //三角形1つ目
		2,1,3, //三角形2つ目

		//後
		5,4,6, //三角形3つ目
		5,6,7, //三角形4つ目

		//左
		8,9,10,
		10,9,11,

		//右
		13,12,14,
		13,14,15,

		//下
		17,16,18,
		17,18,19,

		//上
		20,21,22,
		22,21,23,
	};

	//法線の計算
	for (int i = 0; i < 36 / 3; i++)
	{
		//三角形1つごとに計算していく

		//三角形のインデックスを取り出して、一時的な変数に入れる
		unsigned short index0 = indices[i * 3 + 0];
		unsigned short index1 = indices[i * 3 + 1];
		unsigned short index2 = indices[i * 3 + 2];

		//三角形を構成する頂点座標をベクトルに代入
		XMVECTOR p0 = XMLoadFloat3(&vertices[index0].pos);
		XMVECTOR p1 = XMLoadFloat3(&vertices[index1].pos);
		XMVECTOR p2 = XMLoadFloat3(&vertices[index2].pos);

		//p0→p1ベクトル、p0→p2ベクトルを計算 (ベクトルの減算)
		XMVECTOR v1 = XMVectorSubtract(p1, p0);
		XMVECTOR v2 = XMVectorSubtract(p2, p0);

		//外積は両方から垂直なベクトル
		XMVECTOR normal = XMVector3Cross(v1, v2);

		//正規化(長さを1にする)
		normal = XMVector3Normalize(normal);

		//求めた法線を頂点データに代入
		XMStoreFloat3(&vertices[index0].normal, normal);
		XMStoreFloat3(&vertices[index1].normal, normal);
		XMStoreFloat3(&vertices[index2].normal, normal);
	}

	//頂点データ全体のサイズ = 頂点データ一つ分のサイズ * 頂点データの要素数
	UINT sizeVB = static_cast<UINT>(sizeof(vertices[0]) * _countof(vertices));

	// 頂点バッファの設定
	D3D12_HEAP_PROPERTIES heapProp{}; // ヒープ設定
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD; // GPUへの転送用

	// リソース設定
	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeVB; // 頂点データ全体のサイズ
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 頂点バッファの生成
	ID3D12Resource* vertBuff = nullptr;
	DXInit.result = DXInit.device->CreateCommittedResource
	(&heapProp, // ヒープ設定
		D3D12_HEAP_FLAG_NONE,
		&resDesc, // リソース設定
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertBuff));
	assert(SUCCEEDED(DXInit.result));

	// GPU上のバッファに対応した仮想メモリ(メインメモリ上)を取得
	Vertex* vertMap = nullptr;
	DXInit.result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	assert(SUCCEEDED(DXInit.result));

	// 全頂点に対して
	for (int i = 0; i < _countof(vertices); i++)
	{
		vertMap[i] = vertices[i]; // 座標をコピー
	}

	// 繋がりを解除
	vertBuff->Unmap(0, nullptr);

	// 頂点バッファビューの作成
	D3D12_VERTEX_BUFFER_VIEW vbView{};

	// GPU仮想アドレス
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();

	// 頂点バッファのサイズ
	vbView.SizeInBytes = sizeVB;

	// 頂点1つ分のデータサイズ
	//vbView.StrideInBytes = sizeof(XMFLOAT3);
	vbView.StrideInBytes = sizeof(vertices[0]);

	ID3DBlob* vsBlob = nullptr; // 頂点シェーダオブジェクト
	ID3DBlob* psBlob = nullptr; // ピクセルシェーダオブジェクト
	ID3DBlob* errorBlob = nullptr; // エラーオブジェクト

	// 頂点シェーダの読み込みとコンパイル
	DXInit.result = D3DCompileFromFile
	(L"BasicVS.hlsl", // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&vsBlob, &errorBlob);

	// エラーなら
	if (FAILED(DXInit.result))
	{
		// errorBlobからエラー内容をstring型にコピー
		std::string error;
		error.resize(errorBlob->GetBufferSize());
		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			error.begin());
		error += "\n";

		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(error.c_str());
		assert(0);
	}

	// ピクセルシェーダの読み込みとコンパイル
	DXInit.result = D3DCompileFromFile
	(L"BasicPS.hlsl", // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&psBlob, &errorBlob);

	// エラーなら
	if (FAILED(DXInit.result))
	{
		// errorBlobからエラー内容をstring型にコピー
		std::string error;
		error.resize(errorBlob->GetBufferSize());
		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			error.begin());
		error += "\n";

		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(error.c_str());
		assert(0);
	}

	// 頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		//x,y,z座標
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},

		//法線ベクトル
		{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},

		//u,v座標
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};


	//インデックスデータ全体のサイズ
	UINT sizeIB = static_cast<UINT>(sizeof(uint16_t) * _countof(indices));

	//リソース設定
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeIB; //インデックス情報が入る分のサイズ
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	//インデックスバッファの生成
	ID3D12Resource* indexBuff = nullptr;
	DXInit.result = DXInit.device->CreateCommittedResource
	(
		&heapProp, //ヒープ設定
		D3D12_HEAP_FLAG_NONE,
		&resDesc, //リソース設定
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBuff)
	);

	//インデックスバッファをマッピング
	uint16_t* indexMap = nullptr;
	DXInit.result = indexBuff->Map(0, nullptr, (void**)&indexMap);

	//全てのインデックスに対して
	for (int i = 0; i < _countof(indices); i++)
	{
		indexMap[i] = indices[i]; //インデックスをコピー
	}

	//マッピング解除
	indexBuff->Unmap(0, nullptr);

	//インデックスバッファビューの生成
	D3D12_INDEX_BUFFER_VIEW ibView{};
	ibView.BufferLocation = indexBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeIB;

	// グラフィックスパイプライン設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};

	// シェーダーの設定
	pipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	pipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
	pipelineDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
	pipelineDesc.PS.BytecodeLength = psBlob->GetBufferSize();

	// サンプルマスクの設定
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定

	// ラスタライザの設定
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;   // 背面をカリング
	pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // ポリゴン内塗りつぶし
	pipelineDesc.RasterizerState.DepthClipEnable = true; // 深度クリッピングを有効に

	//RGBA全てのチャンネルを描画(動作は上と同じ)
	D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = pipelineDesc.BlendState.RenderTarget[0];
	blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// 頂点レイアウトの設定
	pipelineDesc.InputLayout.pInputElementDescs = inputLayout;
	pipelineDesc.InputLayout.NumElements = _countof(inputLayout);

	// 図形の形状設定
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// その他の設定
	pipelineDesc.NumRenderTargets = 1; // 描画対象は1つ
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 0~255指定のRGBA
	pipelineDesc.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング

	//デスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.NumDescriptors = 1;      //一度の描画に使うテクスチャが一枚なので１
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange.BaseShaderRegister = 0;  //テクスチャレジスタ0番
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// - ルートパラメータの設定 - //
	D3D12_ROOT_PARAMETER rootParams[3] = {};

	//  定数バッファ0番
	//定数バッファビュー
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	
	//定数バッファ番号
	rootParams[0].Descriptor.ShaderRegister = 0;

	//デフォルト値
	rootParams[0].Descriptor.RegisterSpace = 0;

	//全てのシェーダから見える
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// テクスチャレジスタ0番
	//種類
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;

	//デスクリプタレンジ
	rootParams[1].DescriptorTable.pDescriptorRanges = &descriptorRange;

	//デスクリプタレンジ数
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;

	//全てのシェーダから見える
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	//  定数バッファ1番
	//種類
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;

	//定数バッファ番号
	rootParams[2].Descriptor.ShaderRegister = 1;

	//デフォルト値
	rootParams[2].Descriptor.RegisterSpace = 0;

	//全てのシェーダから見える
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


	//テクスチャサンプラーの設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc{};
	
	//横繰り返し(タイリング)
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

	//縦繰り返し(タイリング)
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

	//奥行繰り返し(タイリング)
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

	//ボーダーの時は黒
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;

	//全てリニア補間
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

	//ミニマップ最大値
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

	//ミニマップ最小値
	samplerDesc.MinLOD = 0.0f;

	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	//ピクセルシェーダからのみ使用可能
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// ルートシグネチャ
	ID3D12RootSignature* rootSignature;

	// ルートシグネチャの設定
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//ルートパラメータの先頭アドレス
	rootSignatureDesc.pParameters = rootParams;

	//ルートパラメータ数
	rootSignatureDesc.NumParameters = _countof(rootParams);

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	// ルートシグネチャのシリアライズ
	ID3DBlob* rootSigBlob = nullptr;
	DXInit.result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob, &errorBlob);
	assert(SUCCEEDED(DXInit.result));
	DXInit.result = DXInit.device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(DXInit.result));
	rootSigBlob->Release();

	// パイプラインにルートシグネチャをセット
	pipelineDesc.pRootSignature = rootSignature;

	//デプスステンシルステートの設定
	pipelineDesc.DepthStencilState.DepthEnable = true;   //深度テストを行う
	pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;   //書き込み許可
	pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;   //小さければ合格
	pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;   //深度値フォーマット

	// パイプランステートの生成
	ID3D12PipelineState* pipelineState = nullptr;
	DXInit.result = DXInit.device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState));
	assert(SUCCEEDED(DXInit.result));

	//3Dオブジェクトの数
	const size_t kObjectCount = 50;

	//3Dオブジェクトの配列
	Object3d object3ds[kObjectCount];

	//配列内の全オブジェクトに対して
	for (int i = 0; i < _countof(object3ds); i++)
	{
		//初期化
		InitializeObject3d(&object3ds[i], DXInit.device);

		//ここから↓は親子構造のサンプル
		//先頭以外なら
		if (i > 0)
		{
			//一つ前のオブジェクトを親オブジェクトとする
			object3ds[i].parent = &object3ds[i - 1];

			//親オブジェクトの9割の大きさ
			object3ds[i].scale = { 0.9f,0.9f,0.9f };

			//親オブジェクトに対してZ軸まわりに30度回転
			object3ds[i].rotation = { 0.0f,0.0f,XMConvertToRadians(30.0f) };

			//親オブジェクトに対してZ軸方向に-8.0ずらす
			object3ds[i].position = { 0.0f,0.0f,-8.0f };

		}

	}

	//ヒープ設定
	D3D12_HEAP_PROPERTIES cbHeapProp{};

	//GPUへの転送用
	cbHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

	//リソース設定
	D3D12_RESOURCE_DESC cbResourceDesc{};
	cbResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//256バイトアラインメント
	cbResourceDesc.Width = (sizeof(ConstBufferDataMaterial) + 0xff) & ~0xff;
	cbResourceDesc.Height = 1;
	cbResourceDesc.DepthOrArraySize = 1;
	cbResourceDesc.MipLevels = 1;
	cbResourceDesc.SampleDesc.Count = 1;
	cbResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* constBuffMaterial = nullptr;

	//定数バッファの生成
	DXInit.result = DXInit.device->CreateCommittedResource
	(
		//ヒープ設定
		&cbHeapProp,
		D3D12_HEAP_FLAG_NONE,
		//リソース設定
		&cbResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuffMaterial)
	);
	assert(SUCCEEDED(DXInit.result));

	//定数バッファのマッピング
	ConstBufferDataMaterial* constMapMaterial = nullptr;

	//マッピング
	DXInit.result = constBuffMaterial->Map(0, nullptr, (void**)&constMapMaterial);
	assert(SUCCEEDED(DXInit.result));

	//ビュー変換行列
	XMMATRIX matView;

	//視点座標
	XMFLOAT3 eye(0, 0, -100);

	//注視点座標
	XMFLOAT3 target(0, 0, 0);

	//上方向ベクトル
	XMFLOAT3 up(0, 1, 0);

	matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

	//カメラの回転角
	float angle = 0.0f;

	//射影変換行列(透視投影)
	XMMATRIX matProjection = XMMatrixPerspectiveFovLH
	(
		//上下が画角45度
		XMConvertToRadians(45.0f),

		//アスペクト比(画面横幅 / 画面縦幅)
		(float)winApp.window_width / winApp.window_height,

		//前幅,奥幅
		0.1f, 1000.0f
	);

	//定数バッファにデータを転送する
	//値を書き込むと自動的に転送される
	constMapMaterial->color = XMFLOAT4(1, 1, 1, 0.5f); //白

	TexMetadata metadata{};
	ScratchImage scratchImg{};

	//WICテクスチャのロード
	DXInit.result = LoadFromWICFile
	(
		L"Resources/texture3.png",  //「Resources」フォルダの「texture.png」
		WIC_FLAGS_NONE,
		&metadata, scratchImg
	);

	ScratchImage mipChain{};

	//ミニマップ生成
	DXInit.result = GenerateMipMaps
	(
		scratchImg.GetImages(), scratchImg.GetImageCount(), scratchImg.GetMetadata(),
		TEX_FILTER_DEFAULT, 0, mipChain
	);

	if (SUCCEEDED(DXInit.result))
	{
		scratchImg = std::move(mipChain);
		metadata = scratchImg.GetMetadata();
	}

	//読み込んだディフューズテクスチャをSRGBとして扱う
	metadata.format = MakeSRGB(metadata.format);

	// --- テクスチャバッファ --- //
	//ヒープ設定
	D3D12_HEAP_PROPERTIES textureHeapProp{};
	textureHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	textureHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	textureHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	//リソース設定
	D3D12_RESOURCE_DESC textureResourceDesc{};
	textureResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureResourceDesc.Format = metadata.format;
	textureResourceDesc.Width = metadata.width;   //幅
	textureResourceDesc.Height = (UINT)metadata.height;  //高さ
	textureResourceDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
	textureResourceDesc.MipLevels = (UINT16)metadata.mipLevels;
	textureResourceDesc.SampleDesc.Count = 1;

	//テクスチャバッファの生成
	ID3D12Resource* texBuff = nullptr;
	DXInit.result = DXInit.device->CreateCommittedResource
	(
		&textureHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&textureResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);

	//全ミニマップについて
	for (size_t i = 0; i < metadata.mipLevels; i++)
	{
		//ミニマップレベルを指定してイメージを取得
		const Image* img = scratchImg.GetImage(i, 0, 0);

		//テクスチャバッファにデータ転送
		DXInit.result = texBuff->WriteToSubresource
		(
			(UINT)i,
			//全領域へコピー
			nullptr, 
			//元データアドレス
			img->pixels,
			//1ラインサイズ
			(UINT)img->rowPitch,
			//1枚サイズ
			(UINT)img->slicePitch
		);
		assert(SUCCEEDED(DXInit.result));
	}

	//SRVの最大個数
	const size_t kMaxSRVCount = 2056;

	//デスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;      //レンダーターゲットビュー
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;  //シェーダから見えるように
	srvHeapDesc.NumDescriptors = kMaxSRVCount;

	//設定を元のSRV用デスクリプタヒープを生成
	ID3D12DescriptorHeap* srvHeap = nullptr;
	DXInit.result = DXInit.device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap));
	assert(SUCCEEDED(DXInit.result));

	//SRVヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();

	//シェーダリソースビュー設定
	//設定構造体
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

	//RGBA float
	srvDesc.Format = resDesc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	//2Dテクスチャ
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = resDesc.MipLevels;

	//ハンドルの指す位置にシェーダーリソースビュー作成
	DXInit.device->CreateShaderResourceView(texBuff, &srvDesc, srvHandle);

	//深度バッファのリソース設定

	//リソース設定
	D3D12_RESOURCE_DESC depthResourceDesc{};
	depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResourceDesc.Width = WindowsApp::window_width;   //レンダーターゲットに合わせる
	depthResourceDesc.Height = WindowsApp::window_height; //レンダーターゲットに合わせる
	depthResourceDesc.DepthOrArraySize = 1;
	depthResourceDesc.Format = DXGI_FORMAT_D32_FLOAT;  //深度値フォーマット
	depthResourceDesc.SampleDesc.Count = 1;
	depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;  //デプスステンシル

	//深度バッファのその他の設定

	//深度値用ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp{};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;  //深度値1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;  //深度値フォーマット

	//深度バッファ生成

	//リソース生成
	ID3D12Resource* depthBuff = nullptr;
	DXInit.result = DXInit.device->CreateCommittedResource
	(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,  //深度値書き込みに使用
		&depthClearValue,
		IID_PPV_ARGS(&depthBuff)
	);

	//深度ビュー用デスクリプタヒープ生成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.NumDescriptors = 1;   //深度ビューは1つ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;   //デプスステンシルビュー
	ID3D12DescriptorHeap* dsvHeap = nullptr;
	DXInit.result = DXInit.device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

	//深度ビュー生成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;   //深度値フォーマット
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DXInit.device->CreateDepthStencilView(depthBuff, &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());


	// --- 描画初期化処理　ここまで --- //

	//ゲームループ
	while (true)
	{
		// ----- ウィンドウメッセージ処理 ----- //

		//メッセージがあるか？
		if (PeekMessage(&winApp.msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&winApp.msg);//キー入力メッセージの処理
			DispatchMessage(&winApp.msg);//プロシージャにメッセージを送る
		}

		//✕ボタンで終了メッセージが来たらゲームループを抜ける
		if (winApp.msg.message == WM_QUIT)
		{
			break;
		}

		// --- DirectX毎フレーム処理　ここから --- //

		//キーボード情報の取得開始
		keyboard->Acquire();

		//全キーの入力状態を取得する
		BYTE key[256] = {};
		keyboard->GetDeviceState(sizeof(key), key);

		//数字の0キーが押されていたら
		if (key[DIK_0])
		{
			OutputDebugStringA("Hit 0\n"); //出力ウィンドウに「Hit 0」と表示
		}

		// バックバッファの番号を取得(2つなので0番か1番)
		UINT bbIndex = DXInit.swapChain->GetCurrentBackBufferIndex();

		//回転
		if (key[DIK_D] || key[DIK_A])
		{
			if (key[DIK_D])
			{
				angle += XMConvertToRadians(1.0f);
			}
			else if (key[DIK_A])
			{
				angle -= XMConvertToRadians(1.0f);
			}

			//angleラジアンだけY軸まわりに回転。半径は-100
			eye.x = -100 * sinf(angle);
			eye.z = -100 * cosf(angle);

			//ビュー変換行列を作り直す
			matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

		}

		//座標操作
		if (key[DIK_UP] || key[DIK_DOWN] || key[DIK_RIGHT] || key[DIK_LEFT])
		{
			if (key[DIK_UP]) { object3ds[0].position.y += 1.0f; }
			else if (key[DIK_DOWN]) { object3ds[0].position.y -= 1.0f; }

			if (key[DIK_RIGHT]) { object3ds[0].position.x += 1.0f; }
			else if (key[DIK_LEFT]) { object3ds[0].position.x -= 1.0f; }
		}

		for (size_t i = 0; i < _countof(object3ds); i++)
		{
			UpdateObject3d(&object3ds[i], matView, matProjection);
		}


		// 1.リソースバリアで書き込み可能に変更
		D3D12_RESOURCE_BARRIER barrierDesc{};
		barrierDesc.Transition.pResource = DXInit.backBuffers[bbIndex]; // バックバッファを指定
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // 表示状態から
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 描画状態へ
		DXInit.commandList->ResourceBarrier(1, &barrierDesc);

		//2.描画先の変更
		//レンダーターゲットビューのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = DXInit.rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += bbIndex * DXInit.device->GetDescriptorHandleIncrementSize(DXInit.rtvHeapDesc.Type);
		//DXInit.commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

		//深度ステンシルビュー用デスクリプターヒープのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		DXInit.commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		//3.画面クリア
		FLOAT clearColor[] = { 0.1f,0.25f,0.5f,0.0f }; //青っぽい色{ R, G, B, A }
		DXInit.commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		DXInit.commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		//スペースキーが押されていたら
		if (key[DIK_SPACE])
		{
			FLOAT clearColor[] = { 0.1f,0.8f,0.8f,0.0f };
			DXInit.commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		}

		//4.描画コマンド　ここから

		// --- グラフィックスコマンド --- //

		// ビューポート設定コマンド
		D3D12_VIEWPORT viewport{};
		viewport.Width = WindowsApp::window_width;
		viewport.Height = WindowsApp::window_height;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		// ビューポート設定コマンドを、コマンドリストに積む
		DXInit.commandList->RSSetViewports(1, &viewport);

		// シザー矩形
		D3D12_RECT scissorRect{};
		scissorRect.left = 0; // 切り抜き座標左
		scissorRect.right = scissorRect.left + WindowsApp::window_width; // 切り抜き座標右
		scissorRect.top = 0; // 切り抜き座標上
		scissorRect.bottom = scissorRect.top + WindowsApp::window_height; // 切り抜き座標下

		// シザー矩形設定コマンドを、コマンドリストに積む
		DXInit.commandList->RSSetScissorRects(1, &scissorRect);

		// パイプラインステートとルートシグネチャの設定コマンド
		DXInit.commandList->SetPipelineState(pipelineState);
		DXInit.commandList->SetGraphicsRootSignature(rootSignature);

		// プリミティブ形状の設定コマンド
		// 三角形リスト
		DXInit.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
		// 点のリスト
		//DXInit.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST); 
		
		// 線のリスト
		//DXInit.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST); 
		
		// 線のストリップ
		//DXInit.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
		
		// 三角形のストリップ
		//DXInit.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		

		// 頂点バッファビューの設定コマンド
		DXInit.commandList->IASetVertexBuffers(0, 1, &vbView);

		//定数バッファビュー(CBV)の設定コマンド
		DXInit.commandList->SetGraphicsRootConstantBufferView(0, constBuffMaterial->GetGPUVirtualAddress());

		//SRVヒープの設定コマンド
		DXInit.commandList->SetDescriptorHeaps(1, &srvHeap);

		//SRVヒープの先頭ハンドルを取得(SRVを指しているはず)
		D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();

		//SRVヒープの先頭にあるSRVをルートパラメータ1番に設定
		DXInit.commandList->SetGraphicsRootDescriptorTable(1, srvGpuHandle);

		//全オブジェクトについての処理
		for (int i = 0; i < _countof(object3ds); i++)
		{
			DrawObject3d(&object3ds[i], DXInit.commandList, vbView, ibView, _countof(indices));
		}

		//ブレンドを有効にする
		blenddesc.BlendEnable = true;

		//加算
		blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;

		//ソースの値を100%使う
		blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE;

		//デストの値を0%使う
		blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO;


		//4.描画コマンド　ここまで

		// 5.リソースバリアを戻す
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // 描画状態から
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; // 表示状態へ
		DXInit.commandList->ResourceBarrier(1, &barrierDesc);

		// 命令のクローズ
		DXInit.result = DXInit.commandList->Close();
		assert(SUCCEEDED(DXInit.result));

		// コマンドリストの実行
		ID3D12CommandList* commandLists[] = { DXInit.commandList };
		DXInit.commandQueue->ExecuteCommandLists(1, commandLists);

		// 画面に表示するバッファをフリップ(裏表の入替え)
		DXInit.result = DXInit.swapChain->Present(1, 0);
		assert(SUCCEEDED(DXInit.result));

		//コマンドの実行完了を待つ
		DXInit.commandQueue->Signal(DXInit.fence, ++DXInit.fenceVal);
		if (DXInit.fence->GetCompletedValue() != DXInit.fenceVal)
		{
			HANDLE event = CreateEvent(nullptr, false, false, nullptr);
			DXInit.fence->SetEventOnCompletion(DXInit.fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}

		//キューをクリア
		DXInit.result = DXInit.commandAllocator->Reset();
		assert(SUCCEEDED(DXInit.result));

		//再びコマンドリストにためる準備
		DXInit.result = DXInit.commandList->Reset(DXInit.commandAllocator, nullptr);
		assert(SUCCEEDED(DXInit.result));

		// --- DirectX毎フレーム処理　ここまで --- //

	}

	//ウィンドウクラスを登録解除
	UnregisterClass(winApp.w.lpszClassName, winApp.w.hInstance);

	return 0;
}