#pragma once
#include "core.h"

using namespace DirectX;
using namespace std;
// A Keyframe defines the bone transformation at an instant in time


struct Keyframe
{
	Keyframe();
	~Keyframe();

	float TimePos;

	XMFLOAT3 Translation;
	XMFLOAT3 Scale;
	XMFLOAT4 RotationQuat;
};

/**
 * A BoneAnimation is defined by a list of keyframes. For time values
 * in between two keyframes, we interpolate between the two nearest
 * keyframes that bound the time.
 * 
 * We assume an animation always has two keyframes.
 */
struct BoneAnimation
{
	float GetStartTime() const;
	float GetEndTime() const;

	void Interpolate(float t, XMFLOAT4X4& M) const;
	std::vector<Keyframe> Keyframes;
};

/**
 * Examples of AnimationClips are "Walk", "Run", "Attack", "Defend".
 * An AnimationClip requires a BoneAnimation for every bone to 
 * form the animation clip.
 */
struct AnimationClip
{
	float GetClipStartTime() const;
	float GetClipEndTime() const;
	
	void Interpolate(float t, std::vector<XMFLOAT4X4>& boneTransforms) const;
	std::vector<BoneAnimation> BoneAnimations;
};

class SkinnedData
{
public:

	UINT BoneCount() const;

	float GetClipStartTime(const std::string& clipName) const;
	float GetClipEndTime(const std::string& clipName) const;

	void Set(
		vector<int>& boneHierarchy,
		vector<XMFLOAT4X4>& boneOffsets,
		unordered_map<string, AnimationClip>& animations
	);

	/**
	 * In a real project you'd want to cache the result if there was a chance
	 * that you were calling this several times with the same clipName at
	 * the same timePos.
	 */
	void GetFinalTransforms(const string& clipName, float timePos,
		vector<XMFLOAT4X4>& finalTransforms) const;

private:
	
	// Gives parentIndex of the ith bone
	vector<int> mBoneHierarchy;

	vector<XMFLOAT4X4> mBoneOffsets;

	unordered_map<string, AnimationClip> mAnimations;
};