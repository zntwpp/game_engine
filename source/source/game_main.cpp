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
        Game.LoadTexture(L"../../Textures/white.dds", "tex");
        if (!Game.Initialize())
            return 0;

        GeometryGenerator gg;
        ObjLoader loader;
        Mesh msh = loader.LoadObj("../../Models/monkey.obj");
        Game.CreateMaterial("mat", (XMFLOAT4)Colors::Gray, (XMFLOAT3)Colors::White, 0.02);
        Game.CreateGeometry(msh, XMMatrixTranslation(0,0,0), "mat", "obj");
        Game.CreateWorld();
        MSG msg = { 0 };
        mTimer.Reset();

        Game.CameraWalk(-4);
        Game.SetAmbient(XMFLOAT4(0.4,0.4,0.6,1));
        Game.SetLight(XMFLOAT3(0.7, -0.5, 0.4), XMFLOAT3(0.6,0.5,0.5));
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
                Game.MoveObject("obj", XMMatrixRotationY(mTimer.TotalTime()));
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
