//game engine core
#include "Game_engine_core.h"

const int gNumFrameResources = 3;

Game_engine::Game_engine(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
    if (!D3DApp::Initialize())
        abort();

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}

Game_engine::~Game_engine()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();
}

bool Game_engine::Initialize()
{

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 3;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    hDescriptor = mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    BuildRootSignature();
    BuildShadersAndInputLayout();

    return true;
}

void Game_engine::OnResize()
{
    D3DApp::OnResize();

    mCam.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

    BoundingFrustum::CreateFromMatrix(mCamFrustum, mCam.GetProj());
}

void Game_engine::Update(const GameTimer& gt)
{

    OnKeyboardInput(gt);
    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
    UpdateMaterialCBs(gt);
    UpdateObjectCBs(gt);
    UpdateMainPassCB(gt);
}

void Game_engine::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    if (mIsWireframe)
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
    }

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
  
    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    mCommandList->SetGraphicsRootConstantBufferView(2, mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void Game_engine::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void Game_engine::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void Game_engine::OnMouseMove(WPARAM btnState, int x, int y)
{
    if (basic_camera_control) {
        if ((btnState & MK_LBUTTON) != 0)
        {
            // Make each pixel correspond to a quarter of a degree.
            float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
            float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

            mCam.Pitch(dy);
            mCam.RotateY(dx);
        }

        mLastMousePos.x = x;
        mLastMousePos.y = y;
    }
}

void Game_engine::OnKeyboardInput(const GameTimer& gt)
{
    if (GetAsyncKeyState('1') & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;
    switch (basic_camera_control)
    {
    case(1):
        if (GetAsyncKeyState('W') & 0x8000)
            mCam.Walk(10.0f * gt.DeltaTime());
        if (GetAsyncKeyState('S') & 0x8000)
            mCam.Walk(-10.0f * gt.DeltaTime());
        if (GetAsyncKeyState('A') & 0x8000)
            mCam.Strafe(-10.0f * gt.DeltaTime());
        if (GetAsyncKeyState('D') & 0x8000)
            mCam.Strafe(10.0f * gt.DeltaTime());

        mCam.UpdateViewMatrix();
    default:
        mCam.UpdateViewMatrix();
        break;
    }
}

bool Game_engine::IsKeyPresed(char key) {
    if (GetAsyncKeyState(key)) {
        return 1;
    }

    else return 0;
}
/*
* comming soon (maybe)
void Game_engine::RotateObject(std::string name, XMMATRIX rotation)
{
    XMStoreFloat4x4(&mOpaqueRitems[names[name]]->World, rotation);
}*/

void Game_engine::CreateMaterial(std::string name, XMFLOAT4 difuse_albedo, XMFLOAT3 FresnelR0, float Roughnes, std::string tex_name, XMFLOAT3 MatTransform)
{
    auto mat = std::make_unique<Material>();
    mat->Name = name;
    mat->DiffuseAlbedo = difuse_albedo;
    mat->FresnelR0 = FresnelR0;
    mat->Roughness = Roughnes;
    XMStoreFloat4x4(&mat->MatTransform, XMMatrixScaling(MatTransform.x, MatTransform.y, MatTransform.z));
    mat->MatCBIndex = mat_CBI_index;
    mat->DiffuseSrvHeapIndex = mat_CBI_index;
    mat->NormalSrvHeapIndex = mat_CBI_index;
    ++mat_CBI_index;
    mMaterials[name] = std::move(mat);

    auto tex = mTextures[tex_name]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = tex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
}

void Game_engine::UpdateMaterial(std::string name, XMFLOAT4 difuse_albedo, XMFLOAT3 FresnelR0, float Roughnes, XMFLOAT3 MatTransform)
{
    mMaterials[name]->DiffuseAlbedo = difuse_albedo;
    mMaterials[name]->FresnelR0 = FresnelR0;
    mMaterials[name]->Roughness = Roughnes;
    XMStoreFloat4x4(&mMaterials[name]->MatTransform, XMMatrixScaling(MatTransform.x, MatTransform.y, MatTransform.z));
}

std::vector<XMFLOAT3> Game_engine::GetVertices(std::string name)
{
    return verts[name];
}

void Game_engine::LoadTexture(std::wstring filepath, std::string name)
{
    auto tex = std::make_unique<Texture>();
    tex->Name = name;
    tex->Filename = filepath;
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), tex->Filename.c_str(),
        tex->Resource, tex->UploadHeap));
    mTextures[name] = std::move(tex);
    tex_names.push_back(name);
}

//Light
void Game_engine::SetLight(DirectX::XMFLOAT3 pos_dir, DirectX::XMFLOAT3 strength)
{
    set_light(pos_dir, strength);
}

void Game_engine::SetAmbient(DirectX::XMFLOAT4 amb)
{
    set_ambient(amb);
}

void Game_engine::EditLight(DirectX::XMFLOAT3 pos_dir, DirectX::XMFLOAT3 strength, unsigned int index)
{
    edit_light(pos_dir, strength, index);
}

void Game_engine::EditAmbient(DirectX::XMFLOAT4 amb)
{
    edit_ambient(amb);
}

//Camera
void Game_engine::SetCameraPos(DirectX::XMFLOAT3 pos)
{
    mCam.SetPosition(pos);
    mCam.UpdateViewMatrix();
}

DirectX::XMFLOAT3 Game_engine::GetCameraPos()
{
    return mCam.GetPosition3f();
}

void Game_engine::CameraWalk(float d)
{
    mCam.Walk(d);
    mCam.UpdateViewMatrix();
}

void Game_engine::CameraStrafe(float d)
{
    mCam.Strafe(d);
    mCam.UpdateViewMatrix();
}

void Game_engine::CameraPitch(float angle)
{
    mCam.Pitch(angle);
    mCam.UpdateViewMatrix();
}

void Game_engine::CameraRotateY(float angle)
{
    mCam.RotateY(angle);
    mCam.UpdateViewMatrix();
}

void Game_engine::CameraLookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp)
{
    mCam.LookAt(pos, target, worldUp);
    mCam.UpdateViewMatrix();
}

void Game_engine::CameraLookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up)
{
    mCam.LookAt(pos, target, up);
}

void Game_engine::CameraSetLens(float fovY, float aspect, float zn, float zf)
{
    mCam.SetLens(fovY, aspect, zn, zf);
}

void Game_engine::DeleteBaseCamControl()
{
    basic_camera_control = 0;
}

void Game_engine::UseBaseCamControl()
{
    basic_camera_control = 0;
}

void Game_engine::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {      
        ObjectConstants objConstants;
        XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(XMLoadFloat4x4(&e->World)));
        XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&e->TexTransform)));
        
        currObjectCB->CopyData(e->ObjCBIndex, objConstants);           
        
    }
}


void Game_engine::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = mCam.GetView();
    XMMATRIX proj = mCam.GetProj();

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    mMainPassCB.EyePosW = mCam.GetPosition3f();
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();
    
    set_lights(&mMainPassCB);

    mCurrFrameResource->PassCB.get()->CopyData(0, mMainPassCB);
}

void Game_engine::UpdateMaterialCBs(const GameTimer& gt)
{
    auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
    for (auto& e : mMaterials)
    {
        // Only update the cbuffer data if the constants have changed.  If the cbuffer
        // data changes, it needs to be updated for each FrameResource.
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
            matConstants.FresnelR0 = mat->FresnelR0;
            matConstants.Roughness = mat->Roughness;

            XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(XMLoadFloat4x4(&mat->MatTransform)));

            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

            // Next FrameResource need to be updated too.
            mat->NumFramesDirty--;
        }
    }
}
/*
void Game_engine::BuildDescriptorHeaps()
{
    //
    // Create the SRV heap.
    //
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 3;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    //
    // Fill out the heap with actual descriptors.
    //
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    auto firstTex = mTextures[tex_names[0]]->Resource;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = firstTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = firstTex->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    md3dDevice->CreateShaderResourceView(firstTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    for (int i = 1; i < tex_names.size(); ++i) {
        hDescriptor.Offset(1, mCbvSrvDescriptorSize);

        srvDesc.Format = mTextures[tex_names[i]]->Resource->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = mTextures[tex_names[i]]->Resource->GetDesc().MipLevels;
        md3dDevice->CreateShaderResourceView(mTextures[tex_names[i]]->Resource.Get(), &srvDesc, hDescriptor);
    }
}
*/

void Game_engine::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        1,  // number of descriptors
        0); // register t0

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0); // register b0
    slotRootParameter[2].InitAsConstantBufferView(1); // register b1
    slotRootParameter[3].InitAsConstantBufferView(2); // register b2

    auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void Game_engine::BuildShadersAndInputLayout()
{
    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "ALPHA_TEST", "1",
        NULL, NULL
    };

    mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void Game_engine::CreateGeometry(Mesh mesh, XMFLOAT3 pos, std::string mat_name, std::string name)
{

    SubmeshGeometry objSubmesh;
    objSubmesh.IndexCount = (UINT)mesh.indices.size();
    objSubmesh.StartIndexLocation = 0;
    objSubmesh.BaseVertexLocation = 0;

    std::vector<Vertex> vertices(mesh.vertices.size());

    XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
    XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

    XMVECTOR vMin = XMLoadFloat3(&vMinf3);
    XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

    for (size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        vertices[i].Pos = mesh.vertices[i].Pos;
        vertices[i].Normal = mesh.vertices[i].Normal;
        vertices[i].TexC = mesh.vertices[i].TexC;

        verts[name].push_back(vertices[i].Pos);

        XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }

    BoundingBox bounds;
    XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
    XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));

    objSubmesh.Bounds = bounds;

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(mesh.indices), std::end(mesh.indices));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = name;

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs[name] = objSubmesh;

    mGeometries[geo->Name] = std::move(geo);
    ++CBI_index;
    Material mat_return;
    if (mMaterials.count(mat_name)) {
        mat_return = *mMaterials[mat_name].get();
    }
    BuildRenderItems(XMMatrixTranslation(pos.x, pos.y, pos.z), name, mat_return);
}

void Game_engine::CreateGeometry(GeometryGenerator::MeshData obj, XMFLOAT3 pos, std::string mat_name, std::string name)
{
    GeometryGenerator::MeshData object = obj;

    SubmeshGeometry objSubmesh;
    objSubmesh.IndexCount = (UINT)object.Indices32.size();
    objSubmesh.StartIndexLocation = 0;
    objSubmesh.BaseVertexLocation = 0;

    std::vector<Vertex> vertices(object.Vertices.size());

    XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
    XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

    XMVECTOR vMin = XMLoadFloat3(&vMinf3);
    XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

    for (size_t i = 0; i < object.Vertices.size(); ++i)
    {
        vertices[i].Pos = object.Vertices[i].Position;
        vertices[i].Normal = object.Vertices[i].Normal;
        vertices[i].TexC = object.Vertices[i].TexC;

        verts[name].push_back(vertices[i].Pos);

        XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }

    BoundingBox bounds;
    XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
    XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));

    objSubmesh.Bounds = bounds;

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(object.GetIndices16()), std::end(object.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = name;

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs[name] = objSubmesh;

    mGeometries[geo->Name] = std::move(geo);
    ++CBI_index;
    Material mat_return;
    if (mMaterials.count(mat_name)) {
        mat_return = *mMaterials[mat_name].get();
    }
    BuildRenderItems(XMMatrixTranslation(pos.x, pos.y, pos.z), name, mat_return);
}

void Game_engine::CreateWorld()
{
    for (auto& e : mAllRitems)
        mOpaqueRitems.push_back(e.get());
    BuildFrameResources();
    //BuildDescriptorHeaps();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();
}

void Game_engine::MoveObject(std::string name, XMMATRIX pos) {
    XMStoreFloat4x4(&mOpaqueRitems[names[name]]->World, pos);
}

void Game_engine::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    //
    // PSO for opaque objects.
    //
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };

    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


    //
    // PSO for opaque wireframe objects.
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
    opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}

void Game_engine::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
    }
}

void Game_engine::BuildRenderItems(XMMATRIX pos, std::string name, Material mat)
{
    auto objRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&objRitem->World, pos);
    objRitem->TexTransform = mat.MatTransform;
    objRitem->ObjCBIndex = CBI_index;
    objRitem->Geo = mGeometries[name].get();
    objRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    objRitem->IndexCount = objRitem->Geo->DrawArgs[name].IndexCount;
    objRitem->StartIndexLocation = objRitem->Geo->DrawArgs[name].StartIndexLocation;
    objRitem->BaseVertexLocation = objRitem->Geo->DrawArgs[name].BaseVertexLocation;
    objRitem->Mat = &mat;
    objRitem->Bounds = objRitem->Geo->DrawArgs[name].Bounds;
    mat_cbis.push_back(mat.MatCBIndex);
    mAllRitems.push_back(std::move(objRitem));
    names[name] = CBI_index;
    visible_objects.push_back(1);
}

void Game_engine::DrawObject(std::string name) {
    visible_objects[names[name]] = 1;
}
void Game_engine::DoNotDrawObject(std::string name) {
    visible_objects[names[name]] = 0;
}

void Game_engine::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

    auto objectCB = mCurrFrameResource->ObjectCB->Resource();
    auto matCB = mCurrFrameResource->MaterialCB->Resource();

    XMMATRIX view = mCam.GetView();
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

    // For each render item...
    for (size_t i = 0; i < ritems.size(); ++i)
    {
        if (visible_objects[i]) {
            auto ri = ritems[i];

            //frustum vars
            XMMATRIX world = XMLoadFloat4x4(&ri->World);
            //XMMATRIX texTransform = XMLoadFloat4x4(&ri->TexTransform);

            XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);

            // View space to the object's local space.
            XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);

            // Transform the camera frustum from view space to the object's local space.
            BoundingFrustum localSpaceFrustum;
            mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

            if (localSpaceFrustum.Contains(ri->Bounds) != DirectX::DISJOINT) {

                cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
                cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
                cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

                CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
                tex.Offset(mat_cbis[i], mCbvSrvDescriptorSize);

                D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
                D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + mat_cbis[i] * matCBByteSize;

                cmdList->SetGraphicsRootDescriptorTable(0, tex);
                cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
                cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

                cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
            }
        }
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Game_engine::GetStaticSamplers()
{
    // Applications usually only need a handful of samplers.  So just define them all up front
    // and keep them available as part of the root signature.  

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}