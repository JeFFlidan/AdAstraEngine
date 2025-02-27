#include "vulkan_command_manager.h"
#include "vulkan_queue.h"
#include "vulkan_common.h"
#include <utility>

using namespace ad_astris;

vulkan::VulkanCommandBuffer::VulkanCommandBuffer(
	VulkanDevice* device,
	VkCommandPool pool,
	VkPipelineStageFlags2 waitFlag) : _device(device), _stageFlag(waitFlag)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	allocInfo.commandPool = pool;
	VK_CHECK(vkAllocateCommandBuffers(_device->get_device(), &allocInfo, &_cmdBuffer));

	create_semaphore(_device->get_device(), &_signalSemaphore);
}

vulkan::VulkanCommandBuffer::~VulkanCommandBuffer()
{
	vkDestroySemaphore(_device->get_device(), _signalSemaphore, nullptr);
}

vulkan::VulkanCommandPool::VulkanCommandPool(VulkanDevice* device, VulkanQueue* queue) : _device(device)
{
	VkCommandPoolCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.flags = 0;
	info.queueFamilyIndex = queue->get_family();
	VK_CHECK(vkCreateCommandPool(_device->get_device(), &info, nullptr, &_cmdPool));

	// Need to think about flags
	switch (queue->get_type())
	{
		case rhi::QueueType::GRAPHICS:
		{
			_waitFlag = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;
		}
		case rhi::QueueType::COMPUTE:
		{
			_waitFlag = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			break;
		}
		case rhi::QueueType::TRANSFER:
		{
			_waitFlag = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			break;
		}
	}
}

vulkan::VulkanCommandPool::~VulkanCommandPool()
{
	vkDestroyCommandPool(_device->get_device(), _cmdPool, nullptr);
}

vulkan::VulkanCommandBuffer* vulkan::VulkanCommandPool::get_cmd_buffer()
{
	VulkanCommandBuffer* cmdBuffer;
	if (!_freeCmdBuffers.empty())
	{
		cmdBuffer = _freeCmdBuffers.back().get();
		_usedCmdBuffers.emplace_back(std::move(_freeCmdBuffers.back()));
		_freeCmdBuffers.pop_back();
	}
	else
	{
		_usedCmdBuffers.emplace_back(new VulkanCommandBuffer(_device, _cmdPool, _waitFlag));
		cmdBuffer = _usedCmdBuffers.back().get();
	}
	return cmdBuffer;
}

void vulkan::VulkanCommandPool::clear_after_submission()
{
	for (auto& cmdBuffer : _usedCmdBuffers)
	{
		cmdBuffer->_waitSemaphores.clear();
		cmdBuffer->_waitFlags.clear();
	}
	_submittedCmdBuffers = std::move(_usedCmdBuffers);			// TODO FIX BUG
}

void vulkan::VulkanCommandPool::flush_submitted_cmd_buffers()
{
	if (_submittedCmdBuffers.empty())
		return;
	vkResetCommandPool(_device->get_device(), _cmdPool, 0);
	//_freeCmdBuffers = std::move(_submittedCmdBuffers);
	// TODO Think how to make it faster
	for (auto& cmdBuffer : _submittedCmdBuffers)
	{
		_freeCmdBuffers.push_back(std::move(cmdBuffer));
		_submittedCmdBuffers.pop_back();
	}
}

vulkan::VulkanCommandManager::VulkanCommandManager(VulkanDevice* device, VulkanSwapChain* swapChain) : _device(device)
{
	VulkanQueue* graphicsQueue = _device->get_graphics_queue();
	VulkanQueue* computeQueue = _device->get_compute_queue();
	VulkanQueue* transferQueue = _device->get_transfer_queue();

	_bufferCount = swapChain->get_buffers_count();
	size_t poolsPerQueue = RENDER_THREAD_COUNT * _bufferCount;
	
	for (int i = 0; i != 3; ++i)
	{
		std::vector<std::unique_ptr<VulkanCommandPool>> tempPools;
		for (size_t j = 1; j <= poolsPerQueue; ++j)
		{
			switch (i)
			{
				case 0:
				{
					tempPools.emplace_back(new VulkanCommandPool(_device, graphicsQueue));
					if (tempPools.size() == poolsPerQueue / _bufferCount)
					{
						_freeGraphicsCmdPools.push_back(std::move(tempPools));
						//tempPools.clear();
					}
					break;
				}
				case 1:
				{
					tempPools.emplace_back(new VulkanCommandPool(_device, computeQueue));
					if (tempPools.size() == poolsPerQueue / _bufferCount)
					{
						_freeComputeCmdPools.push_back(std::move(tempPools));
						//tempPools.clear();
					}
					break;
				}
				case 2:
				{
					tempPools.emplace_back(new VulkanCommandPool(_device, transferQueue));
					if (tempPools.size() == poolsPerQueue / _bufferCount)
					{
						_freeTransferCmdPools.push_back(std::move(tempPools));
						//tempPools.clear();
					}
					break;
				}
			}
		}
	}
	LOG_INFO("First frame graphics: {}", _freeGraphicsCmdPools[0].size())
	LOG_INFO("Second frame graphics: {}", _freeGraphicsCmdPools[1].size())

	LOG_INFO("First frame transsfer: {}", _freeTransferCmdPools[0].size())
	LOG_INFO("Second frame transfer: {}", _freeTransferCmdPools[1].size())

	LOG_INFO("First frame compute: {}", _freeComputeCmdPools[0].size())
	LOG_INFO("Second frame compute: {}", _freeComputeCmdPools[1].size())

	_lockedGraphicsCmdPools.resize(_bufferCount);
	_lockedComputeCmdPools.resize(_bufferCount);
	_lockedTransferCmdPools.resize(_bufferCount);

	_syncManager.init(_device, _bufferCount);
}

void vulkan::VulkanCommandManager::cleanup()
{
	_syncManager.cleanup(_device, _bufferCount);
	for (uint32_t i = 0; i != _bufferCount; ++i)
	{
		_freeGraphicsCmdPools[i].clear();
		_freeComputeCmdPools[i].clear();
		_freeTransferCmdPools[i].clear();
		_lockedComputeCmdPools[i].clear();
		_lockedGraphicsCmdPools[i].clear();
		_lockedTransferCmdPools[i].clear();
	}
}

void vulkan::VulkanCommandManager::reset_cmd_buffers(uint32_t frameIndex)
{
	_frameIndex = frameIndex;
	_syncManager.wait_fences(_device, _frameIndex);
	flush_cmd_buffers();
	_device->get_graphics_queue()->cleanup_present_wait_semaphores();
}

vulkan::VulkanCommandBuffer* vulkan::VulkanCommandManager::get_command_buffer(rhi::QueueType queueType)
{
	VulkanCommandBuffer* cmdBuffer;
	switch (queueType)
	{
		case rhi::QueueType::GRAPHICS:
		{
			if (_freeGraphicsCmdPools[_frameIndex].empty())
			{
				LOG_FATAL("VulkanCommandManage::get_command_buffer(): Can't dedicate more than 4 threads")
			}

			cmdBuffer = _freeGraphicsCmdPools[_frameIndex].back()->get_cmd_buffer();
			_lockedGraphicsCmdPools[_frameIndex].push_back(std::move(_freeGraphicsCmdPools[_frameIndex].back()));
			_freeGraphicsCmdPools[_frameIndex].pop_back();
			cmdBuffer->_waitFlags.push_back(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
			break;
		}
		case rhi::QueueType::COMPUTE:
		{
			// TODO Think about adding acquire semaphore and pipeline flags
			if (_freeComputeCmdPools[_frameIndex].empty())
			{
				LOG_FATAL("VulkanCommandManage::get_command_buffer(): Can't dedicate more than 4 threads")
			}

			cmdBuffer = _freeComputeCmdPools[_frameIndex].back()->get_cmd_buffer();
			_lockedComputeCmdPools[_frameIndex].push_back(std::move(_freeComputeCmdPools[_frameIndex].back()));
			_freeComputeCmdPools[_frameIndex].pop_back();
			cmdBuffer->_waitFlags.push_back(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
			break;
		}
		case rhi::QueueType::TRANSFER:
		{
			if (_freeTransferCmdPools[_frameIndex].empty())
			{
				LOG_FATAL("VulkanCommandManage::get_command_buffer(): Can't dedicate more than 4 threads")
			}
			
			cmdBuffer = _freeTransferCmdPools[_frameIndex].back()->get_cmd_buffer();
			_lockedTransferCmdPools[_frameIndex].push_back(std::move(_freeTransferCmdPools[_frameIndex].back()));
			_freeTransferCmdPools[_frameIndex].pop_back();
			cmdBuffer->_waitFlags.push_back(VK_PIPELINE_STAGE_2_TRANSFER_BIT);
			break;
		}
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pInheritanceInfo = nullptr;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmdBuffer->get_handle(), &beginInfo);
	return cmdBuffer;
}

void vulkan::VulkanCommandManager::submit(rhi::QueueType queueType, bool useSignalSemaphores)
{
	switch (queueType)
	{
		case rhi::QueueType::GRAPHICS:
		{
			_device->get_graphics_queue()->submit(*this, useSignalSemaphores);
			auto& lockedPools = _lockedGraphicsCmdPools[_frameIndex];
			auto& freePools = _freeGraphicsCmdPools[_frameIndex];
			clear_after_submission(freePools, lockedPools);
			break;
		}
		case rhi::QueueType::COMPUTE:
		{
			_device->get_compute_queue()->submit(*this, useSignalSemaphores);
			auto& lockedPools = _lockedComputeCmdPools[_frameIndex];
			auto& freePools = _freeComputeCmdPools[_frameIndex];
			clear_after_submission(freePools, lockedPools);
			break;
		}
		case rhi::QueueType::TRANSFER:
		{
			_device->get_transfer_queue()->submit(*this, useSignalSemaphores);
			auto& lockedPools = _lockedTransferCmdPools[_frameIndex];
			auto& freePools = _freeTransferCmdPools[_frameIndex];
			clear_after_submission(freePools, lockedPools);
			break;
		}
	}
}

VkFence vulkan::VulkanCommandManager::get_free_fence()
{
	return _syncManager.get_free_fence(_device, _frameIndex);
}

void vulkan::VulkanCommandManager::wait_fences()
{
	_syncManager.wait_fences(_device, _frameIndex);
}

void vulkan::VulkanCommandManager::wait_all_fences()
{
	for (uint32_t i = 0; i != _bufferCount; ++i)
	{
		_syncManager.wait_fences(_device, i);
	}
}

void vulkan::VulkanCommandManager::flush_cmd_buffers()
{
	auto& graphicsPools = _freeGraphicsCmdPools[_frameIndex];
	auto& transferPools = _freeTransferCmdPools[_frameIndex];
	auto& computePools = _freeComputeCmdPools[_frameIndex];

	for (int i = 0; i != RENDER_THREAD_COUNT; ++i)
	{
		graphicsPools[i]->flush_submitted_cmd_buffers();
		transferPools[i]->flush_submitted_cmd_buffers();
		computePools[i]->flush_submitted_cmd_buffers();
	}
}

void vulkan::VulkanCommandManager::SynchronizationManager::init(VulkanDevice* device, uint32_t bufferCount)
{
	_freeFences.resize(bufferCount);
	_lockedFences.resize(bufferCount);
}

void vulkan::VulkanCommandManager::SynchronizationManager::cleanup(VulkanDevice* device, uint32_t bufferCount)
{
	for (uint32_t i = 0; i != bufferCount; ++i)
	{
		auto& fences = _freeFences[i];
		for (auto& fence : fences)
			vkDestroyFence(device->get_device(), fence, nullptr);
		fences = _lockedFences[i];
		for (auto& fence : fences)
			vkDestroyFence(device->get_device(), fence, nullptr);
	}
}

void vulkan::VulkanCommandManager::SynchronizationManager::wait_fences(VulkanDevice* device, uint32_t bufferIndex)
{
	//LOG_INFO("LOCKED FENCES COUNT: {}", _lockedFences[bufferIndex].size())
	if (!_lockedFences[bufferIndex].empty())
	{
		VK_CHECK(vkWaitForFences(device->get_device(), _lockedFences[bufferIndex].size(), _lockedFences[bufferIndex].data(), true, 1000000000));
		VK_CHECK(vkResetFences(device->get_device(), _lockedFences[bufferIndex].size(), _lockedFences[bufferIndex].data()));
		for (auto& fence : _lockedFences[bufferIndex])
			_freeFences[bufferIndex].push_back(fence);
		_lockedFences[bufferIndex].clear();
	}
}

VkFence vulkan::VulkanCommandManager::SynchronizationManager::get_free_fence(VulkanDevice* device, uint32_t bufferIndex)
{
	std::vector<VkFence>& freeFences = _freeFences[bufferIndex];
	if (freeFences.empty())
		freeFences.push_back(create_fence(device->get_device()));
	VkFence fence = _freeFences[bufferIndex].back();
	_lockedFences[bufferIndex].push_back(fence);
	_freeFences[bufferIndex].pop_back();
	return fence;
}

void vulkan::clear_after_submission(
	std::vector<std::unique_ptr<VulkanCommandPool>>& freePools,
	std::vector<std::unique_ptr<VulkanCommandPool>>& lockedPools)
{
	for (auto& pool : lockedPools)
	{
		pool->clear_after_submission();
		freePools.push_back(std::move(pool));
	}
	lockedPools.clear();
}
