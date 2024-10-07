#include "Lighting.h"

void Light_c::set_lights(PassConstants* pc){
	for (short i = 0; i < count_light; ++i) {
		pc->Lights[i].Direction = pos_dir_m[i];
		pc->Lights[i].Position = pos_dir_m[i];
		pc->Lights[i].Strength = strength_m[i];
		pc->AmbientLight = ambient_m;
	}
}

void Light_c::set_light(DirectX::XMFLOAT3 pos_dir, DirectX::XMFLOAT3 strength)
{
	pos_dir_m.push_back(pos_dir);
	strength_m.push_back(strength);
	++count_light;
}

void Light_c::set_ambient(DirectX::XMFLOAT4 amb)
{
	ambient_m = amb;
	++count_light;
}

void Light_c::edit_light(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 str, unsigned int index)
{
	pos_dir_m[index] = pos;
	strength_m[index] = str;
}

void Light_c::edit_ambient(DirectX::XMFLOAT4 amb)
{
	ambient_m = amb;
}
