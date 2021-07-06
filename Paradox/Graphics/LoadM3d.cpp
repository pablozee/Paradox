#include <fstream>
#include "LoadM3d.h"


bool M3DLoader::LoadM3d(const string& filename, vector<Vertex>& vertices, vector<USHORT>& indices, vector<Subset>& subsets, vector<M3dMaterial>& mats)
{
	std::ifstream fin(filename);

	UINT numMaterials = 0;
	UINT numVertices = 0;
	UINT numTriangles = 0;
	UINT numBones = 0;
	UINT numAnimationClips = 0;

	string ignore;

	if (fin)
	{
		fin >> ignore; // file header text
		fin >> ignore >> numMaterials;
		fin >> ignore >> numVertices;
		fin >> ignore >> numTriangles;
		fin >> ignore >> numBones;
		fin >> ignore >> numAnimationClips;

		ReadMaterials(fin, numMaterials, mats);
		ReadSubsetTable(fin, numMaterials, subsets);
		ReadVertices(fin, numVertices, vertices);
		ReadTriangles(fin, numTriangles, indices);

		return true;
	}
	return false;
}

bool M3DLoader::LoadM3d(const string& filename, vector<SkinnedVertex>& vertices, vector<USHORT>& indices, vector<Subset>& subsets, vector<M3dMaterial>& mats, SkinnedData& skinInfo)
{
	std::ifstream fin(filename);

	UINT numMaterials = 0;
	UINT numVertices = 0;
	UINT numTriangles = 0;
	UINT numBones = 0;
	UINT numAnimationClips = 0;

	string ignore;

	if (fin)
	{
		fin >> ignore; // file header text
		fin >> ignore >> numMaterials;
		fin >> ignore >> numVertices;
		fin >> ignore >> numTriangles;
		fin >> ignore >> numBones;
		fin >> ignore >> numAnimationClips;

		vector<XMFLOAT4X4> boneOffsets;
		vector<int> boneIndexToParentIndex;
		unordered_map<string, AnimationClip> animations;

		ReadMaterials(fin, numMaterials, mats);
		ReadSubsetTable(fin, numMaterials, subsets);
		ReadSkinnedVertices(fin, numVertices, vertices);
		ReadTriangles(fin, numTriangles, indices);
		ReadBoneOffsets(fin, numBones, boneOffsets);
		ReadBoneHierarchy(fin, numBones, boneIndexToParentIndex);
		ReadAnimationClips(fin, numBones, numAnimationClips, animations);

		skinInfo.Set(boneIndexToParentIndex, boneOffsets, animations);

		return true;
	}
	return false;
}

void M3DLoader::ReadMaterials(ifstream& fin, UINT numMaterials, vector<M3dMaterial>& mats)
{
	string ignore;
	mats.resize(numMaterials);

	string diffuseMapName;
	string normalMapName;

	fin >> ignore; // materials header text
	for (UINT i = 0; i < numMaterials; ++i)
	{
		fin >> ignore >> mats[i].Name;
		fin >> ignore >> mats[i].DiffuseAlbedo.x >> mats[i].DiffuseAlbedo.y >> mats[i].DiffuseAlbedo.z;
		fin >> ignore >> mats[i].FresnelR0.x >> mats[i].FresnelR0.y >> mats[i].FresnelR0.z;
		fin >> ignore >> mats[i].Roughness;
		fin >> ignore >> mats[i].AlphaClip;
		fin >> ignore >> mats[i].MaterialTypeName;
		fin >> ignore >> mats[i].DiffuseMapName;
		fin >> ignore >> mats[i].NormalMapName;
	}
}

void M3DLoader::ReadSubsetTable(ifstream& fin, UINT numSubsets, vector<Subset>& subsets)
{
	string ignore;
	subsets.resize(numSubsets);

	fin >> ignore;
	for (UINT i = 0; i < numSubsets; ++i)
	{
		fin >> ignore >> subsets[i].Id;
		fin >> ignore >> subsets[i].VertexStart;
		fin >> ignore >> subsets[i].VertexCount;
		fin >> ignore >> subsets[i].FaceStart;
		fin >> ignore >> subsets[i].FaceCount;
	}
}

void M3DLoader::ReadVertices(ifstream& fin, UINT numVertices, vector<Vertex>& vertices)
{
	string ignore;
	vertices.resize(numVertices);

	fin >> ignore;
	for (UINT i = 0; i < numVertices; ++i)
	{
		fin >> ignore >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> ignore >> vertices[i].TangentU.x >> vertices[i].TangentU.y >> vertices[i].TangentU.z >> vertices[i].TangentU.w;
		fin >> ignore >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
		fin >> ignore >> vertices[i].TexC.x >> vertices[i].TexC.y;
	}
}

void M3DLoader::ReadSkinnedVertices(ifstream& fin, UINT numVertices, vector<SkinnedVertex>& vertices)
{
	string ignore;
	vertices.resize(numVertices);

	fin >> ignore;
	int boneIndices[4];
	float weights[4];

	for (UINT i = 0; i < numVertices; ++i)
	{
		float blah;
		fin >> ignore >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> ignore >> vertices[i].TangentU.x >> vertices[i].TangentU.y >> vertices[i].TangentU.z >> blah;
		fin >> ignore >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
		fin >> ignore >> vertices[i].TexC.x >> vertices[i].TexC.y;
		fin >> ignore >> weights[0] >> weights[1] >> weights[2] >> weights[3];
		fin >> ignore >> boneIndices[0] >> boneIndices[1] >> boneIndices[2] >> boneIndices[3];

		vertices[i].BoneWeights.x = weights[0];
		vertices[i].BoneWeights.y = weights[1];
		vertices[i].BoneWeights.z = weights[2];

		vertices[i].BoneIndices[0] = (BYTE)boneIndices[0];
		vertices[i].BoneIndices[1] = (BYTE)boneIndices[1];
		vertices[i].BoneIndices[2] = (BYTE)boneIndices[2];
		vertices[i].BoneIndices[3] = (BYTE)boneIndices[3];
	}
}

void M3DLoader::ReadTriangles(ifstream& fin, UINT numTriangles, vector<USHORT>& indices)
{
	string ignore;
	indices.resize(numTriangles * 3);

	fin >> ignore; // triangles header text
	for (UINT i = 0; i < numTriangles; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}
}

void M3DLoader::ReadBoneOffsets(ifstream& fin, UINT numBones, vector<XMFLOAT4X4>& boneOffsets)
{
	string ignore;
	boneOffsets.resize(numBones);

	fin >> ignore; // BoneOffsets header text
	for (UINT i = 0; i < numBones; ++i)
	{
		fin >> boneOffsets[i](0, 0) >> boneOffsets[i](0, 1) >> boneOffsets[i](0, 2) >> boneOffsets[i](0, 3) >>
			boneOffsets[i](1, 0) >> boneOffsets[i](1, 1) >> boneOffsets[i](1, 2) >> boneOffsets[i](1, 3) >>
			boneOffsets[i](2, 0) >> boneOffsets[i](2, 1) >> boneOffsets[i](2, 2) >> boneOffsets[i](2, 3) >>
			boneOffsets[i](3, 0) >> boneOffsets[i](3, 1) >> boneOffsets[i](3, 2) >> boneOffsets[i](3, 3);
	}
}

void M3DLoader::ReadBoneHierarchy(ifstream& fin, UINT numBones, vector<int>& boneIndexToParentIndex)
{
	string ignore;
	boneIndexToParentIndex.resize(numBones);

	fin >> ignore; // BoneHierarchy header
	for (UINT i = 0; i < numBones; ++i)
	{
		fin >> ignore >> boneIndexToParentIndex[i];
	}
}

void M3DLoader::ReadAnimationClips(ifstream& fin, UINT numBones, UINT numAnimationClips, unordered_map<string, AnimationClip>& animations)
{
	string ignore;

	fin >> ignore; // AnimationClips header text
	for (UINT clipIndex = 0; clipIndex < numAnimationClips; ++clipIndex)
	{
		string clipName;
		fin >> ignore >> clipName;
		fin >> ignore;

		AnimationClip clip;
		clip.BoneAnimations.resize(numBones);

		for (UINT boneIndex = 0; boneIndex < numBones; ++boneIndex)
		{
			ReadBoneKeyframes(fin, numBones, clip.BoneAnimations[boneIndex]);
		}
		fin >> ignore;

		animations[clipName] = clip;
	}
}

void M3DLoader::ReadBoneKeyframes(ifstream& fin, UINT numBones, BoneAnimation& boneAnimation)
{
	string ignore;
	UINT numKeyframes = 0;
	fin >> ignore >> ignore >> numKeyframes;
	fin >> ignore;

	boneAnimation.Keyframes.resize(numKeyframes);
	for (UINT i = 0; i < numKeyframes; ++i)
	{
		float t = 0.0f;
		XMFLOAT3 p(0.0f, 0.0f, 0.0f);
		XMFLOAT3 s(1.0f, 1.0f, 1.0f);
		XMFLOAT4 q(0.0f, 0.0f, 0.0f, 1.0f);
		fin >> ignore >> t;
		fin >> ignore >> p.x >> p.y >> p.z;
		fin >> ignore >> s.x >> s.y >> s.z;
		fin >> ignore >> q.x >> q.y >> q.z >> q.w;

		boneAnimation.Keyframes[i].TimePos = t;
		boneAnimation.Keyframes[i].Translation = p;
		boneAnimation.Keyframes[i].Scale = s;
		boneAnimation.Keyframes[i].RotationQuat = q;
	}

	fin >> ignore;

}