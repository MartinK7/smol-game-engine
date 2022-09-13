
#include <array>
#include <memory>
#include <iostream>
#include <fstream>
#include "sge/base/sge.hpp"
#include "demo.hpp"

#define PATH "../demo/assets"

namespace Game {

	static glm::vec3 fromBlender(glm::vec3 vec) {
		return glm::vec3(vec.x, vec.z, -vec.y);
	}

	static GL::Texture cubemapBackground;
	static GL::Program programBMaterial, programBackground, programBright, programBDepth;

	static SGE::Camera cameraPlayer;
	static glm::vec2 cameraPlayerAzimuthAltitude(214.0f, -9.0);

	static SGE::Model modelCube;

	static std::map<std::string, SGE::GameObject> gameObjects;
	static glm::vec3 lightPointPosition = fromBlender({-5.42395f, -3.00282f, 3.03571f});
	static GL::Framebuffer lightDepthCubemap, teapotCubemap;

	static std::vector<glm::vec3> lightPath;

	static inline float map(float value, float min1, float max1, float min2, float max2) {
		return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
	}

	static void loadTextures() {
		/// Create textures

		// Depth map for light
		lightDepthCubemap.createFramebufferCubemapDepthmap(); // This bugs and hide other textures, why??
		lightDepthCubemap.bindTextureDepth(10);

		teapotCubemap.createFramebufferCubemap();
		teapotCubemap.bindTextureColor(21);

		const char *cubemapBackgroundFiles[] = {
				PATH"/images/cubemaps/Maskonaive2/posx.jpg", PATH"/images/cubemaps/Maskonaive2/negx.jpg",
				PATH"/images/cubemaps/Maskonaive2/posy.jpg", PATH"/images/cubemaps/Maskonaive2/negy.jpg",
				PATH"/images/cubemaps/Maskonaive2/posz.jpg", PATH"/images/cubemaps/Maskonaive2/negz.jpg"
		};
		cubemapBackground.createCubemapFromFiles(cubemapBackgroundFiles);
		cubemapBackground.bind(20);
	}

	static void loadModels() {
		modelCube.createCube();

		const char *models_names[] = {"boxes", "bulb", "light_frame", "floor", "walls", "vent", "vent_pipe", "vent_hold", "vent_prop", "teapot"};
		for(auto &o : models_names) {
			// Load model data
			SGE::GameObject gameObject;
			gameObject.model = std::make_shared<SGE::Model>();
			gameObject.model->createFromOBJ(PATH"/models/test_scene/" + std::string(o) + ".obj");

			// Create material
			gameObject.bmaterial = std::make_shared<SGE::BMaterial>();

			// Create transformation
			gameObject.affine = std::make_shared<SGE::Affine>();

			gameObjects[o] = gameObject;
		}

		// Experimental, attach texture to one specific object
		if(gameObjects.contains("boxes")) {
			// Load texture
			std::shared_ptr<GL::Texture> texture = std::make_shared<GL::Texture>();
			texture->createFromFile(PATH"/images/textures/TexturesCom_Cargo0097/TexturesCom_Cargo0097_M.jpg");
			gameObjects["boxes"].bmaterial->setTextureAlbedo(texture);
			// Texture only
			gameObjects["boxes"].bmaterial->setMixColorTextureAlbedo(1.0f);
		}

		if(gameObjects.contains("vent")) {
			// Load texture
			std::shared_ptr<GL::Texture> texture = std::make_shared<GL::Texture>();
			texture->createFromFile(PATH"/images/textures/TexturesCom_MetalBare0212_7_M.jpg");
			gameObjects["vent"].bmaterial->setTextureAlbedo(texture);
			// Texture only
			gameObjects["vent"].bmaterial->setMixColorTextureAlbedo(1.0f);
		}

		if(gameObjects.contains("floor")) {
			// Load texture
			std::shared_ptr<GL::Texture> texture = std::make_shared<GL::Texture>();
			texture->createFromFile(PATH"/images/textures/TexturesCom_ConcreteBare0330_7_seamless_S.png");
			gameObjects["floor"].bmaterial->setTextureAlbedo(texture);
			// Texture only
			gameObjects["floor"].bmaterial->setMixColorTextureAlbedo(1.0f);
		}

		if(gameObjects.contains("walls")) {
			// Load texture
			std::shared_ptr<GL::Texture> textureAlbedo = std::make_shared<GL::Texture>();
			textureAlbedo->createFromFile(PATH"/images/textures/TexturesCom_Brick_Rustic2_1K/TexturesCom_Brick_Rustic2_1K_albedo.png");
			gameObjects["walls"].bmaterial->setTextureAlbedo(textureAlbedo);
			// Load texture
			std::shared_ptr<GL::Texture> textureNormal = std::make_shared<GL::Texture>();
			textureNormal->createFromFile(PATH"/images/textures/TexturesCom_Brick_Rustic2_1K/TexturesCom_Brick_Rustic2_1K_normal.png");
			gameObjects["walls"].bmaterial->setTextureNormal(textureNormal);
			// Texture only
			gameObjects["walls"].bmaterial->setMixColorTextureAlbedo(1.0f);
		}

		if(gameObjects.contains("teapot")) {
			// Texture only
			gameObjects["teapot"].bmaterial->setColorAlbedo(glm::vec3(1.0f, 0.8f, 0.8f));
			gameObjects["teapot"].bmaterial->setGlossy(0.5);
			// Position
			gameObjects["teapot"].affine->setPosition(fromBlender({-3.35234f, -0.57187f, 0.0f}));
			gameObjects["teapot"].affine->setScale(glm::vec3(0.8f));
		}
	}

	static void loadPaths() {
		std::ifstream path;
		path.open(PATH"/paths/lightpath.txt");
		for(std::string line; std::getline(path, line); ) {
			glm::vec3 p;
			if(sscanf(line.c_str(), "%f %f %f", &p.x, &p.y, &p.z) == 3) {
				lightPath.push_back(p);
			}
		}
		path.close();
	}

	static void loadPrograms() {
		programBMaterial.createFromFile(PATH"/programs/bmaterial/bmaterial.vert", PATH"/programs/bmaterial/bmaterial.frag");
		programBackground.createFromFile(PATH"/programs/background/background.vert", PATH"/programs/background/background.frag");
		programBright.createFromFile(PATH"/programs/bright/bright.vert", PATH"/programs/bright/bright.frag");
		programBDepth.createFromFile(PATH"/programs/bdepth/bdepth.vert", PATH"/programs/bdepth/bdepth.frag");
	}

	static void updateProgramsUniforms(bool once) {
		if(once) {
			// Default model matrices
			programBMaterial.setMat4f("matrixModel", glm::mat4(1.0f));
			programBright.setMat4f("matrixModel", glm::mat4(1.0f));

			// Bind uniform textures to layers
			programBMaterial.setInt("cubemapBackground", 20);
			programBackground.setInt("cubemapBackground", 20);
			programBMaterial.setInt("sge_pointLight.cubemapDepthmap", 10);
			programBMaterial.setInt("sge_material.cubemapEnvironment", 21);

			cameraPlayer.setPosition(fromBlender({-3.77249f, 6.23212f, 1.99697f}));
		}

		// Player camera
		programBMaterial.setMat4f("matrixCamera", cameraPlayer.getTransformMatrix());
		programBMaterial.setVec3f("cameraPosition", cameraPlayer.getPosition());
		//
		programBright.setMat4f("matrixCamera", cameraPlayer.getTransformMatrix());

		// Lights
		programBMaterial.setVec3f("sge_pointLight.position", lightPointPosition);
		programBMaterial.setVec3f("sge_pointLight.color", glm::vec3(0.7f, 0.7f, 0.7f));
		programBMaterial.setFloat("sge_pointLight.intensity", 1.5f);

		programBDepth.setVec3f("sge_pointLight.position", lightPointPosition);
	}

	static void drawSkybox(GL::Window &window) {
		// Do not care about depth test and also invert face culling
		// (the used model cube normals are directed outward we need them inwards)
		glDisable(GL_DEPTH_TEST);
		glCullFace(GL_FRONT);

		// Place temporary player camera inside cube and keep same rotation
		SGE::Camera cameraCubemap(window.getRatio(), glm::vec3(0.0f), glm::normalize(cameraPlayer.getLookAt() - cameraPlayer.getPosition()));
		programBackground.setMat4f("matrixCamera", cameraCubemap.getTransformMatrix());

		// Draw simple cube which fills user view
		modelCube.draw();

		// Reset settings
		glEnable(GL_DEPTH_TEST);
		glCullFace(GL_BACK);
	}

	static void updatePlayer(GL::Window &window) {
		float speedMax = 0.02f;
		float rotationSpeed = 0.3f;

		cameraPlayerAzimuthAltitude -= rotationSpeed * window.getMouseDelta();
		while(cameraPlayerAzimuthAltitude.x > 360.0f) { cameraPlayerAzimuthAltitude.x -= 360.0f; }
		while(cameraPlayerAzimuthAltitude.x < -360.0f) { cameraPlayerAzimuthAltitude.x += 360.0f; }
		if(cameraPlayerAzimuthAltitude.y > 89.5f) { cameraPlayerAzimuthAltitude.y = 89.5f; }
		if(cameraPlayerAzimuthAltitude.y < -89.5f) { cameraPlayerAzimuthAltitude.y = -89.5f; }

		glm::vec4 dirForwardAzimuth(0.0f, 0.0f, -1.0f, 1.0f);
		glm::vec4 dirForwardAzimuthAltitude = dirForwardAzimuth;
		glm::vec4 dirRight(1.0f, 0.0f, 0.0f, 1.0f);
		glm::vec4 dirUp(0.0f, 1.0f, 0.0f, 1.0f);
		glm::mat4 matrixAzimuth = glm::rotate(glm::mat4(1.0f), glm::radians((float)cameraPlayerAzimuthAltitude.x), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 matrixAltitude = glm::rotate(matrixAzimuth, glm::radians((float)cameraPlayerAzimuthAltitude.y), glm::vec3(1.0f, 0.0f, 0.0f));
		dirForwardAzimuth = matrixAzimuth * dirForwardAzimuth;
		dirForwardAzimuthAltitude = matrixAltitude * dirForwardAzimuthAltitude;
		dirRight = matrixAltitude * dirRight;

		if(window.isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
			speedMax *= 3.0f;
		}

		if(window.isKeyPressed(GLFW_KEY_W)) {
			cameraPlayer.setPosition(cameraPlayer.getPosition() + speedMax * glm::vec3(dirForwardAzimuth));
		}

		if(window.isKeyPressed(GLFW_KEY_S)) {
			cameraPlayer.setPosition(cameraPlayer.getPosition() - speedMax * glm::vec3(dirForwardAzimuth));
		}

		if(window.isKeyPressed(GLFW_KEY_A)) {
			cameraPlayer.setPosition(cameraPlayer.getPosition() - speedMax * glm::vec3(dirRight));
		}

		if(window.isKeyPressed(GLFW_KEY_D)) {
			cameraPlayer.setPosition(cameraPlayer.getPosition() + speedMax * glm::vec3(dirRight));
		}

		if(window.isKeyPressed(GLFW_KEY_SPACE)) {
			cameraPlayer.setPosition(cameraPlayer.getPosition() + speedMax * glm::vec3(dirUp));
		}

		if(window.isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
			cameraPlayer.setPosition(cameraPlayer.getPosition() - speedMax * glm::vec3(dirUp));
		}

		cameraPlayer.setAspectRatio(window.getRatio());
		cameraPlayer.setLookAt(cameraPlayer.getPosition() + glm::vec3(dirForwardAzimuthAltitude));
	}

	static void Start() {
		GL::Window window;
		window.create(640 * 2.5, 480 * 2);
		//window.create(320, 240);

		loadPrograms();
		loadModels();
		loadTextures();
		loadPaths();

		bool once = true;

		while(!window.shouldExit()) {
			window.setTitleFPS("");
			window.getMouseDelta();

			// Animate point light
			float t = window.getTimeSeconds();
			uint32_t index = (uint32_t)(t * 10.0f);
			float t0 = (float)(index + 0) / 10.0f;
			float t1 = (float)(index + 1) / 10.0f;
			if(index + 1 < lightPath.size()) {
				lightPointPosition = glm::mix(lightPath[index], lightPath[index + 1], map(t, t0, t1, 0.0f, 1.0f));
			}

			updatePlayer(window);

			updateProgramsUniforms(once);

			#if 1
			/// Drawing - textures
			SGE::Camera cameraCubemap(1.0f, glm::vec3(0.0), glm::vec3(0.0), 90.0f);
			std::array<std::pair<glm::vec3, glm::vec3>, 6> cameraCubemapList = {
				std::make_pair(glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.0,-1.0, 0.0)),
				std::make_pair(glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0,-1.0, 0.0)),
				std::make_pair(glm::vec3( 0.0f, 1.0f, 0.0f), glm::vec3(0.0, 0.0, 1.0)),
				std::make_pair(glm::vec3( 0.0f,-1.0f, 0.0f), glm::vec3(0.0, 0.0,-1.0)),
				std::make_pair(glm::vec3( 0.0f, 0.0f, 1.0f), glm::vec3(0.0,-1.0, 0.0)),
				std::make_pair(glm::vec3( 0.0f, 0.0f,-1.0f), glm::vec3(0.0,-1.0, 0.0))
			};

			// Render light depth cubemap
			cameraCubemap.setPosition(lightPointPosition);
			for(int i = 0; i < 6; ++i) {
				// Bind target cubemap face
				lightDepthCubemap.setViewPort(i);

				// Camera
				cameraCubemap.setLookAt(cameraCubemap.getPosition() + cameraCubemapList[i].first);
				cameraCubemap.setUpVector(cameraCubemapList[i].second);

				// Objects in scene
				programBDepth.setMat4f("matrixModel", glm::mat4(1.0f));
				programBDepth.setMat4f("matrixCamera", cameraCubemap.getTransformMatrix());
				//glCullFace(GL_FRONT);
				for(auto &o : gameObjects) {
					if(o.second.affine) {
						o.second.affine->setUniforms(programBDepth);
					}
					if(o.second.model) {
						o.second.model->draw();
					}
				}
				//glCullFace(GL_BACK);
			}

			// Render teapot environment cubemap
			if(gameObjects.contains("teapot")) {
				cameraCubemap.setPosition(gameObjects["teapot"].affine->getPosition() + glm::vec3(0.0f, 0.1f, 0.0f));
				for(int i = 0; i < 6; ++i) {
					// Bind target cubemap face
					teapotCubemap.setViewPort(i);

					// Camera
					cameraCubemap.setLookAt(cameraCubemap.getPosition() + cameraCubemapList[i].first);
					cameraCubemap.setUpVector(cameraCubemapList[i].second);

					// Objects in scene
					programBMaterial.setMat4f("matrixCamera", cameraCubemap.getTransformMatrix());
					for(auto &o : gameObjects) {
						if(o.first == "teapot") {
							continue;
						}
						if(o.second.bmaterial) {
							o.second.bmaterial->setUniforms(programBMaterial);
						}
						if(o.second.affine) {
							o.second.affine->setUniforms(programBMaterial);
						}
						if(o.second.model) {
							o.second.model->draw();
						}
					}
				}
			}
			#endif

			/// Drawing - main window
			window.setViewport();

			// Draw background first
			#if 1
			drawSkybox(window);
			#endif

			#if 1
			// Objects in scene
			programBMaterial.setMat4f("matrixCamera", cameraPlayer.getTransformMatrix());
			for(auto &o : gameObjects) {
				if(o.second.bmaterial) {
					o.second.bmaterial->setUniforms(programBMaterial);
				}
				if(o.second.affine) {
					o.second.affine->setUniforms(programBMaterial);
				}
				if(o.second.model) {
					o.second.model->draw();
				}
			}
			#endif

			#if 1
			// Debugging objects
			SGE::Affine transform;
			transform.reset();
			transform.setScale(glm::vec3(0.01f));
			transform.setPosition(lightPointPosition);
			programBright.setMat4f("matrixModel", transform.getTransformMatrix());
			modelCube.draw();
			#endif

			/// End of drawing

			window.swapBuffer();

			/// Update other post draw variables

			once = false;
		}
	}
}

int main() {
	//GL::Test::DemoStart();
	Game::Start();
	return 0;
}