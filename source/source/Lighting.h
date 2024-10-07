#pragma once
#include "../../Common/d3dApp.h"
#include "FrameResource.h"

class Light_c {
protected:
	void set_lights(PassConstants* pc);
	void set_light(DirectX::XMFLOAT3, DirectX::XMFLOAT3);
	void set_ambient(DirectX::XMFLOAT4);
	void edit_light(DirectX::XMFLOAT3, DirectX::XMFLOAT3, unsigned int index);
	void edit_ambient(DirectX::XMFLOAT4);
protected:
	int count_light = -1;

	std::vector<DirectX::XMFLOAT3> pos_dir_m;
	std::vector<DirectX::XMFLOAT3> strength_m;
	DirectX::XMFLOAT4 ambient_m;
};