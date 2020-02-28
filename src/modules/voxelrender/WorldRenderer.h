/**
 * @file
 */

#pragma once

#include "AnimationShaders.h"
#include "VoxelrenderShaders.h"
#include "WorldChunkMgr.h"
#include "WorldBuffers.h"
#include "EntityMgr.h"
#include "core/Color.h"
#include "core/GLM.h"
#include "core/Var.h"
#include "core/collection/List.h"
#include "render/RandomColorTexture.h"
#include "render/Shadow.h"
#include "render/Skybox.h"
#include "video/Camera.h"
#include "video/FrameBuffer.h"
#include "video/UniformBuffer.h"
#include "voxel/PagedVolume.h"

namespace voxelrender {

/**
 * @brief Class that performs the rendering and extraction of the needed chunks.
 */
class WorldRenderer {
protected:
	WorldChunkMgr _worldChunkMgr;
	WorldBuffers _worldBuffers;
	EntityMgr _entityMgr;

	render::Shadow _shadow;
	render::RandomColorTexture _colorTexture;
	video::TexturePtr _distortionTexture;
	video::TexturePtr _normalTexture;
	render::Skybox _skybox;

	video::FrameBuffer _frameBuffer;
	video::FrameBuffer _reflectionBuffer;
	video::FrameBuffer _refractionBuffer;
	shader::PostprocessShader _postProcessShader;
	video::Buffer _postProcessBuf;
	int32_t _postProcessBufId = -1;

	float _fogRange = 0.0f;
	float _viewDistance = 0.0f;
	float _seconds = 0.0f;
	glm::vec3 _focusPos { 0.0f };

	const glm::vec4 _clearColor = core::Color::LightBlue;
	const glm::vec3 _diffuseColor = glm::vec3(1.0, 1.0, 1.0);
	const glm::vec3 _ambientColor = glm::vec3(0.2, 0.2, 0.2);
	const glm::vec3 _nightColor = glm::vec3(0.001, 0.001, 0.2);
	core::VarPtr _shadowMap;

	// this ub is currently shared between the world, world instanced and water shader
	shader::WorldData _materialBlock;
	// dedicated shaders
	shader::WorldShader _worldShader;
	shader::WaterShader _waterShader;
	shader::SkeletonShader _chrShader;
	// shared shaders
	shader::ShadowmapShader& _shadowMapShader;
	shader::SkeletonshadowmapShader& _skeletonShadowMapShader;

	void initFrameBuffers(const glm::ivec2 &dimensions);
	void shutdownFrameBuffers();

	int renderPostProcessEffects(const video::Camera& camera);
	int renderToShadowMap(const video::Camera& camera);
	int renderClippingPlanes(const video::Camera& camera);
	int renderTerrain(const glm::mat4& viewProjectionMatrix, const glm::vec4& clipPlane);
	int renderWater(const video::Camera& camera, const glm::vec4& clipPlane);
	int renderAll(const video::Camera& camera, const glm::vec4& clipPlane);
	int renderToFrameBuffer(const video::Camera &camera);
	int renderEntities(const glm::mat4& viewProjectionMatrix, const glm::vec4& clipPlane);
public:
	WorldRenderer();
	~WorldRenderer();

	void reset();

	void construct();
	bool init(voxel::PagedVolume *volume, const glm::ivec2 &position, const glm::ivec2 &dimension);
	void update(const video::Camera &camera, uint64_t dt);
	void shutdown();

	render::Shadow &shadow();
	video::FrameBuffer &frameBuffer();
	video::FrameBuffer &reflectionBuffer();
	video::FrameBuffer &refractionBuffer();
	render::RandomColorTexture &colorTexture();

	glm::mat4 reflectionMatrix(const video::Camera& camera) const;

	EntityMgr &entityMgr();
	const EntityMgr &entityMgr() const;

	WorldChunkMgr &chunkMgr();

	void setSeconds(float seconds);

	float getViewDistance() const;
	void setViewDistance(float viewDistance);

	int renderWorld(const video::Camera &camera);
};

inline WorldChunkMgr &WorldRenderer::chunkMgr() {
	return _worldChunkMgr;
}

inline EntityMgr &WorldRenderer::entityMgr() {
	return _entityMgr;
}

inline const EntityMgr &WorldRenderer::entityMgr() const {
	return _entityMgr;
}

inline void WorldRenderer::setSeconds(float seconds) {
	_seconds = seconds;
}

inline float WorldRenderer::getViewDistance() const {
	return _viewDistance;
}

inline void WorldRenderer::setViewDistance(float viewDistance) {
	_viewDistance = viewDistance;
	_fogRange = _viewDistance * 0.80f;
}

inline render::Shadow &WorldRenderer::shadow() {
	return _shadow;
}

inline video::FrameBuffer &WorldRenderer::frameBuffer() {
	return _frameBuffer;
}

inline video::FrameBuffer &WorldRenderer::reflectionBuffer() {
	return _reflectionBuffer;
}

inline video::FrameBuffer &WorldRenderer::refractionBuffer() {
	return _refractionBuffer;
}

inline render::RandomColorTexture &WorldRenderer::colorTexture() {
	return _colorTexture;
}

}
