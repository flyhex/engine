/**
 * @file
 */

#pragma once

#include "core/IComponent.h"
#include "video/FrameBuffer.h"
#include "core/SharedPtr.h"

namespace video {

struct TextureAtlasData {
	// the region [0-1]
	float sx;
	float sy;
	float tx;
	float ty;

	// the framebuffer texture size
	int texWidth;
	int texHeight;

	video::Id handle;
};

/**
 * @brief Helper class to render objects to a texture atlas. The objects get an id "on" the
 * texture that is used to look up the texture coordinates
 */
class TextureAtlasRenderer : public core::IComponent {
private:
	video::FrameBuffer _frameBuffer;

	glm::ivec2 resolvePos(int id, int w, int h);

public:
	TextureAtlasData beginRender(int id, int w, int h);
	void endRender();

	bool init() override;
	void shutdown() override;
};

using TextureAtlasRendererPtr = core::SharedPtr<TextureAtlasRenderer>;

}