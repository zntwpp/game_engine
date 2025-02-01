#pragma once
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <DirectXMath.h>
#include "FrameResource.h"

class ObjLoader {
public:
    Mesh LoadObj(const std::string& pFile) {
        // Create an instance of the Importer class
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(pFile, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

        // If the import failed, report it
        if (nullptr == scene) {
            m_err = importer.GetErrorString();
            return Mesh();
        }

        return processNode(scene->mRootNode->mChildren[0], scene);
    }
    std::string get_error() {
        return m_err;
    }

private:
    std::string m_err = "";
    std::vector<Vertex> mVertexes;
    Mesh processNode(aiNode* node, const aiScene* scene)
    {
        // process each mesh located at the current node
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // the node object only contains indices to index the actual objects in the scene. 
            // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

            // walk through each of the mesh's vertices
            for (unsigned int i = 0; i < mesh->mNumVertices; i++)
            {
                Vertex vertex;
                DirectX::XMFLOAT3 vector;
                // positions
                vector.x = mesh->mVertices[i].x;
                vector.y = mesh->mVertices[i].y;
                vector.z = mesh->mVertices[i].z;
                vertex.Pos = vector;
                // normals
                if (mesh->HasNormals())
                {
                    vector.x = mesh->mNormals[i].x;
                    vector.y = mesh->mNormals[i].y;
                    vector.z = mesh->mNormals[i].z;
                    vertex.Normal = vector;
                }

                if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
                {
                    DirectX::XMFLOAT2 vec;
                    vec.x = mesh->mTextureCoords[0][i].x;
                    vec.y = mesh->mTextureCoords[0][i].y;
                    vertex.TexC = vec;
                }

                vertices.push_back(vertex);
            }
            // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
            for (unsigned int i = 0; i < mesh->mNumFaces; i++)
            {
                aiFace face = mesh->mFaces[i];
                // retrieve all indices of the face and store them in the indices vector
                for (unsigned int j = 0; j < face.mNumIndices; j++)
                    indices.push_back(face.mIndices[j]);
            }
            
        }
        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        Mesh m_mesh;
        m_mesh.vertices = vertices;
        m_mesh.indices = indices;
        return m_mesh;

    }
};