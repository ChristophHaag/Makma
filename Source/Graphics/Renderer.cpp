#include "Renderer.hpp"

#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // bring the depth range from [-1,1] (OpenGL) to [0,1] (Vulkan)
#include <gtc\matrix_transform.hpp>

Renderer::Renderer(const std::shared_ptr<Window> window, const std::shared_ptr<Camera> camera)
{
	this->window = window;
	this->camera = camera;

	context = std::make_shared<Context>(window);
	
	Material::loadDefaultTextures(context);

	buffers = std::make_shared<Buffers>(context);
	models.push_back(new Model(context, buffers, "Models\\Sponza\\", "Sponza.fbx"));
	models.push_back(new Model(context, buffers, "Models\\OldMan\\", "OldMan.fbx"));
	models.push_back(new Model(context, buffers, "Models\\Machinegun\\", "Machinegun.fbx"));
	models.push_back(new Model(context, buffers, "Models\\HeavyTank\\", "HeavyTank.fbx"));
	buffers->finalize(static_cast<uint32_t>(models.size()));

	// position the heavy tank
	models.at(3)->position += models.at(3)->getUp() * 115.0f;
	models.at(3)->position += models.at(3)->getRight() * 1000.0f;
	models.at(3)->position += models.at(3)->getForward() * 15.0f;
	models.at(3)->setScale(glm::vec3(400.0f, 400.0f, 400.0f));
	models.at(3)->setYaw(-115.0f);

	descriptor = std::make_shared<Descriptor>(context, buffers, Material::getNumMaterials());

	for (auto &model : models)
	{
		model->finalizeMaterials(descriptor);
	}

	directionalLights.push_back(new DirectionalLight(glm::vec3(0.0f, -0.5f, -1.0f), glm::vec3(1.0f, 0.75f, 0.5f)));
	directionalLights.push_back(new DirectionalLight(glm::vec3(0.0f, -0.5f, 1.0f), glm::vec3(0.5f, 0.75f, 1.0f)));
	directionalLights.push_back(new DirectionalLight(glm::vec3(-1.0f, -0.5f, 0.0f), glm::vec3(1.0f, 0.75f, 0.5f)));
	directionalLights.push_back(new DirectionalLight(glm::vec3(1.0f, -0.5f, 0.0f), glm::vec3(0.5f, 0.75f, 1.0f)));

	geometryBuffer = std::make_shared<GeometryBuffer>(window, context, descriptor);
	geometryPipeline = std::make_shared<GeometryPipeline>(window, context, buffers, descriptor, geometryBuffer->getRenderPass());

	swapchain = std::make_unique<Swapchain>(window, context);
	lightingPipeline = std::make_shared<LightingPipeline>(window, context, descriptor, swapchain->getRenderPass());
	swapchain->recordCommandBuffers(lightingPipeline, geometryBuffer, descriptor, buffers, &directionalLights);

#ifndef MK_OPTIMIZATION_PUSH_CONSTANTS
	// just record the command buffers once if we are not using push constants
	geometryBuffer->recordCommandBuffer(geometryPipeline, buffers, &models, camera);

	buffers->getViewProjectionData()->projectionMatrix = *camera.get()->getProjectionMatrix();
	buffers->getViewProjectionData()->projectionMatrix[1][1] *= -1.0f;
#endif

	semaphores = std::make_unique<Semaphores>(context);
}

void Renderer::update(float delta)
{
	models.at(1)->setYaw(models.at(1)->getYaw() + delta * 0.1f);
	
	/*
	// values for the pistol
	models->at(2)->position = camera->position - camera->getForward() * 22.5f - camera->getUp() * 12.0f + camera->getRight() * 4.0f;
	models->at(2)->setYaw(camera->getYaw());
	models->at(2)->setPitch(camera->getPitch() - 90.0f);
	models->at(2)->setRoll(camera->getRoll());
	*/

	// values for the m249
	models.at(2)->position = camera->position + camera->getForward() * 150.0f - camera->getUp() * 75.0f - camera->getRight() * 40.0f;
	models.at(2)->setYaw(camera->getYaw());
	models.at(2)->setPitch(camera->getPitch() - 90.0f);
	models.at(2)->setRoll(camera->getRoll());

	directionalLights[0]->color.r -= delta * 0.001f;
	directionalLights[1]->color.g -= delta * 0.001f;
	directionalLights[2]->color.b -= delta * 0.001f;
	if (directionalLights[0]->color.r < 0.0f) directionalLights[0]->color.r = 1.0f;
	if (directionalLights[1]->color.g < 0.0f) directionalLights[1]->color.g = 1.0f;
	if (directionalLights[2]->color.b < 0.0f) directionalLights[2]->color.b = 1.0f;

#ifndef MK_OPTIMIZATION_PUSH_CONSTANTS

	// world matrix

	auto memory = context->getDevice()->mapMemory(*buffers->getWorldUniformBufferMemory(), 0, sizeof(WorldData));
	for (size_t i = 0; i < models.size(); ++i)
	{
		auto dst = ((char*)memory) + i * buffers->getWorldDataAlignment();
		memcpy(dst, &models.at(i)->getWorldMatrix(), sizeof(glm::mat4));
	}

	auto memoryRange = vk::MappedMemoryRange().setMemory(*buffers->getWorldUniformBufferMemory()).setSize(sizeof(WorldData));
	context->getDevice()->flushMappedMemoryRanges(1, &memoryRange);
	context->getDevice()->unmapMemory(*buffers->getWorldUniformBufferMemory());


	// light data

	memory = context->getDevice()->mapMemory(*buffers->getLightUniformBufferMemory(), 0, sizeof(LightData));
	for (size_t i = 0; i < directionalLights.size(); ++i)
	{
		auto directionalLight = directionalLights.at(i);

		glm::mat4 lightData;
		lightData[0] = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // position
		lightData[1] = glm::vec4(directionalLight->direction, 0.0f); // direction
		lightData[2] = glm::vec4(directionalLight->color, 1.0f); // color
		lightData[3] = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // unused
		
		auto dst = ((char*)memory) + i * buffers->getLightDataAlignment();
		memcpy(dst, &lightData, sizeof(glm::mat4));
	}

	memoryRange = vk::MappedMemoryRange().setMemory(*buffers->getLightUniformBufferMemory()).setSize(sizeof(LightData));
	context->getDevice()->flushMappedMemoryRanges(1, &memoryRange);
	context->getDevice()->unmapMemory(*buffers->getLightUniformBufferMemory());


	// view and projection matrices

	buffers->getViewProjectionData()->viewMatrix = *camera.get()->getViewMatrix();
	memory = context->getDevice()->mapMemory(*buffers->getViewProjectionUniformBufferMemory(), 0, sizeof(ViewProjectionData));
	memcpy(memory, buffers->getViewProjectionData(), sizeof(ViewProjectionData));
	context->getDevice()->unmapMemory(*buffers->getViewProjectionUniformBufferMemory());
#endif
}

void Renderer::render()
{
	// geometry pass

#ifdef MK_OPTIMIZATION_PUSH_CONSTANTS
	geometryBuffer->recordCommandBuffer(geometryPipeline, buffers, &models, camera);
#endif

	auto submitInfo = vk::SubmitInfo().setSignalSemaphoreCount(1).setPSignalSemaphores(semaphores->getGeometryPassDoneSemaphore());
	submitInfo.setCommandBufferCount(1).setPCommandBuffers(geometryBuffer->getCommandBuffer());
	context->getQueue().submit({ submitInfo }, nullptr);

	// lighting pass
	
	auto nextImage = context->getDevice()->acquireNextImageKHR(*swapchain->getSwapchain(), std::numeric_limits<uint64_t>::max(), *semaphores->getImageAvailableSemaphore(), nullptr);
	if (nextImage.result != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to acquire next image for rendering.");
	}

	auto imageIndex = nextImage.value;
	vk::PipelineStageFlags stageFlags[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput };
	std::vector<vk::Semaphore> waitingForSemaphores = { *semaphores->getGeometryPassDoneSemaphore(), *semaphores->getImageAvailableSemaphore() };
	submitInfo.setWaitSemaphoreCount(static_cast<uint32_t>(waitingForSemaphores.size())).setPWaitSemaphores(waitingForSemaphores.data()).setPWaitDstStageMask(stageFlags);
	submitInfo.setPCommandBuffers(swapchain->getCommandBuffer(imageIndex)).setPSignalSemaphores(semaphores->getLightingPassDoneSemaphore());
	context->getQueue().submit({ submitInfo }, nullptr);


	// present

	auto presentInfo = vk::PresentInfoKHR().setWaitSemaphoreCount(1).setPWaitSemaphores(semaphores->getLightingPassDoneSemaphore());
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