#include "Game_engine_core.h"
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
        Game.LoadTexture(L"../../Textures/WoodCrate01.dds", "mat");
        Game.LoadTexture(L"../../Textures/white1x1.dds", "mat2");
        if (!Game.Initialize())
            return 0;
        GeometryGenerator gg;
        GeometryGenerator::MeshData sphere = gg.CreateSphere(0.3, 40, 40);
        GeometryGenerator::MeshData cube = gg.CreateBox(2, 2, 2, 2);
        GeometryGenerator::MeshData grid = gg.CreateGrid(20, 10, 2, 2);
        Game.CreateMaterial("mat", (XMFLOAT4)Colors::White, XMFLOAT3(0.f, 0.f, 0.f), 0.f, {2, 1, 1});
        Game.CreateMaterial("mat2", (XMFLOAT4)Colors::Red, XMFLOAT3(0.f, 0.f, 0.f), 10.f);
        Game.CreateGeometry(grid, XMMatrixTranslation(0, 0, 0), "mat", "cube");
        Game.CreateGeometry(sphere, XMMatrixTranslation(2, 0, 0), "mat2", "cube2");
        Game.CreateWorld();
        MSG msg = { 0 };
        mTimer.Reset();

        Position posx;
        Position pos;
        Position color;
        posx.set_pos(2, 0, 0);
        pos.set_pos(0, 2, 0);
        color.set_pos(1, 1, 1);

        Game.SetAmbient({ 0.3,0.3,0.4,1 });
        Game.SetLight({ 0.5,0.5,0.5 }, { 0.5,0.f,0.f });

        float speed = 7;

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
                if (Game.IsKeyPresed('Q')) {
                    pos.y -= speed * mTimer.DeltaTime();
                }
                if (Game.IsKeyPresed('E')) {
                    pos.y += speed * mTimer.DeltaTime();
                }
                if (Game.IsKeyPresed('W')) {
                    pos.z += speed * mTimer.DeltaTime();
                }
                if (Game.IsKeyPresed('A')) {
                    pos.x -= speed * mTimer.DeltaTime();
                }
                if (Game.IsKeyPresed('S')) {
                    pos.z -= speed * mTimer.DeltaTime();
                }
                if (Game.IsKeyPresed('D')) {
                    pos.x += speed * mTimer.DeltaTime();
                }
                if (Game.IsKeyPresed('R')) {
                    color.x += 0.01;
                }
                if (Game.IsKeyPresed('T')) {
                    color.x -= 0.01;
                }
                if (Game.IsKeyPresed('G')) {
                    color.y += 0.01;
                }
                if (Game.IsKeyPresed('H')) {
                    color.y -= 0.01;
                }
                if (Game.IsKeyPresed('B')) {
                    color.z += 0.01;
                }
                if (Game.IsKeyPresed('N')) {
                    color.z -= 0.01;
                }
                Game.EditLight({ pos.x, pos.y, pos.z }, { 0.5,0.f,0.f }, 0);
                Game.UpdateMaterial("mat", XMFLOAT4(color.x, color.y, color.z, 1), XMFLOAT3(0.1, 0.1f, 0.1f), 0.04f, XMFLOAT3(2, 2, 2));
                Game.MoveObject("cube2", pos.get_pos());
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
