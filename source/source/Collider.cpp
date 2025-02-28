#include "Collider.h"

Collider::Collider(std::vector<XMFLOAT3> vert)
{
    m_vert = vert;
    for (int i = 0; i < vert.size(); ++i) {
        if (vert[i].x > size_x) {
            size_x = vert[i].x;
        }
        if (vert[i].y > size_y) {
            size_y = vert[i].y;
        }
        if (vert[i].z > size_z) {
            size_z = vert[i].z;
        }
    }
}


bool Collider::is_intersect(Collider* msh, XMFLOAT3 frst_pos, XMFLOAT3 scnd_pos)
{   
    // delete this shit if it doesnot working
    if (abs(frst_pos.x - scnd_pos.x) <= msh->size_x + size_x) {
        if (abs(frst_pos.y - scnd_pos.y) <= msh->size_y + size_y) {
            if (abs(frst_pos.z - scnd_pos.z) <= msh->size_z + size_z) {
                return 1;
            }
        }
    }
    return 0;
}
