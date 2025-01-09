#include "Game_engine_core.h"
#include "Collider.h"
#include <random>
#include <fstream>

GameTimer mTimer;

void update(Game_engine& gm) {
    mTimer.Tick();
    gm.Update(mTimer);
    gm.Draw(mTimer);
}

float randfloat(float min, float max) {
    std::random_device rd;  // Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<> dis(min, max);
    return dis(gen);
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
        Game.LoadTexture(L"../../Textures/tile.dds", "floor_tex");
        Game.LoadTexture(L"../../Textures/white1x1.dds", "cubes_tex");
        if (!Game.Initialize())
            return 0;
        GeometryGenerator gg;
        GeometryGenerator::MeshData floor = gg.CreateBox(4, 1, 20, 2);
        GeometryGenerator::MeshData cube = gg.CreateCylinder(0.7, 0.7, 2, 24, 2);
        GeometryGenerator::MeshData player = gg.CreateBox(0.3,2,0.1,2);
        Game.CreateMaterial("floor_mat", (XMFLOAT4)Colors::White, XMFLOAT3(0.f, 0.f, 0.f), 0.f, XMFLOAT3(4,20,1));
        Game.CreateMaterial("cubes_mat", (XMFLOAT4)Colors::Red, XMFLOAT3(0.f, 0.f, 0.f), 10.f);
        Game.CreateMaterial("player_mat", (XMFLOAT4)Colors::White, XMFLOAT3(0.f, 0.f, 0.f), 0);
        Game.CreateGeometry(floor, XMMatrixTranslation(0, -1, 7), "floor_mat", "floor_obj");
        Game.CreateGeometry(cube, XMMatrixTranslation(0, 0, 0), "cubes_mat", "cube1");
        Game.CreateGeometry(cube, XMMatrixTranslation(0, 0, 0), "cubes_mat", "cube2");
        Game.CreateGeometry(player, XMMatrixTranslation(0, 0, 0), "player_mat", "player");
        Game.CreateWorld();
        Collider player_col(Game.GetVertices("player"));
        Collider frst_cube_col(Game.GetVertices("cube1"));
        Collider scnd_cube_col(Game.GetVertices("cube2"));
        MSG msg = { 0 };
        mTimer.Reset();
        Game.EnableFrustumCulling();

        Game.SetAmbient(XMFLOAT4(0.1,0.1,0.2,1));
        Game.SetLight(XMFLOAT3(0.7, -1, 0.4), XMFLOAT3(0.1,0.1,0.2));

        int record = 0;
        float speed = 4;
        float cam_speed = 5;

        Position frst_cube_pos;
        Position scnd_cube_pos;
        frst_cube_pos.set_pos(randfloat(-2, 2), 0.5, 15);
        scnd_cube_pos.set_pos(randfloat(-2, 2), 0.5, 15);
        Game.SetLight(XMFLOAT3(frst_cube_pos.x, frst_cube_pos.y, frst_cube_pos.z), (XMFLOAT3)Colors::Red);
        Game.SetLight(XMFLOAT3(scnd_cube_pos.x, scnd_cube_pos.y, scnd_cube_pos.z), (XMFLOAT3)Colors::Red);
        Game.MoveObject("cube1", frst_cube_pos.get_pos());
        Game.MoveObject("cube2", scnd_cube_pos.get_pos());
        Game.DeleteBaseCamControl();

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
                if (Game.IsKeyPresed(VK_RIGHT) && Game.GetCameraPos().x < 2) {
                    Game.CameraStrafe(cam_speed * mTimer.DeltaTime());
                }
                if (Game.IsKeyPresed(VK_LEFT) && Game.GetCameraPos().x > -2) {
                    Game.CameraStrafe(-cam_speed * mTimer.DeltaTime());
                }

                if (player_col.is_intersect(&frst_cube_col, XMFLOAT3(Game.GetCameraPos().x, Game.GetCameraPos().y, Game.GetCameraPos().z), XMFLOAT3(frst_cube_pos.x, frst_cube_pos.y, frst_cube_pos.z))) {
                    std::ofstream out;
                    out.open("record.txt", std::ios::out);
                    if (out.is_open()) {
                        out << "your record is " << record;
                    }
                    return 0;
                }
                else if (player_col.is_intersect(&scnd_cube_col, XMFLOAT3(Game.GetCameraPos().x, Game.GetCameraPos().y, Game.GetCameraPos().z), XMFLOAT3(scnd_cube_pos.x, scnd_cube_pos.y, scnd_cube_pos.z))) {
                    std::ofstream out;
                    out.open("record.txt", std::ios::out);
                    if (out.is_open()) {
                        out << "your record is " << record;
                    }
                    return 0;
                }
                Game.MoveObject("player", XMMatrixTranslation(Game.GetCameraPos().x, Game.GetCameraPos().y,Game.GetCameraPos().z));
                Game.MoveObject("cube1", frst_cube_pos.get_pos());
                Game.MoveObject("cube2", scnd_cube_pos.get_pos());
                frst_cube_pos.z -= speed * mTimer.DeltaTime();
                scnd_cube_pos.z -= speed * mTimer.DeltaTime();
                if (frst_cube_pos.z <= 0) {
                    frst_cube_pos.z = 15;
                    scnd_cube_pos.z = 15;
                    frst_cube_pos.x = randfloat(-2,2);
                    scnd_cube_pos.x = randfloat(-2,2);
                    speed += 0.5;
                    ++record;
                }
                Game.EditLight(XMFLOAT3(frst_cube_pos.x, frst_cube_pos.y, frst_cube_pos.z), (XMFLOAT3)Colors::Red, 1);
                Game.EditLight(XMFLOAT3(scnd_cube_pos.x, scnd_cube_pos.y, scnd_cube_pos.z), (XMFLOAT3)Colors::Red, 2);
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
