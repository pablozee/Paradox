#pragma once

#include "SkinnedData.h"

class M3DLoader
{
public:

	struct Vertex
	{
		XMFLOAT3 Pos;
		XMFLOAT3 Normal;
		XMFLOAT2 TexC;
		XMFLOAT4 TangentU;
	};

	struct SkinnedVertex 
	{
		XMFLOAT3 Pos;
		XMFLOAT3 Normal;
		XMFLOAT2 TexC;
		XMFLOAT3 TangentU;
		XMFLOAT3 BoneWeights;
		BYTE BoneIndices[4];
	};

	struct Subset
	{
		UINT Id = -1;
		UINT VertexStart = 0;
		UINT VertexCount = 0;
		UINT FaceStart = 0;
		UINT FaceCount = 0;
	};

	struct M3dMaterial
	{
		string Name;
		XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
		float Roughness = 0.8f;
		bool AlphaClip = false;

		string MaterialTypeName;
		string DiffuseMapName;
		string NormalMapName;

		bool LoadM3d(
			const string& filename,
			vector<Vertex>& vertices,
			vector<USHORT>& indices,
			vector<Subset>& subsets,
			vector<M3dMaterial>& mats
		);

		bool LoadM3d(
			const string& filename,
			vector<Vertex>& vertices,
			vector<USHORT>& indices,
			vector<Subset>& subsets,
			vector<M3dMaterial>& mats,
			SkinnedData& skinInfo
		);

	private:

		void ReadMaterials(ifstream& fin, UINT numMaterials, vector<M3dMaterial>& mats);
		void ReadSubsetTable(ifstream& fin, UINT numSubsets, vector<Subset>& subsets);
		void ReadVertices(ifstream& fin, UINT numVertices, vector<Vertex>& vertices);
		void ReadSkinnedVertices(ifstream& fin, UINT numVertices, vector<SkinnedVertex>& vertices);
		void ReadTriangles(ifstream& fin, UINT numTriangles, vector<USHORT>& indices);
		void ReadBoneOffsets(ifstream& fin, UINT numBones, vector<XMFLOAT4X4>& boneOffsets);
		void ReadBoneHierarchy(ifstream& fin, UINT numBones, vector<int>& boneIndexToParentIndex);
		void ReadAnimationClips(ifstream& fin, UINT numBones, UINT numAnimationClips, unordered_map<string, AnimationClip>& animations);
		void ReadBoneKeyframes(ifstream& fin, UINT numBones, BoneAnimation& boneAnimation);

	};
};