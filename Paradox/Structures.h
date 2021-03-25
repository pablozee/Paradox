#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

using namespace DirectX;

const UINT gNumFrameResources = 3;

struct Config
{
	Config(const std::string windowTitle = "Paradox",
		unsigned int width = 1280,
		unsigned int height = 960)
		:
		windowTitle(windowTitle),
		width(width),
		height(height)
	{}

	std::string windowTitle;
	unsigned int width, height;
};

static bool CompareVector3WithEpsilon(const XMFLOAT3 lhs, const XMFLOAT3 rhs)
{
	const XMFLOAT3 vector3Epsilon(0.000001f, 0.000001f, 0.000001f);
	return XMVector3NearEqual(XMLoadFloat3(&lhs), XMLoadFloat3(&rhs), XMLoadFloat3(&vector3Epsilon));
}

static bool CompareVector2WithEpsilon(const XMFLOAT2 lhs, const XMFLOAT2 rhs)
{
	const XMFLOAT2 vector2Epsilon(0.000001f, 0.000001f);
	return XMVector2NearEqual(XMLoadFloat2(&lhs), XMLoadFloat2(&rhs), XMLoadFloat2(&vector2Epsilon));
}

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT2 uv;
	XMFLOAT3 normal;

	bool operator==(const Vertex& v) const
	{
		if (CompareVector3WithEpsilon(position, v.position))
		{
			if (CompareVector2WithEpsilon(uv, v.uv))
			{
				if (CompareVector3WithEpsilon(normal, v.normal)) return true;
			}
		}
		return false;
	}

	Vertex& operator=(const Vertex& v)
	{
		position = v.position;
		uv = v.uv;
		normal = v.normal;
		return *this;
	}
};

struct Model
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	std::vector<uint16_t>& GetIndices16()
	{
		if (m_Indices16.empty())
		{
			m_Indices16.resize(indices.size());
			for (size_t i = 0; i < indices.size(); i++)
			{
				m_Indices16[i] = static_cast<uint16_t>(indices[i]);
			}
		}
	}

private:
	std::vector<uint16_t> m_Indices16;
};

struct Material
{
	std::string name = "defaultMaterial";
	int materialCBIndex = -1;
	int numFramesDirty = gNumFrameResources;
	int useTex = 0;
	XMFLOAT3 ambient;
	XMFLOAT3 diffuse;
	XMFLOAT3 specular;
	XMFLOAT3 transmittance;
	XMFLOAT3 emission;
	float shininess;
	float ior;
	float dissolve;
	float roughness;
	float metallic;
	float sheen;
	std::string texturePath = "";
	float textureResolution = 512;
	XMMATRIX materialTransform = XMMatrixIdentity();
};

struct TextureInfo
{
	std::vector<UINT8> pixels;
	int width = 0;
	int height = 0;
	int stride = 0;
	int offset = 0;
};

struct DirectionalLight
{
	XMFLOAT3 direction;
	float dirLightPadding;
	XMFLOAT3 colour;
	float dirLightPadding1;
};

struct PointLight
{
	XMFLOAT3 position;
	float pointLightPadding;
	XMFLOAT3 colour;
	float pointLightPadding1;
};