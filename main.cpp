#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <map>
#include <limits>
#include <cstdint>
#include <fstream>
#include <omp.h>
#include <cmath>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

float zoomFactor = 1.0f;
float rotationAngle = 0.0f;   // horizontal orbit / auto rotation
float cameraPitch = 0.0f;     // vertical mouse tilt

bool autoRotate = true;
float rotationSpeed = 0.002f;
bool simulationPaused = false;

bool mouseDragging = false;
double lastMouseX = 0.0;
double lastMouseY = 0.0;

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

// N body

#include "simulation.h"

#define NBODIES 50000

struct Particle {
    float position[3];
    float velocity[3];
    float mass;
};

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (yoffset > 0)

		zoomFactor *= 1.1f;
    else
        zoomFactor *= 0.9f;

    std::string title = "Zoom: " + std::to_string(zoomFactor);

    glfwSetWindowTitle(window, title.c_str());
    //std::cout << "SCROLL DETECTED -> " << zoomFactor << std::endl;
};


void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            mouseDragging = true;
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        }
        else if (action == GLFW_RELEASE)
        {
            mouseDragging = false;
        }
    }
}

void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    (void)window;

    if (mouseDragging)
    {
        double dx = xpos - lastMouseX;
        double dy = ypos - lastMouseY;

        rotationAngle += static_cast<float>(dx) * 0.005f;
        cameraPitch   += static_cast<float>(dy) * 0.0035f;

        if (cameraPitch > 1.0f)
            cameraPitch = 1.0f;

        if (cameraPitch < -1.0f)
            cameraPitch = -1.0f;

        lastMouseX = xpos;
        lastMouseY = ypos;
    }
}

class NBSim 
{
	public:
		static vk::VertexInputBindingDescription getBindingDescription() {
			vk::VertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Particle);
			bindingDescription.inputRate = vk::VertexInputRate::eVertex;
			return bindingDescription;
		};

		static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
			std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

			// Position
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0; // Maps to POSITION in Slang
			attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat; // float3
			attributeDescriptions[0].offset = offsetof(Particle, position);

			// Velocity
			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1; // Maps to VELOCITY in Slang
			attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat; // float3
			attributeDescriptions[1].offset = offsetof(Particle, velocity);

			// Mass
			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2; // Maps to MASS in Slang
			attributeDescriptions[2].format = vk::Format::eR32Sfloat; // float
			attributeDescriptions[2].offset = offsetof(Particle, mass);

			return attributeDescriptions;
		};

		void init() {
			iterations = 100000;

			simulation = (BHSim*)malloc(sizeof(BHSim));

			simulation->nBodies = NBODIES;
			simulation->dt = 0.1667;
			simulation->theta = 0.5;

			simulation->bodies.x = (real*)malloc(simulation->nBodies * sizeof(real));
			simulation->bodies.y = (real*)malloc(simulation->nBodies * sizeof(real));
			simulation->bodies.z = (real*)malloc(simulation->nBodies * sizeof(real));
			simulation->bodies.w = (real*)malloc(simulation->nBodies * sizeof(real));

			simulation->vel.x = (real*)malloc(simulation->nBodies * sizeof(real));
			simulation->vel.y = (real*)malloc(simulation->nBodies * sizeof(real));
			simulation->vel.z = (real*)malloc(simulation->nBodies * sizeof(real));

			simulation->force.x = (real*)malloc(simulation->nBodies * sizeof(real));
			simulation->force.y = (real*)malloc(simulation->nBodies * sizeof(real));
			simulation->force.z = (real*)malloc(simulation->nBodies * sizeof(real));

			simulation->octree = (octreeArray*)malloc(sizeof(octreeArray));

			simulation->octree->nBodies = simulation->nBodies;
			simulation->octree->arraySize = simulation->nBodies * 2;
			simulation->octree->array = (octreeNode*)malloc(simulation->octree->arraySize * sizeof(octreeNode));

			simulation->octree->nodesArray.x = (real*)malloc(simulation->octree->arraySize * sizeof(real));
			simulation->octree->nodesArray.y = (real*)malloc(simulation->octree->arraySize * sizeof(real));
			simulation->octree->nodesArray.z = (real*)malloc(simulation->octree->arraySize * sizeof(real));
			simulation->octree->nodesArray.w = (real*)malloc(simulation->octree->arraySize * sizeof(real));

			simulation->octree->nodesChildrenArray.x = (real*)malloc(simulation->octree->arraySize * 8 * sizeof(real));
			simulation->octree->nodesChildrenArray.y = (real*)malloc(simulation->octree->arraySize * 8 * sizeof(real));
			simulation->octree->nodesChildrenArray.z = (real*)malloc(simulation->octree->arraySize * 8 * sizeof(real));
			simulation->octree->nodesChildrenArray.w = (real*)malloc(simulation->octree->arraySize * 8 * sizeof(real));

			simulation->octree->nChildrenArray = (unsigned int*)malloc(simulation->octree->arraySize * sizeof(unsigned int));
			simulation->octree->idChildrenArray = (unsigned int*)malloc(simulation->octree->arraySize * sizeof(unsigned int) * 8);
			simulation->octree->posChildrenArray = (unsigned int*)malloc(simulation->octree->arraySize * sizeof(unsigned int) * 8);
			
			randomizeBodies(simulation->bodies, simulation->vel, 100.54f, simulation->nBodies, 1e14f);

			// Calculate the mathematically perfect speed for a stable binary orbit
			const float G = 6.67430e-11f;
			float coreMass = 1e14f;
			float coreDistanceFromCenter = 500.0f;
			float stableOrbitalVel = sqrtf((G * coreMass) / (4.0f * coreDistanceFromCenter)); // ~1.826 m/s

			// Cluster 1 Center (Black Hole 1)
			simulation->bodies.x[0] = coreDistanceFromCenter;
			simulation->bodies.y[0] = 0.0f;
			simulation->bodies.z[0] = 0.0f;
			simulation->bodies.w[0] = coreMass;

			simulation->vel.x[0] = 0.0f;
			simulation->vel.y[0] = -stableOrbitalVel; // Orbiting clockwise/counter-clockwise
			simulation->vel.z[0] = 0.0f;

			// Shift the first half of the particles to orbit around Cluster 1
			for(int i = 1; i < simulation->nBodies / 2; i++)
			{
				simulation->bodies.x[i] += coreDistanceFromCenter; 
				simulation->vel.y[i] += -stableOrbitalVel; // Inherit core velocity
			}

			// Cluster 2 Center (Black Hole 2)
			simulation->bodies.x[1] = -coreDistanceFromCenter;
			simulation->bodies.y[1] = 0.0f;
			simulation->bodies.z[1] = 0.0f;
			simulation->bodies.w[1] = coreMass;

			simulation->vel.x[1] = 0.0f;
			simulation->vel.y[1] = stableOrbitalVel; // Moving opposite to Core 1
			simulation->vel.z[1] = 0.0f;

			// Shift the second half of the particles to orbit around Cluster 2
			for(int i = simulation->nBodies / 2; i < simulation->nBodies; i++)
			{
				if (i == 1) continue; 
				simulation->bodies.x[i] += -coreDistanceFromCenter;
				simulation->vel.y[i] += stableOrbitalVel; // Inherit core velocity
			}

			printf("n=%d bodies for %d iterations:\n", simulation->nBodies, iterations);
		}

		bool step(std::vector<Particle>& targetParticles) {
		if (it < iterations)
		{
			buildOctreeArray(simulation);
			transferOctreeArray(simulation->octree);
			integrateOctreeArray(simulation);

			// Resize the target vector if it doesn't match the current simulation size
			if (targetParticles.size() != simulation->nBodies) {
				targetParticles.resize(simulation->nBodies);
			}

			for (int i = 0; i < simulation->nBodies; i++) {
				targetParticles[i].position[0] = simulation->bodies.x[i]/1000.0f;
				targetParticles[i].position[1] = simulation->bodies.y[i]/1000.0f;
				targetParticles[i].position[2] = simulation->bodies.z[i]/1000.0f;

				targetParticles[i].velocity[0] = simulation->vel.x[i];
				targetParticles[i].velocity[1] = simulation->vel.y[i];
				targetParticles[i].velocity[2] = simulation->vel.z[i];

				targetParticles[i].mass = simulation->bodies.w[i];
			}
			
			it++;
			return true;
		}
		return false;
	}

		void free() {
			real3 p_av = average(simulation->bodies, simulation->nBodies);
			printf("Average position: (%f,%f,%f)\n", p_av.x, p_av.y, p_av.z);
			printf("Body-0  position: (%f,%f,%f)\n", simulation->bodies.x[0], simulation->bodies.y[0], simulation->bodies.z[0]);

			freeAll(simulation);
		}
	private:
		int it;
		int iterations;
		BHSim* simulation;
		Particle bodies;
};

// end of n body defs

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

	std::vector<char> buffer(file.tellg());
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

	file.close();

	return buffer;
}

class NBodyRenderer
{
  public:
	void run()
	{
		BarnesHutSim.init();

		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
		
		BarnesHutSim.free();
	}

  private:
	GLFWwindow                      *window 			= nullptr;
	vk::raii::Context				 context;
	vk::raii::Instance               instance       	= nullptr;
	vk::raii::DebugUtilsMessengerEXT debugMessenger 	= nullptr;
	vk::raii::SurfaceKHR             surface        	= nullptr;
	vk::raii::PhysicalDevice         physicalDevice 	= nullptr;
	vk::raii::Device                 device         	= nullptr;
	vk::raii::Queue                  queue          	= nullptr;
	uint32_t                         queueIndex     	= ~0;
	vk::raii::SwapchainKHR           swapChain      	= nullptr;
	std::vector<vk::Image>           swapChainImages;
	vk::SurfaceFormatKHR             swapChainSurfaceFormat;
	vk::Extent2D                     swapChainExtent;
	std::vector<vk::raii::ImageView> swapChainImageViews;
	vk::raii::PipelineLayout 		 pipelineLayout 	= nullptr;
	vk::raii::Pipeline 				 graphicsPipeline 	= nullptr;
	vk::raii::CommandPool 		     commandPool 		= nullptr;
	vk::raii::CommandBuffer 		 commandBuffer 		= nullptr;

	vk::raii::Semaphore presentCompleteSemaphore = nullptr;
	vk::raii::Semaphore renderFinishedSemaphore  = nullptr;
	vk::raii::Fence     drawFence                = nullptr;

	// Simulation variables
	uint32_t particleCount = NBODIES; // Change this to your desired amount
	std::vector<Particle> particles; 

	// Vulkan handles for your vertex data
	vk::raii::Buffer vertexBuffer = nullptr;
	vk::raii::DeviceMemory vertexBufferMemory = nullptr;

	double lastTime = 0.0;
	int nbFrames = 0;

	// Orbit camera
float orbitAngle = 0.0f;
float orbitRadius = 2.0f;

float cameraX = 0.0f;
float cameraY = 0.0f;
float cameraZ = 2.0f;

	NBSim BarnesHutSim;
	
	std::vector<const char *> requiredDeviceExtension = {
	    vk::KHRSwapchainExtensionName};

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetScrollCallback(window, scrollCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPositionCallback);
	}

	void initVulkan()
	{
		createInstance();
		//setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createGraphicsPipeline();
		createCommandPool();

		createVertexBuffer(); // Bodies buffer

		createCommandBuffer();
    	createSyncObjects();
		
	}

	void mainLoop()
	{
		lastTime = glfwGetTime();

		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			static bool rPressed = false;

if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
{
    if (!rPressed)
    {
        autoRotate = !autoRotate;
        rPressed = true;
    }
}
else
{
    rPressed = false;
}
static bool spacePressed = false;

if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
{
    if (!spacePressed)
    {
        simulationPaused = !simulationPaused;
        spacePressed = true;
    }
}
else
{
    spacePressed = false;
}
static bool hPressed = false;

if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
{
    if (!hPressed)
    {
        std::cout << "===== CONTROLS =====" << std::endl;
        std::cout << "Mouse Wheel  : Zoom" << std::endl;
        std::cout << "0            : Reset Zoom" << std::endl;
        std::cout << "R            : Toggle Rotation" << std::endl;
        std::cout << "UP           : Increase Rotation Speed" << std::endl;
        std::cout << "DOWN         : Decrease Rotation Speed" << std::endl;
        std::cout << "SPACE        : Pause / Resume Simulation" << std::endl;
        std::cout << "LEFT DRAG    : Orbit Camera" << std::endl;
        std::cout << "  Left/Right : Rotate View" << std::endl;
        std::cout << "  Up/Down    : Tilt View" << std::endl;
        std::cout << "H            : Show Help" << std::endl;
        std::cout << "====================" << std::endl;

        hPressed = true;
    }
}
else
{
    hPressed = false;
}
if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS)
{
    zoomFactor = 1.0f;
}
if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
{
    rotationSpeed += 0.0005f;
}

if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
{
    rotationSpeed -= 0.0005f;

    if (rotationSpeed < 0.0f)
        rotationSpeed = 0.0f;
}


			double currentTime = glfwGetTime();
			if (autoRotate)
{
    rotationAngle += rotationSpeed;
}

orbitAngle += 0.01f;

cameraX = orbitRadius * cos(orbitAngle);
cameraZ = orbitRadius * sin(orbitAngle);

nbFrames++;
			
			if (currentTime - lastTime >= 1.0) {
				double fps = double(nbFrames) / (currentTime - lastTime);
    std::string title =
    "NBody | FPS: " +
    std::to_string(static_cast<int>(fps)) +
    " | Bodies: " +
    std::to_string(particleCount) +
    " | Zoom: " +
    std::to_string(zoomFactor) +
    " | Rot: " +
    std::to_string(rotationSpeed) +
    " | State: " +
    std::string(simulationPaused ? "PAUSED" : "RUNNING");
				glfwSetWindowTitle(window, title.c_str());

				nbFrames = 0;
				lastTime = currentTime;
			}

			bool isRunning = false;

if (!simulationPaused)
{
    isRunning = BarnesHutSim.step(particles);
}
			
			if (isRunning) {
				updateVertexBuffer();
			}

			updateVertexBuffer();
			drawFrame();
		}
	}

	void cleanup()
	{
		glfwDestroyWindow(window);

		glfwTerminate();
	}

	void drawFrame()
	{
		auto fenceResult = device.waitForFences(*drawFence, vk::True, UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!");
		}
		device.resetFences(*drawFence);

		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphore, nullptr);
		
		recordCommandBuffer(imageIndex);

		vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
		const vk::SubmitInfo   submitInfo {
			.waitSemaphoreCount   = 1,
			.pWaitSemaphores      = &*presentCompleteSemaphore,
			.pWaitDstStageMask    = &waitDestinationStageMask,
			.commandBufferCount   = 1,
			.pCommandBuffers      = &*commandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores    = &*renderFinishedSemaphore
		};

		queue.submit(submitInfo, *drawFence);

		const vk::PresentInfoKHR presentInfoKHR {
			.waitSemaphoreCount = 1,
			.pWaitSemaphores    = &*renderFinishedSemaphore,
			.swapchainCount     = 1,
			.pSwapchains        = &*swapChain,
			.pImageIndices      = &imageIndex
		};

		result = queue.presentKHR(presentInfoKHR);
	}

	void createInstance()
	{
		constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "Hello Triangle",
		                                      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		                                      .pEngineName        = "No Engine",
		                                      .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
		                                      .apiVersion         = vk::ApiVersion13};

		// Get the required layers
		std::vector<char const *> requiredLayers;
		if (enableValidationLayers)
		{
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		// Check if the required layers are supported by the Vulkan implementation.
		auto layerProperties    = context.enumerateInstanceLayerProperties();
		auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
		                                               [&layerProperties](auto const &requiredLayer) {
			                                               return std::ranges::none_of(layerProperties,
			                                                                           [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
		                                               });
		if (unsupportedLayerIt != requiredLayers.end())
		{
			throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
		}

		// Get the required extensions.
		auto requiredExtensions = getRequiredInstanceExtensions();

		// Check if the required extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		auto unsupportedPropertyIt =
		    std::ranges::find_if(requiredExtensions,
		                         [&extensionProperties](auto const &requiredExtension) {
			                         return std::ranges::none_of(extensionProperties,
			                                                     [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
		                         });
		if (unsupportedPropertyIt != requiredExtensions.end())
		{
			throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
		}

		vk::InstanceCreateInfo createInfo{.pApplicationInfo        = &appInfo,
		                                  .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
		                                  .ppEnabledLayerNames     = requiredLayers.data(),
		                                  .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
		                                  .ppEnabledExtensionNames = requiredExtensions.data()};
		instance = vk::raii::Instance(context, createInfo);
	}

	void createSurface()
	{
		VkSurfaceKHR _surface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
		{
			throw std::runtime_error("failed to create window surface!");
		}
		surface = vk::raii::SurfaceKHR(instance, _surface);
	}

	bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice)
	{
		// Check if the physicalDevice supports the Vulkan 1.3 API version
		bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;

		// Check if any of the queue families support graphics operations
		auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

		// Check if all required physicalDevice extensions are available
		auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
		bool supportsAllRequiredExtensions =
		    std::ranges::all_of(requiredDeviceExtension,
		                        [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
			                        return std::ranges::any_of(availableDeviceExtensions,
			                                                   [requiredDeviceExtension](auto const &availableDeviceExtension) { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
		                        });

		// Check if the physicalDevice supports the required features
		auto features                 = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
		                                                                     vk::PhysicalDeviceVulkan11Features,
		                                                                     vk::PhysicalDeviceVulkan13Features,
		                                                                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
		                                features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
		                                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		// Return true if the physicalDevice meets all the criteria
		return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
	}

	void pickPhysicalDevice()
	{
		// Fetch the available physical devices using RAII
		std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
		if (physicalDevices.empty())
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		// Use an ordered map to automatically sort candidates by score.
		// Note: std::multimap sorts by key in ASCENDING order, so the highest score is at the end.
		std::multimap<int, vk::raii::PhysicalDevice> candidates;

		for (const auto& pd : physicalDevices)
		{
			// 1. HARD GATE: Keep the core functionality check from your first function.
			// If the device lacks queue families, extensions, or swap chain support, ignore it completely.
			if (!isDeviceSuitable(pd))
			{
				continue; 
			}

			auto deviceProperties = pd.getProperties();
			auto deviceFeatures = pd.getFeatures();
			uint32_t score = 0;

			// 2. SCORING: Evaluate how good the suitable device is.
			if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
				score += 1000;
			}

			// Maximum possible size of textures affects graphics quality
			score += deviceProperties.limits.maxImageDimension2D;

			// Application can't function without geometry shaders 
			// (You can leave this here or move this check inside isDeviceSuitable)
			if (!deviceFeatures.geometryShader)
			{
				continue;
			}

			std::cout << deviceProperties.deviceName << " is suitable and has score: " << score << std::endl;

			candidates.insert(std::make_pair(score, pd));
		}

		// 3. SELECTION: Pick the candidate with the highest score.
		// Since multimap sorts ascending, the highest score is at the very end (rbegin).
		if (!candidates.empty())
		{
			physicalDevice = candidates.rbegin()->second;
			std::cout << "Picked GPU: " << physicalDevice.getProperties().deviceName << std::endl;
		}
		else
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}

	void createLogicalDevice()
	{
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

		// get the first index into queueFamilyProperties which supports both graphics and present
		queueIndex = ~0;
		for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
		{
			if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
			    physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
			{
				// found a queue family that supports both graphics and present
				queueIndex = qfpIndex;
				break;
			}
		}
		if (queueIndex == ~0)
		{
			throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
		}

		// query for Vulkan 1.3 features
		vk::StructureChain<vk::PhysicalDeviceFeatures2,
		                   vk::PhysicalDeviceVulkan11Features,
		                   vk::PhysicalDeviceVulkan13Features,
		                   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		    featureChain = {
		        {},                                    // vk::PhysicalDeviceFeatures2
		        {.shaderDrawParameters = true},        // vk::PhysicalDeviceVulkan11Features
		        {.dynamicRendering = true},            // vk::PhysicalDeviceVulkan13Features
		        {.extendedDynamicState = true}         // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		    };

		// create a Device
		float                     queuePriority = 0.5f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
		vk::DeviceCreateInfo      deviceCreateInfo{.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		                                           .queueCreateInfoCount    = 1,
		                                           .pQueueCreateInfos       = &deviceQueueCreateInfo,
		                                           .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
		                                           .ppEnabledExtensionNames = requiredDeviceExtension.data()};

		device = vk::raii::Device(physicalDevice, deviceCreateInfo);
		queue  = vk::raii::Queue(device, queueIndex, 0);
	}

	void createSwapChain()
	{
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
		swapChainExtent                                = chooseSwapExtent(surfaceCapabilities);
		uint32_t minImageCount                         = chooseSwapMinImageCount(surfaceCapabilities);

		std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
		swapChainSurfaceFormat                             = chooseSwapSurfaceFormat(availableFormats);

		std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
		vk::PresentModeKHR              presentMode           = chooseSwapPresentMode(availablePresentModes);

		vk::SwapchainCreateInfoKHR swapChainCreateInfo{.surface          = *surface,
		                                               .minImageCount    = minImageCount,
		                                               .imageFormat      = swapChainSurfaceFormat.format,
		                                               .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
		                                               .imageExtent      = swapChainExtent,
		                                               .imageArrayLayers = 1,
		                                               .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
		                                               .imageSharingMode = vk::SharingMode::eExclusive,
		                                               .preTransform     = surfaceCapabilities.currentTransform,
		                                               .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		                                               .presentMode      = presentMode,
		                                               .clipped          = true};

		swapChain       = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
		swapChainImages = swapChain.getImages();
	}

	void createImageViews()
	{
		assert(swapChainImageViews.empty());

		vk::ImageViewCreateInfo imageViewCreateInfo{ 
			.viewType         = vk::ImageViewType::e2D,
			.format           = swapChainSurfaceFormat.format,
			.subresourceRange = { 
				vk::ImageAspectFlagBits::eColor, 
				0, 1, 0, 1 
			} 
		};
	
		imageViewCreateInfo.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor, 
			.levelCount = 1, 
			.layerCount = 1
		};
		
		for (auto &image : swapChainImages)
		{
			imageViewCreateInfo.image = image;
		}

		for (auto &image : swapChainImages)
		{
			imageViewCreateInfo.image = image;
			swapChainImageViews.emplace_back(device, imageViewCreateInfo);
		}
	}

	void createGraphicsPipeline() {
		vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/slang.spv"));

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
			.stage = vk::ShaderStageFlagBits::eVertex, 
			.module = *shaderModule, 
			.pName = "vertMain"};
		
		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
			.stage = vk::ShaderStageFlagBits::eFragment, 
			.module = *shaderModule, 
			.pName = "fragMain"
		};

		vk::PipelineShaderStageCreateInfo shaderStages[] = {
			vertShaderStageInfo, 
			fragShaderStageInfo
		};

		auto bindingDescription = NBSim::getBindingDescription();
		auto attributeDescriptions = NBSim::getAttributeDescriptions();

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
			
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
			.topology = vk::PrimitiveTopology::ePointList,
			.primitiveRestartEnable = vk::False
		};
	
		std::vector<vk::DynamicState> dynamicStates = {
			vk::DynamicState::eViewport, 
			vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dynamicState{
			.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), 
			.pDynamicStates = dynamicStates.data()
		};

		vk::PipelineViewportStateCreateInfo viewportState{
			.viewportCount = 1, 
			.scissorCount = 1
		};

		vk::PipelineRasterizationStateCreateInfo rasterizer{
			.depthClampEnable        = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode             = vk::PolygonMode::eFill,
			.cullMode                = vk::CullModeFlagBits::eNone, // No need for culling without faces
			.frontFace               = vk::FrontFace::eClockwise,
			.depthBiasEnable         = vk::False,
			.lineWidth               = 1.0f
		};

		vk::PipelineMultisampleStateCreateInfo multisampling{
			.rasterizationSamples = vk::SampleCountFlagBits::e1, 
			.sampleShadingEnable = vk::False
		};

		vk::PipelineColorBlendAttachmentState colorBlendAttachment{
			.blendEnable    = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		};

		/*

		vk::PipelineColorBlendAttachmentState colorBlendAttachment{
			.blendEnable         = vk::True,
			.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
			.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
			.colorBlendOp        = vk::BlendOp::eAdd,
			.srcAlphaBlendFactor = vk::BlendFactor::eOne,
			.dstAlphaBlendFactor = vk::BlendFactor::eZero,
			.alphaBlendOp        = vk::BlendOp::eAdd,
			.colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		};

		*/

		vk::PipelineColorBlendStateCreateInfo colorBlending{
    		.logicOpEnable = vk::False, 
			.logicOp = vk::LogicOp::eCopy, 
			.attachmentCount = 1, .pAttachments = &colorBlendAttachment
		};

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0};

		pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);
	
		vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{ 
			.colorAttachmentCount = 1, 
			.pColorAttachmentFormats = &swapChainSurfaceFormat.format 
		};

		vk::GraphicsPipelineCreateInfo pipelineCreateInfo {
			.stageCount          = 2,
			.pStages             = shaderStages,
			.pVertexInputState   = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState   = &multisampling,
			.pColorBlendState    = &colorBlending,
			.pDynamicState       = &dynamicState,
			.layout              = *pipelineLayout,
			.renderPass          = nullptr
		};

		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
			pipelineCreateInfo,
			pipelineRenderingCreateInfo
		};

		graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
	}

	void createCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo{
			.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		    .queueFamilyIndex = queueIndex
		};

		commandPool = vk::raii::CommandPool(device, poolInfo);
	}

	void createCommandBuffer()
	{
		vk::CommandBufferAllocateInfo allocInfo{ 
			.commandPool = *commandPool, 
			.level = vk::CommandBufferLevel::ePrimary, 
			.commandBufferCount = 1 
		};

		commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
	}

	void recordCommandBuffer(uint32_t imageIndex)
	{
		commandBuffer.begin({});

		// Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
		transition_image_layout(
		    imageIndex,
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eColorAttachmentOptimal,
		    {},                                                        // srcAccessMask (no need to wait for previous operations)
		    vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput         // dstStage
		);
		vk::ClearValue              clearColor     = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo attachmentInfo = {
		    .imageView   = *swapChainImageViews[imageIndex],
		    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		    .loadOp      = vk::AttachmentLoadOp::eClear,
		    .storeOp     = vk::AttachmentStoreOp::eStore,
		    .clearValue  = clearColor};
		vk::RenderingInfo renderingInfo = {
		    .renderArea           = {.offset = {0, 0}, .extent = swapChainExtent},
		    .layerCount           = 1,
		    .colorAttachmentCount = 1,
		    .pColorAttachments    = &attachmentInfo};

		vk::Buffer vertexBuffers[] = { *vertexBuffer };
		vk::DeviceSize offsets[] = { 0 };
		commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);

		commandBuffer.beginRendering(renderingInfo);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
		commandBuffer.draw(particleCount, 1, 0, 0);
		commandBuffer.endRendering();

		// After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
		transition_image_layout(
		    imageIndex,
		    vk::ImageLayout::eColorAttachmentOptimal,
		    vk::ImageLayout::ePresentSrcKHR,
		    vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
		    {},                                                        // dstAccessMask
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		    vk::PipelineStageFlagBits2::eBottomOfPipe                  // dstStage
		);
		commandBuffer.end();
	}

	void createSyncObjects()
	{
		presentCompleteSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
		renderFinishedSemaphore  = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
		drawFence                = vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
	}

	void transition_image_layout(
	    uint32_t                imageIndex,
	    vk::ImageLayout         old_layout,
	    vk::ImageLayout         new_layout,
	    vk::AccessFlags2        src_access_mask,
	    vk::AccessFlags2        dst_access_mask,
	    vk::PipelineStageFlags2 src_stage_mask,
	    vk::PipelineStageFlags2 dst_stage_mask)
	{
		vk::ImageMemoryBarrier2 barrier = {
		    .srcStageMask        = src_stage_mask,
		    .srcAccessMask       = src_access_mask,
		    .dstStageMask        = dst_stage_mask,
		    .dstAccessMask       = dst_access_mask,
		    .oldLayout           = old_layout,
		    .newLayout           = new_layout,
		    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .image               = swapChainImages[imageIndex],
		    .subresourceRange    = {
		           .aspectMask     = vk::ImageAspectFlagBits::eColor,
		           .baseMipLevel   = 0,
		           .levelCount     = 1,
		           .baseArrayLayer = 0,
		           .layerCount     = 1}};
		vk::DependencyInfo dependency_info = {
		    .dependencyFlags         = {},
		    .imageMemoryBarrierCount = 1,
		    .pImageMemoryBarriers    = &barrier};
		commandBuffer.pipelineBarrier2(dependency_info);
	}

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
	{
		vk::ShaderModuleCreateInfo createInfo{ 
			.codeSize = code.size() * sizeof(char), 
			.pCode = reinterpret_cast<const uint32_t*>(code.data()) 
		};

		vk::raii::ShaderModule shaderModule{ 
			device, 
			createInfo 
		};

		return shaderModule;
	}

	static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
	{
		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
		{
			minImageCount = surfaceCapabilities.maxImageCount;
		}
		return minImageCount;
	}

	static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats)
	{
		assert(!availableFormats.empty());
		const auto formatIt = std::ranges::find_if(
		    availableFormats,
		    [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
		return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
	}

	static vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
	{
		assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
		return std::ranges::any_of(availablePresentModes,
		                           [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
		           vk::PresentModeKHR::eMailbox :
		           vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		return {
		    std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		    std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
	}

	std::vector<const char *> getRequiredInstanceExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enableValidationLayers)
		{
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

	// N-body helper functions

	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) 
	{
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		throw std::runtime_error("failed to find suitable memory type!");
	}

	void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, 
					vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory) 
					{
		
		vk::BufferCreateInfo bufferInfo{};
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;

		buffer = vk::raii::Buffer(device, bufferInfo);

		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

		vk::MemoryAllocateInfo allocInfo{};
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
		buffer.bindMemory(*bufferMemory, 0);
	}

	void createVertexBuffer() {
		BarnesHutSim.step(particles);
    	particleCount = static_cast<uint32_t>(particles.size());

		vk::DeviceSize bufferSize = sizeof(Particle) * particleCount;

		// Initialize your particle vectors with your initial simulation states here
		particles.resize(particleCount);
		// e.g., fill particles[i].position and mass...

		// Create a buffer that functions as a Vertex Buffer and can be written to by the host (CPU)
		createBuffer(
			bufferSize,
			vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vertexBuffer,
			vertexBufferMemory
		);

		// Initial upload of data
		updateVertexBuffer();
	}

void updateVertexBuffer() {
    vk::DeviceSize bufferSize = sizeof(Particle) * particles.size();

    std::vector<Particle> zoomedParticles = particles;

    for (auto& p : zoomedParticles)
{
    float x = p.position[0];
    float y = p.position[1];
    float z = p.position[2];

    // Horizontal orbit / auto rotation around Z axis
    float cosYaw = cos(rotationAngle);
    float sinYaw = sin(rotationAngle);

    float x1 = x * cosYaw - y * sinYaw;
    float y1 = x * sinYaw + y * cosYaw;
    float z1 = z;

    // Vertical tilt from mouse drag
    float cosPitch = cos(cameraPitch);
    float sinPitch = sin(cameraPitch);

    float y2 = y1 * cosPitch - z1 * sinPitch;
    // We do not use z2 for rendering because changing clip-space Z can make particles disappear.

    // Apply zoom
    p.position[0] = x1 * zoomFactor;
    p.position[1] = y2 * zoomFactor;
    p.position[2] = 0.5f;  // keep particles inside Vulkan visible depth range
}

    void* data = vertexBufferMemory.mapMemory(0, bufferSize);
    std::memcpy(data, zoomedParticles.data(), static_cast<size_t>(bufferSize));
    vertexBufferMemory.unmapMemory();
}
};
int main()
{
	try
	{
		NBodyRenderer app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
