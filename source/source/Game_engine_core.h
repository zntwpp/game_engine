#pragma once
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include <DirectXCollision.h>
#include "Lighting.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Position {
    float x;
    float y;
    float z;
    void set_pos_to_null() {
        x = 0;
        y = 0;
        z = 0;
    }
    void set_pos(float in_x, float in_y, float in_z) {
        x = in_x;
        y = in_y;
        z = in_z;
    }
    XMMATRIX get_pos(float x_offset = 0, float y_offset = 0, float z_offset = 0) {
        return XMMatrixTranslation(x + x_offset, y + y_offset, z + z_offset);
    }
};
struct Rotation {
    float x;
    float y;
    float z;

    void set_rotation(float in_x, float in_y, float in_z) {
        x = in_x;
        y = in_y;
        z = in_z;
    }
    XMMATRIX get_rotation(float x_offset = 0, float y_offset = 0, float z_offset = 0) {
        XMMATRIX res;
        res = XMMatrixRotationRollPitchYaw(x, y, z);
        return res;
    }
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
    RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

    // Dirty flag indicating the object data has changed and we need to update the constant buffer.
    // Because we have an object cbuffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify obect data we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    Material* Mat = nullptr;
    MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class Game_engine : public D3DApp, public Light_c
{
public:
    Game_engine(HINSTANCE hInstance);
    Game_engine(const Game_engine& rhs) = delete;
    Game_engine& operator=(const Game_engine& rhs) = delete;
    ~Game_engine();
    virtual bool Initialize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;
public:
    //Game engine interface
    void CreateGeometry(GeometryGenerator::MeshData obj, XMMATRIX pos, std::string mat_name, std::string name);
    void CreateWorld();
    void MoveObject(std::string name, XMMATRIX pos);
    void DrawObject(std::string name);
    void DoNotDrawObject(std::string name);
    bool IsKeyPresed(char key);
    void RotateObject(std::string name, XMMATRIX rotation);
    void CreateMaterial(std::string name, XMFLOAT4 difuse_albedo, XMFLOAT3 FresnelR0, float Roughnes, XMFLOAT3 matTransform = XMFLOAT3(1, 1, 1));
    void UpdateMaterial(std::string name, XMFLOAT4 difuse_albedo, XMFLOAT3 FresnelR0, float Roughnes, XMFLOAT3 matTransform = XMFLOAT3(1, 1, 1));
    void LoadTexture(std::wstring filepath, std::string name);
    void SetLight(DirectX::XMFLOAT3 pos_dir, DirectX::XMFLOAT3 strength);
    void SetAmbient(DirectX::XMFLOAT4);
    void EditLight(DirectX::XMFLOAT3 pos_dir, DirectX::XMFLOAT3 strength, unsigned int index);
    void EditAmbient(DirectX::XMFLOAT4);
private:
    virtual void OnResize()override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
    void UpdateCamera(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);

    void BuildDescriptorHeaps();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildRenderItems(XMMATRIX pos, std::string name, Material mat);
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
    bool mDraw_all = 0;
    int CBI_index = -1;
    int mat_CBI_index = 0;
    std::unordered_map<std::string, int> names;
    std::vector<std::string> tex_names;
    std::vector<int> mat_cbis;
    std::vector<bool> visible_objects;

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

    UINT mCbvSrvDescriptorSize = 0;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    // List of all the render items.
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;

    // Render items divided by PSO.
    std::vector<RenderItem*> mOpaqueRitems;

    PassConstants mMainPassCB;

    UINT mPassCbvOffset = 0;

    bool mIsWireframe = false;

    XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f * XM_PI;
    float mPhi = 0.2f * XM_PI;
    float mRadius = 15.0f;

    POINT mLastMousePos;
};