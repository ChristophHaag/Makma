#include "Renderer.hpp"

#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // bring the depth range from [-1,1] (OpenGL) to [0,1] (Vulkan)
#include <gtc\matrix_transform.hpp>

Renderer::Renderer(const std::shared_ptr<Window> window)
{
	this->window = window;

	context = std::make_shared<Context>(window);
	swapchain = std::make_unique<Swapchain>(window, context);
	
	buffers = std::make_shared<Buffers>(context);
	model = std::make_unique<Model>("Models\\Chalet.obj", buffers);
	buffers->finalize();

	texture = std::make_shared<Texture>("Textures\\Chalet.jpg", context);

	pipeline = std::make_shared<Pipeline>(window, context, buffers, texture);
	
	swapchain->createFramebuffers(pipeline);
	swapchain->createCommandBuffers();

#ifndef MK_OPTIMIZATION_PUSH_CONSTANTS
	// just record the command buffers once if we are not using push constants
	swapchain->recordCommandBuffers(pipeline, buffers);
#endif

	semaphores = std::make_unique<Semaphores>(context);
}

void Renderer::update()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

#ifdef MK_OPTIMIZATION_PUSH_CONSTANTS
	pushConstants[0] = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	pushConstants[1] = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	pushConstants[2] = glm::perspective(glm::radians(45.0f), window->getWidth() / (float)window->getHeight(), 0.1f, 10.0f);
	pushConstants[2][1][1] *= -1; // flip the y axis because GLM uses the OpenGL coordinate system
#else
	uniformBufferObject.worldMatrix = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	uniformBufferObject.viewMatrix = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	uniformBufferObject.projectionMatrix = glm::perspective(glm::radians(45.0f), window->getWidth() / (float)window->getHeight(), 0.1f, 10.0f);
	uniformBufferObject.projectionMatrix[1][1] *= -1; // flip the y axis because GLM uses the OpenGL coordinate system

	auto memory = context->getDevice()->mapMemory(*buffers->getUniformBufferMemory(), 0, sizeof(UniformBufferObject));
	memcpy(memory, &uniformBufferObject, sizeof(UniformBufferObject));
	context->getDevice()->unmapMemory(*buffers->getUniformBufferMemory());
#endif
}

void Renderer::render()
{
#ifdef MK_OPTIMIZATION_PUSH_CONSTANTS
	// we need to re-record the command buffers every frame for push constants to update
	swapchain->recordCommandBuffers(pipeline, buffers);
#endif

	auto nextImage = context->getDevice()->acquireNextImageKHR(*swapchain->getSwapchain(), std::numeric_limits<uint64_t>::max(), *semaphores->getImageAvailableSemaphore(), nullptr);

	if (nextImage.result != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to acquire next image for rendering.");
	}

	auto imageIndex = nextImage.value;
	auto stageFlags = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	auto submitInfo = vk::SubmitInfo().setWaitSemaphoreCount(1).setPWaitSemaphores(semaphores->getImageAvailableSemaphore()).setPWaitDstStageMask(&stageFlags);
	submitInfo.setCommandBufferCount(1).setPCommandBuffers(swapchain->getCommandBuffer(imageIndex)).setSignalSemaphoreCount(1).setPSignalSemaphores(semaphores->getRenderFinishedSemaphore());
	context->getQueue().submit({ submitInfo }, nullptr);

	auto presentInfo = vk::PresentInfoKHR().setWaitSemaphoreCount(1).setPWaitSemaphores(semaphores->getRenderFinishedSemaphore());
	presentInfo.setSwapchainCount(1).setPSwapchains(swapchain->getSwapchain()).setPImageIndices(&imageIndex);
	if (context->getQueue().presentKHR(presentInfo) != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to present queue.");
	}

	waitForIdle();
}

void Renderer::waitForIdle()
{
	context->getQueue().waitIdle();
}