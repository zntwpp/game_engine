#include "Game_engine_core.h"
#include "Collider.h"
#include "ObjLoader.h"

GameTimer mTimer;

void update(Game_engine& gm) {
    mTimer.Tick();
    gm.Update(mTimer);
    gm.Draw(mTimer);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    try
    {
        Game_engine Game(hInstance);
        Game.LoadTexture(L"../../Textures/white.dds", "white");
        Game.LoadTexture(L"../../Textures/stone.dds", "stone");
        if (!Game.Initialize())
            return 0;

        ObjLoader loader;
        Mesh msh = loader.LoadObj("../../Models/cat.obj");
        Mesh msh2 = loader.LoadObj("../../Models/monkey.obj");
        loader.get_error();
        Game.CreateMaterial("mat", (XMFLOAT4)Colors::Gold, (XMFLOAT3)Colors::White, 0.02f, "white");
        Game.CreateMaterial("mat2", (XMFLOAT4)Colors::White, (XMFLOAT3)Colors::White, 0.02f, "stone");

        Game.CreateGeometry(msh, XMFLOAT3(0, 0, 0), "mat", "cat");
        Game.CreateGeometry(msh2, XMFLOAT3(3, 0, 0), "mat2", "monkey");
        Game.CreateWorld();
        MSG msg = { 0 };
        mTimer.Reset();

        Game.CameraWalk(-4);
        Game.SetAmbient(XMFLOAT4(0.4f,0.4f,0.6f,1.f));
        Game.SetLight(XMFLOAT3(0.7f, -0.5f, 0.4f), XMFLOAT3(0.6f,0.5f,0.5f));
        //Game.DoNotDrawObject("monkey");
        while (msg.message != WM_QUIT)
        {
            // If there are Window messages then process them.
            if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            // Otherwise, do animation/game stuff.
            else
            {
                //chip
                Game.MoveObject("cat", XMMatrixRotationY(mTimer.TotalTime()));
                update(Game);
            }
        }

        return (int)msg.wParam;
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}
