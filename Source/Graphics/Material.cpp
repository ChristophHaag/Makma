#include "Material.hpp"

std::vector<std::shared_ptr<Material>> Material::materials;
uint32_t Material::numMaterials = 0;

vk::DescriptorSet *Material::createDescriptorSet(const std::shared_ptr<Context> context, const std::shared_ptr<Descriptor> descriptor, const Texture *diffuseTexture, const Texture *normalTexture)
{
	auto descriptorSetAllocateInfo = vk::DescriptorSetAllocateInfo().setDescriptorPool(*descriptor->getDescriptorPool()).setDescriptorSetCount(1).setPSetLayouts(descriptor->getMaterialDescriptorSetLayout());
	auto descriptorSet = context->getDevice()->allocateDescriptorSets(descriptorSetAllocateInfo).at(0);

	auto diffuseDescriptorImageInfo = vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(*diffuseTexture->getImageView()).setSampler(*diffuseTexture->getSampler());
	auto diffuseSamplerWriteDescriptorSet = vk::WriteDescriptorSet().setDstBinding(0).setDstSet(descriptorSet).setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
	diffuseSamplerWriteDescriptorSet.setDescriptorCount(1).setPImageInfo(&diffuseDescriptorImageInfo);

	auto normalDescriptorImageInfo = vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(*normalTexture->getImageView()).setSampler(*normalTexture->getSampler());
	auto normalSamplerWriteDescriptorSet = vk::WriteDescriptorSet().setDstBinding(1).setDstSet(descriptorSet).setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
	normalSamplerWriteDescriptorSet.setDescriptorCount(1).setPImageInfo(&normalDescriptorImageInfo);

	std::vector<vk::WriteDescriptorSet> writeDescriptorSets = { diffuseSamplerWriteDescriptorSet, normalSamplerWriteDescriptorSet };
	context->getDevice()->updateDescriptorSets(static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	return new vk::DescriptorSet(descriptorSet);
}

Material::Material(const std::shared_ptr<Context> context, const std::string &name, std::string &diffuseTextureFilename, const std::string &normalTextureFilename)
{
	this->context = context;
	this->name = name;

	diffuseTexture = std::make_unique<Texture>(context, diffuseTextureFilename);
	normalTexture = std::make_unique<Texture>(context, normalTextureFilename);

	isFinalized = false;
}

std::shared_ptr<Material> Material::cacheMaterial(const std::shared_ptr<Context> context, const std::string &name, std::string &diffuseTextureFilename, const std::string &normalTextureFilename)
{
	for (auto &material : materials)
	{
		if (material->name.compare(name) == 0)
		{
			return material;
		}
	}

	auto newMaterial = std::make_shared<Material>(context, name, diffuseTextureFilename, normalTextureFilename);
	materials.push_back(newMaterial);
	numMaterials++;
	return newMaterial;
}

void Material::finalize(const std::shared_ptr<Descriptor> descriptor)
{
	if (!isFinalized)
	{
		descriptorSet = std::unique_ptr<vk::DescriptorSet>(createDescriptorSet(context, descriptor, diffuseTexture.get(), normalTexture.get()));
		isFinalized = true;
	}
}