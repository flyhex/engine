#include "core/tests/AbstractTest.h"
#include "noise/SimplexNoise.h"
// TODO: not a real dependency to the voxel module.... but... look there - a three headed monkey!
#include "voxel/WorldContext.h"
#include "voxel/Voxel.h"
#include "image/Image.h"
#include <glm/gtc/noise.hpp>
#include <glm/gtc/constants.hpp>

namespace noise {

class SimplexNoiseTest: public core::AbstractTest {
protected:
	const int components = 4;
	const int w = 256;
	const int h = 256;

	bool WriteImage(const char* name, uint8_t* buffer, int w = 256, int h = 256, int components = 4) {
		return image::Image::writePng(name, buffer, w, h, components);
	}
};

TEST_F(SimplexNoiseTest, testLandscapeMountains) {
	const int w = 1024;
	const int h = 1024;
	uint8_t buffer[w * h * components];

	voxel::WorldContext worldCtx;

	for (int x = 0; x < w; ++x) {
		for (int y = 0; y < h; ++y) {
			const glm::vec2 pos(x, y);
			const float landscapeNoise = noise::Simplex::Noise2D(pos, worldCtx.landscapeNoiseOctaves,
					worldCtx.landscapeNoisePersistence, worldCtx.landscapeNoiseFrequency, worldCtx.landscapeNoiseAmplitude);
			const float noiseNormalized = noise::norm(landscapeNoise);
			ASSERT_LE(noiseNormalized, 1.0f)<< "Noise is bigger than 1.0: " << noiseNormalized;
			ASSERT_GE(noiseNormalized, 0.0f)<< "Noise is less than 0.0: " << noiseNormalized;
			const float mountainNoise = noise::Simplex::Noise2D(pos, worldCtx.mountainNoiseOctaves,
					worldCtx.mountainNoisePersistence, worldCtx.mountainNoiseFrequency, worldCtx.mountainNoiseAmplitude);
			const float mountainNoiseNormalized = noise::norm(mountainNoise);

			const int ni = noiseNormalized * (MAX_TERRAIN_HEIGHT - 1);
			const int mni = mountainNoiseNormalized * (MAX_MOUNTAIN_HEIGHT - 1);
			const int dni = std::max(0, mni - ni);
			const unsigned char color = (unsigned char) ((ni + dni) / MAX_HEIGHT * 255.0f);
			ASSERT_LE(color, 255) << "Color is bigger than 255: " << color;
			ASSERT_GE(color, 0) << "Color is less than 0: " << color;
			int index = y * (w * components) + (x * components);
			const int n = components == 4 ? 3 : components;
			for (int i = 0; i < n; ++i) {
				buffer[index++] = color;
			}
			if (components == 4) {
				buffer[index] = 255;
			}
		}
	}
	ASSERT_TRUE(WriteImage("testNoiseLandscapeMountains.png", buffer, w, h));
}

TEST_F(SimplexNoiseTest, testLandscape) {
	const int w = 1024;
	const int h = 1024;
	uint8_t buffer[w * h * components];

	voxel::WorldContext worldCtx;

	for (int x = 0; x < w; ++x) {
		for (int y = 0; y < h; ++y) {
			const glm::vec2 pos(x, y);
			const float landscapeNoise = noise::Simplex::Noise2D(pos, worldCtx.landscapeNoiseOctaves,
					worldCtx.landscapeNoisePersistence, worldCtx.landscapeNoiseFrequency, worldCtx.landscapeNoiseAmplitude);
			const float noiseNormalized = noise::norm(landscapeNoise);
			ASSERT_LE(noiseNormalized, 1.0f)<< "Noise is bigger than 1.0: " << noiseNormalized;
			ASSERT_GE(noiseNormalized, 0.0f)<< "Noise is less than 0.0: " << noiseNormalized;
			const unsigned char color = (unsigned char) (noiseNormalized * 255.0f);
			ASSERT_LE(color, 255) << "Color is bigger than 255: " << color;
			ASSERT_GE(color, 0) << "Color is less than 0: " << color;
			int index = y * (w * components) + (x * components);
			const int n = components == 4 ? 3 : components;
			for (int i = 0; i < n; ++i) {
				buffer[index++] = color;
			}
			if (components == 4) {
				buffer[index] = 255;
			}
		}
	}
	ASSERT_TRUE(WriteImage("testNoiseLandscape.png", buffer, w, h));
}

TEST_F(SimplexNoiseTest, testMountains) {
	const int w = 2048;
	const int h = 2048;
	uint8_t buffer[w * h * components];

	voxel::WorldContext worldCtx;

	for (int x = 0; x < w; ++x) {
		for (int y = 0; y < h; ++y) {
			const glm::vec2 pos(x, y);
			const int o = worldCtx.mountainNoiseOctaves;
			const float p = worldCtx.mountainNoisePersistence;
			const float f = worldCtx.mountainNoiseFrequency;
			const float a = worldCtx.mountainNoiseAmplitude;
			const float mountainNoise = noise::Simplex::Noise2D(pos, o, p, f, a);
			const float mountainNoiseNormalized = noise::norm(mountainNoise);
			const unsigned char color = (unsigned char) (mountainNoiseNormalized * 255.0f);
			ASSERT_LE(color, 255) << "Color is bigger than 255: " << color;
			ASSERT_GE(color, 0) << "Color is less than 0: " << color;
			int index = y * (w * components) + (x * components);
			const int n = components == 4 ? 3 : components;
			for (int i = 0; i < n; ++i) {
				buffer[index++] = color;
			}
			if (components == 4) {
				buffer[index] = 255;
			}
		}
	}
	ASSERT_TRUE(WriteImage("testNoiseMountains.png", buffer, w, h));
}

TEST_F(SimplexNoiseTest, test2DNoise) {
	uint8_t buffer[w * h * components];

	for (int x = 0; x < w; ++x) {
		for (int y = 0; y < h; ++y) {
			const glm::vec2 pos(x, y);
			const float noise = Simplex::Noise2D(pos, 2, 1.0f, 0.5f, 1.5f);
			float normalized = noise::norm(noise);
			ASSERT_LE(normalized, 1.0f)<< "Noise is bigger than 1.0: " << normalized;
			ASSERT_GE(normalized, 0.0f)<< "Noise is less than 0.0: " << normalized;
			unsigned char color = (unsigned char) (normalized * 255.0f);
			ASSERT_LE(color, 255)<< "Color is bigger than 255: " << color;
			ASSERT_GE(color, 0)<< "Color is less than 0: " << color;
			int index = y * (w * components) + (x * components);
			const int n = components == 4 ? 3 : components;
			for (int i = 0; i < n; ++i) {
				buffer[index++] = color;
			}
			if (components == 4)
				buffer[index] = 255;
		}
	}
	ASSERT_TRUE(WriteImage("testNoise2d.png", buffer));
}

TEST_F(SimplexNoiseTest, test2DNoiseGray) {
	const int width = 100;
	const int height = 100;
	const int components = 3;
	uint8_t buffer[width * height * components];
	Simplex::Noise2DGray(buffer, width, height, 1, 1.0, 1.0, 1.0);
	ASSERT_TRUE(image::Image::writePng("testNoiseGray.png", buffer, width, height, components));
}

TEST_F(SimplexNoiseTest, test2DNoiseColorMap) {
	const int width = 256;
	const int height = 256;
	const int components = 3;
	uint8_t buffer[width * height * components];
	const int octaves = 2;
	const float persistence = 0.3f;
	const float frequency = 0.7f;
	const float amplitude = 1.0f;
	noise::Simplex::SeamlessNoise2DRGB(buffer, width, octaves, persistence, frequency, amplitude);
	ASSERT_TRUE(image::Image::writePng("testNoiseColorMap.png", buffer, width, height, components));
}

}
