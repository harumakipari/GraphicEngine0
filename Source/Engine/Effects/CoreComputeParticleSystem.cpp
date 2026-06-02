#include "pch.h"
#include "CoreComputeParticleSystem.h"


#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI


CoreComputeParticleSystem::CoreComputeParticleSystem(ID3D11Device* device, UINT particlesCount,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView, DirectX::XMUINT2 splitCount)
{
    HRESULT hr;
    //バイトニックソートの仕様上、２の累乗にしておかないといけないため、パーティクル数を補正
    float f_exponent = log2f(static_cast<float>(particlesCount));
    int exponent = static_cast<int>(ceilf(f_exponent) + 0.5f);
    particlesCount = static_cast<UINT>(pow(2, exponent) + 0.5f);
    particlesCount = max(min(particlesCount, 1 << 27), 1 << 7);

    //パーティクル数をスレッド数に合わせて制限
    numParticles = ((particlesCount + (NumParticleThread - 1)) / NumParticleThread) * NumParticleThread;
    numEmitParticles = numParticles;
    textureSplitCount = splitCount;
    oneShotInitialize = false;

    //定数バッファ
    {
        D3D11_BUFFER_DESC bufferDesc{};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = 0;
        bufferDesc.MiscFlags = 0;
        bufferDesc.StructureByteStride = 0;
        {
            bufferDesc.ByteWidth = sizeof(CommonConstants);
            hr = device->CreateBuffer(&bufferDesc, nullptr, commonConstantBuffer.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        }
        {
            bufferDesc.ByteWidth = sizeof(BitonicSortConstants);
            hr = device->CreateBuffer(&bufferDesc, nullptr, bitonicSortConstantBuffer.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        }
    }

    //パーティクルバッファ生成
    {
        D3D11_BUFFER_DESC desc{};
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.ByteWidth = sizeof(ParticleData) * numParticles;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = sizeof(ParticleData);
        desc.Usage = D3D11_USAGE_DEFAULT;
        hr = device->CreateBuffer(&desc, nullptr, particleDataBuffer.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = device->CreateShaderResourceView(particleDataBuffer.Get(), nullptr, particleDataShaderResourceView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = device->CreateUnorderedAccessView(particleDataBuffer.Get(), nullptr, particleDataUnordredAccessView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    //パーティクルヘッダーバッファ
    {
        D3D11_BUFFER_DESC desc{};
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.ByteWidth = sizeof(ParticleHeader) * numParticles;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = sizeof(ParticleHeader);
        desc.Usage = D3D11_USAGE_DEFAULT;
        hr = device->CreateBuffer(&desc, nullptr, particleHeaderBuffer.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = device->CreateShaderResourceView(particleHeaderBuffer.Get(), nullptr, particleHeaderShaderResourceView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = device->CreateUnorderedAccessView(particleHeaderBuffer.Get(), nullptr, particleHeaderUnordredAccessView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    //パーティクルの生成/破棄番号を溜め込むバッファ生成
    {
        D3D11_BUFFER_DESC desc{};
        desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        desc.ByteWidth = sizeof(UINT) * numParticles;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = sizeof(UINT);
        desc.Usage = D3D11_USAGE_DEFAULT;
        hr = device->CreateBuffer(&desc, nullptr, particleAppendConsumeBuffer.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        //Append/Consumeを利用する場合はビュー側にフラグを立てる
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = numParticles;
        uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
        hr = device->CreateUnorderedAccessView(particleAppendConsumeBuffer.Get(), &uavDesc, particleAppendConsumeUnordredAccessView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    //パーティクルエミット用バッファ生成
    {
        D3D11_BUFFER_DESC desc{};
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.ByteWidth = sizeof(EmitParticleData) * numEmitParticles;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = sizeof(EmitParticleData);
        desc.Usage = D3D11_USAGE_DEFAULT;
        hr = device->CreateBuffer(&desc, nullptr, particleEmitBuffer.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = device->CreateShaderResourceView(particleEmitBuffer.Get(), nullptr, particleEmitShaderResourceView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = device->CreateUnorderedAccessView(particleEmitBuffer.Get(), nullptr, particleEmitUnordredAccessView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    //パーティクルの更新・描画数の削減のためのバッファ
    {
        D3D11_BUFFER_DESC desc{};
        desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        desc.ByteWidth = DrawIndirectSize;
        desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        desc.Usage = D3D11_USAGE_DEFAULT;
        D3D11_SUBRESOURCE_DATA initializeData{};
        std::vector<UINT> initializeBuffer(desc.ByteWidth / sizeof(UINT));
        {
            //初期値設定
            DrawIndirect* drawData = reinterpret_cast<DrawIndirect*>(initializeBuffer.data() + DrawIndirectOffset / sizeof(UINT));
            drawData->vertexCountPerInstance = numParticles;
            drawData->instanceCount = 1;
            drawData->startVertexLocation = 0;
            drawData->startInstanceLocation = 0;
        }

        //初期化しておく
        {
            UINT index = NumEmitPixelParticleIndirectOffset / sizeof(UINT);
            initializeBuffer[index] = 0;
        }

        initializeData.pSysMem = initializeBuffer.data();
        initializeData.SysMemPitch = desc.ByteWidth;
        hr = device->CreateBuffer(&desc, &initializeData, indirectDataBuffer.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        //Append/Consumeを利用する場合はビュー側にフラグを立てる
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = desc.ByteWidth / sizeof(UINT);
        uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        hr = device->CreateUnorderedAccessView(indirectDataBuffer.Get(), &uavDesc, indirectDataUnordredAccessView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    //コンピュートシェーダー読み込み
    hr = CreateCsFromCSO(device, "./Data/Shaders/ComputeParticleInitCS.cso", initShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/ComputeParticleEmitCS.cso", emitShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/ComputeParticleUpdateCS.cso", updateShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/ComputeParticleBeginFrameCS.cso", beginFrameShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/ComputeParticleEndFrameCS.cso", endFrameShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    hr = CreateCsFromCSO(device, "./Data/Shaders/ComputeParticleBitonicSortB2CS.cso", sortB2Shader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/ComputeParticleBitonicSortC2CS.cso", sortC2Shader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    //描画用情報生成
    this->shaderResourceView = shaderResourceView;
    hr = CreateVsFromCSO(device, "./Data/Shaders/ComputeParticleRenderVS.cso", vertexShader.GetAddressOf(), nullptr, nullptr, 0);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateGsFromCSO(device, "./Data/Shaders/ComputeParticleRenderGS.cso", geometryShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/ComputeParticleRenderPS.cso", pixelShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

CoreComputeParticleSystem::~CoreComputeParticleSystem()
{
    initShader.Reset();
    updateShader.Reset();
    emitShader.Reset();
    vertexShader.Reset();
    geometryShader.Reset();
    pixelShader.Reset();
    shaderResourceView.Reset();
}

void CoreComputeParticleSystem::Emit(const EmitParticleData& data)
{
    if (emitParticles.size() + pendingParticles.size() >= numEmitParticles)
        return;

    if (data.parameter.z <= 0) {
        //遅延なしなら直接追加
        emitParticles.emplace_back(data);
    }
    else {
        //保留リストに追加
        pendingParticles.emplace_back(data);
    }
}

void CoreComputeParticleSystem::PixelEmitBegin(ID3D11DeviceContext* immediateContext, float deltaTime)
{
    //定数バッファ設定
    {
        immediateContext->PSSetConstantBuffers(10, 1, commonConstantBuffer.GetAddressOf());
        immediateContext->CSSetConstantBuffers(10, 1, commonConstantBuffer.GetAddressOf());
        immediateContext->CSSetConstantBuffers(11, 1, bitonicSortConstantBuffer.GetAddressOf());

        //定数バッファ更新
        CommonConstants constant;
        constant.textureSplitCount = textureSplitCount;
        constant.systemNumParticles = numParticles;
        constant.totalEmitCount = static_cast<UINT>(emitParticles.size());
        constant.maxEmitParticles = numEmitParticles;
        immediateContext->UpdateSubresource(commonConstantBuffer.Get(), 0, nullptr, &constant, 0, 0);
    }

    //初期化処理
    if (!oneShotInitialize)
    {
        oneShotInitialize = true;
        immediateContext->CSSetShader(initShader.Get(), nullptr, 0);
        immediateContext->Dispatch(numParticles / NumParticleThread, 1, 1);
    }

    //ピクセルエミッターの処理
    ID3D11UnorderedAccessView* uavs[] =
    {
        particleEmitUnordredAccessView.Get(),
        indirectDataUnordredAccessView.Get(),
    };
    immediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr,
        0, ARRAYSIZE(uavs), uavs, nullptr);
}

void CoreComputeParticleSystem::PixelEmitEnd(ID3D11DeviceContext* immediateContext)
{
    immediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr,
        0, 0, nullptr, nullptr);
}

void CoreComputeParticleSystem::Update(ID3D11DeviceContext* immediateContext, float deltaTime)
{
    //保留リストのパーティクルの処理
    {
        for (size_t i = 0; i < pendingParticles.size();)
        {
            EmitParticleData& data = pendingParticles[i];
            data.parameter.z -= deltaTime;
            if (data.parameter.z <= 0.0f)
            {
                emitParticles.emplace_back(data);

                //最後の要素と交換して削除
                data = pendingParticles.back();
                pendingParticles.pop_back();
                //インデックスはそのまま（最後から持ってきた要素を再評価）
            }
            else {
                i++;
            }
        }

    }

    //定数バッファ設定
    {
        //定数バッファ更新
        CommonConstants constant;
        constant.textureSplitCount = textureSplitCount;
        constant.systemNumParticles = numParticles;
        constant.totalEmitCount = static_cast<UINT>(emitParticles.size());
        constant.maxEmitParticles = numEmitParticles;
        immediateContext->UpdateSubresource(commonConstantBuffer.Get(), 0, nullptr, &constant, 0, 0);

        immediateContext->CSSetConstantBuffers(10, 1, commonConstantBuffer.GetAddressOf());
        immediateContext->CSSetConstantBuffers(11, 1, bitonicSortConstantBuffer.GetAddressOf());

    }

    //SRV/UAV設定
    {
        immediateContext->CSSetShaderResources(0, 1, particleEmitShaderResourceView.GetAddressOf());

        //Append/Consumeバッファ初期化処理
        if (!oneShotInitialize)
        {
            UINT clearParameter = 0;
            immediateContext->CSSetUnorderedAccessViews(0, 1, particleAppendConsumeUnordredAccessView.GetAddressOf(), &clearParameter);
        }

        ID3D11UnorderedAccessView* uavs[] =
        {
            particleDataUnordredAccessView.Get(),
            particleAppendConsumeUnordredAccessView.Get(),
            indirectDataUnordredAccessView.Get(),
            particleHeaderUnordredAccessView.Get(),
        };
        immediateContext->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);
    }

    //初期化処理
    if (!oneShotInitialize)
    {
        oneShotInitialize = true;
        immediateContext->CSSetShader(initShader.Get(), nullptr, 0);
        immediateContext->Dispatch(numParticles / NumParticleThread, 1, 1);
    }

    //フレーム開始時の処理
    {
        //現在フレームでのパーティクル総数を算出
        //それに合わせて各種設定を行う
        immediateContext->CSSetShader(beginFrameShader.Get(), nullptr, 0);
        immediateContext->Dispatch(1, 1, 1);
    }

    //エミット処理
    {
        //エミットバッファ更新
        if (!emitParticles.empty())
        {
            D3D11_BOX writeBox{};
            writeBox.left = 0;
            writeBox.right = static_cast<UINT>(emitParticles.size() * sizeof(EmitParticleData));
            writeBox.top = 0;
            writeBox.bottom = 1;
            writeBox.front = 0;
            writeBox.back = 1;
            immediateContext->UpdateSubresource(particleEmitBuffer.Get(),
                0,
                &writeBox,
                emitParticles.data(),
                static_cast<UINT>(emitParticles.size() * sizeof(EmitParticleData)),
                0);
            emitParticles.clear();
        }

        immediateContext->CSSetShader(emitShader.Get(), nullptr, 0);
        immediateContext->DispatchIndirect(indirectDataBuffer.Get(), EmitDispatchIndirectOffset);
    }

    //更新処理
    {
        immediateContext->CSSetShader(updateShader.Get(), nullptr, 0);
        immediateContext->DispatchIndirect(indirectDataBuffer.Get(), UpdateDispatchIndirectOffset);
    }

    {
        //ソート処理
        //バイトニックソート
        float f_exponent = log2f(static_cast<float>(numParticles));
        UINT exponent = static_cast<UINT>(ceilf(f_exponent) + 0.5f);
        for (UINT i = 0; i < exponent; ++i)
        {
            immediateContext->CSSetShader(sortB2Shader.Get(), nullptr, 0);
            UINT increment = 1 << i;
            for (UINT j = 0; j < i + 1; ++j)
            {
                BitonicSortConstants constant;
                constant.increment = increment;
                constant.direction = 2 << i;
                immediateContext->UpdateSubresource(bitonicSortConstantBuffer.Get(), 0, nullptr, &constant, 0, 0);

                if (increment <= BitonicSortC2Thread)
                {
                    immediateContext->CSSetShader(sortC2Shader.Get(), nullptr, 0);
                    immediateContext->Dispatch(numParticles / 2 / BitonicSortC2Thread, 1, 1);
                    break;
                }

                immediateContext->Dispatch(numParticles / 2 / BitonicSortB2Thread, 1, 1);
                increment /= 2;
            }
        }
    }

    //フレーム終了時の処理
    {
        //総パーティクル数を変動させる
        immediateContext->CSSetShader(endFrameShader.Get(), nullptr, 0);
        immediateContext->Dispatch(1, 1, 1);
    }

    //NULLのUAV設定
    {
        ID3D11UnorderedAccessView* uavs[] = { nullptr, nullptr, nullptr, nullptr };
        immediateContext->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);
    }
}

void CoreComputeParticleSystem::Render(ID3D11DeviceContext* immediateContext)
{
    //点描画設定
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

    //シェーダー設定
    immediateContext->VSSetShader(vertexShader.Get(), nullptr, 0);
    immediateContext->GSSetShader(geometryShader.Get(), nullptr, 0);
    immediateContext->PSSetShader(pixelShader.Get(), nullptr, 0);

    //入力レイアウト設定
    immediateContext->IASetInputLayout(nullptr);

    //リソース設定
    immediateContext->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());
    immediateContext->GSSetShaderResources(0, 1, particleDataShaderResourceView.GetAddressOf());
    immediateContext->GSSetShaderResources(1, 1, particleHeaderShaderResourceView.GetAddressOf());

    //バッファクリア
    ID3D11Buffer* clearBuffer[] = { nullptr };
    UINT strides[] = { 0 };
    UINT offsets[] = { 0 };
    immediateContext->IASetVertexBuffers(0, 1, clearBuffer, strides, offsets);
    immediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);

    //GPU側で計算したパーティクルの描画数をCPU側で取得して描画コールを呼び出すのはもったいないので、
    // DrawIndirect命令を使ってGPU側だけで処理させる
    immediateContext->DrawInstancedIndirect(indirectDataBuffer.Get(), DrawIndirectOffset);

    //シェーダー無効化
    immediateContext->VSSetShader(nullptr, nullptr, 0);
    immediateContext->GSSetShader(nullptr, nullptr, 0);
    immediateContext->PSSetShader(nullptr, nullptr, 0);

    //リソースクリア
    ID3D11ShaderResourceView* clearShaderResourceViews[] = { nullptr, nullptr };
    immediateContext->PSSetShaderResources(0, 2, clearShaderResourceViews);
    immediateContext->GSSetShaderResources(0, 2, clearShaderResourceViews);
    immediateContext->CSSetShaderResources(0, 2, clearShaderResourceViews);
}

void CoreComputeParticleSystem::DrawGUI() const
{
#ifdef USE_IMGUI
    ImGui::Text("EmitParticles:%d", numEmitParticles);
    ImGui::Text("NumParticles:%d", numParticles);
#endif // USE_IMGUI
}
