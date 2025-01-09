#pragma once
#include <vector>
#include <DirectXMath.h>

using namespace DirectX;

class Collider {
public:
	Collider(std::vector<XMFLOAT3> vert);

	bool is_intersect(Collider* msh, XMFLOAT3 frst_pos, XMFLOAT3 scnd_pos);

	float size_x = 0;
	float size_y = 0;
	float size_z = 0;

private:
	std::vector<XMFLOAT3> m_vert;
};