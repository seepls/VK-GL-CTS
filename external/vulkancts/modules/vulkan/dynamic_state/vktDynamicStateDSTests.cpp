/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Dynamic State Depth Stencil Tests 
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateDSTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktDynamicStateTestCaseUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "vkRefUtil.hpp"
#include "vkImageUtil.hpp"

#include "vktDynamicStateCreateInfoUtil.hpp"
#include "vktDynamicStateImageObjectUtil.hpp"
#include "vktDynamicStateBufferObjectUtil.hpp"
#include "vkPrograms.hpp"

namespace vkt
{
namespace DynamicState
{

namespace
{

class DepthStencilBaseCase : public TestInstance
{
public:
	DepthStencilBaseCase (Context& context, const char* vertexShaderName, const char* fragmentShaderName)
		: TestInstance						(context)
		, m_colorAttachmentFormat			(vk::VK_FORMAT_R8G8B8A8_UNORM)
		, m_depthStencilAttachmentFormat	(vk::VK_FORMAT_D24_UNORM_S8_UINT)
		, m_topology						(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
		, m_vertexShaderName				(vertexShaderName)
		, m_fragmentShaderName				(fragmentShaderName)
		, m_vk								(context.getDeviceInterface())
	{
	}

protected:

	enum 
	{
		WIDTH   = 128,
		HEIGHT  = 128
	};

	vk::VkFormat									m_colorAttachmentFormat;
	vk::VkFormat									m_depthStencilAttachmentFormat;

	vk::VkPrimitiveTopology							m_topology;

	const vk::DeviceInterface&						m_vk;

	vk::Move<vk::VkPipeline>						m_pipeline_1;
	vk::Move<vk::VkPipeline>						m_pipeline_2;
	vk::Move<vk::VkPipelineLayout>					m_pipelineLayout;

	de::SharedPtr<Image>							m_colorTargetImage;
	vk::Move<vk::VkImageView>						m_colorTargetView;
		
	de::SharedPtr<Image>							m_depthStencilImage;
	vk::Move<vk::VkImageView>						m_attachmentView;

	PipelineCreateInfo::VertexInputState			m_vertexInputState;
	de::SharedPtr<Buffer>							m_vertexBuffer;

	vk::Move<vk::VkCommandPool>						m_cmdPool;
	vk::Move<vk::VkCommandBuffer>					m_cmdBuffer;

	vk::Move<vk::VkFramebuffer>						m_framebuffer;
	vk::Move<vk::VkRenderPass>						m_renderPass;

	const std::string								m_vertexShaderName;
	const std::string								m_fragmentShaderName;

	std::vector<PositionColorVertex>				m_data;

	PipelineCreateInfo::DepthStencilState			m_depthStencilState_1;
	PipelineCreateInfo::DepthStencilState			m_depthStencilState_2;

	void initialize (void)
	{
		tcu::TestLog &log = m_context.getTestContext().getLog();
		const vk::VkDevice device = m_context.getDevice();

		const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		m_pipelineLayout = vk::createPipelineLayout(m_vk, device, &pipelineLayoutCreateInfo);

		const vk::Unique<vk::VkShaderModule> vs(createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_vertexShaderName), 0));
		const vk::Unique<vk::VkShaderModule> fs(createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_fragmentShaderName), 0));

		const vk::VkExtent3D imageExtent = { WIDTH, HEIGHT, 1 };
		const ImageCreateInfo targetImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT, 
													vk::VK_IMAGE_TILING_OPTIMAL, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		m_colorTargetImage = Image::createAndAlloc(m_vk, device, targetImageCreateInfo, m_context.getDefaultAllocator());

		const ImageCreateInfo depthStencilImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_depthStencilAttachmentFormat, imageExtent,
														  1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
														  vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

		m_depthStencilImage = Image::createAndAlloc(m_vk, device, depthStencilImageCreateInfo, m_context.getDefaultAllocator());

		const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
		m_colorTargetView = vk::createImageView(m_vk, device, &colorTargetViewInfo);

		const ImageViewCreateInfo attachmentViewInfo(m_depthStencilImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_depthStencilAttachmentFormat);
		m_attachmentView = vk::createImageView(m_vk, device, &attachmentViewInfo);

		RenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,
																 vk::VK_SAMPLE_COUNT_1_BIT,
																 vk::VK_ATTACHMENT_LOAD_OP_LOAD,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_IMAGE_LAYOUT_GENERAL,
																 vk::VK_IMAGE_LAYOUT_GENERAL));

		renderPassCreateInfo.addAttachment(AttachmentDescription(m_depthStencilAttachmentFormat,
																 vk::VK_SAMPLE_COUNT_1_BIT,
																 vk::VK_ATTACHMENT_LOAD_OP_LOAD,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

		const vk::VkAttachmentReference colorAttachmentReference =
		{
			0,
			vk::VK_IMAGE_LAYOUT_GENERAL
		};

		const vk::VkAttachmentReference depthAttachmentReference =
		{
			1,
			vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(
			vk::VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			0,
			DE_NULL,
			1,
			&colorAttachmentReference,
			DE_NULL,
			depthAttachmentReference,
			0,
			DE_NULL));

		m_renderPass = vk::createRenderPass(m_vk, device, &renderPassCreateInfo);

		const vk::VkVertexInputBindingDescription vertexInputBindingDescription =
		{
			0,
			sizeof(tcu::Vec4) * 2,
			vk::VK_VERTEX_INPUT_RATE_VERTEX,
		};

		const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
		{
			{
				0u,
				0u,
				vk::VK_FORMAT_R32G32B32A32_SFLOAT,
				0u
			},
			{
				1u,
				0u,
				vk::VK_FORMAT_R32G32B32A32_SFLOAT,
				(deUint32)(sizeof(float)* 4),
			}
		};

		m_vertexInputState = PipelineCreateInfo::VertexInputState(
			1,
			&vertexInputBindingDescription,
			2,
			vertexInputAttributeDescriptions);

		const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

		PipelineCreateInfo pipelineCreateInfo_1(*m_pipelineLayout, *m_renderPass, 0, 0);
		pipelineCreateInfo_1.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
		pipelineCreateInfo_1.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));
		pipelineCreateInfo_1.addState(PipelineCreateInfo::VertexInputState(m_vertexInputState));
		pipelineCreateInfo_1.addState(PipelineCreateInfo::InputAssemblerState(m_topology));
		pipelineCreateInfo_1.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
		pipelineCreateInfo_1.addState(PipelineCreateInfo::ViewportState(1));
		pipelineCreateInfo_1.addState(m_depthStencilState_1);
		pipelineCreateInfo_1.addState(PipelineCreateInfo::RasterizerState());
		pipelineCreateInfo_1.addState(PipelineCreateInfo::MultiSampleState());
		pipelineCreateInfo_1.addState(PipelineCreateInfo::DynamicState());

		PipelineCreateInfo pipelineCreateInfo_2(*m_pipelineLayout, *m_renderPass, 0, 0);
		pipelineCreateInfo_2.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
		pipelineCreateInfo_2.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));
		pipelineCreateInfo_2.addState(PipelineCreateInfo::VertexInputState(m_vertexInputState));
		pipelineCreateInfo_2.addState(PipelineCreateInfo::InputAssemblerState(m_topology));
		pipelineCreateInfo_2.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
		pipelineCreateInfo_2.addState(PipelineCreateInfo::ViewportState(1));
		pipelineCreateInfo_2.addState(m_depthStencilState_2);
		pipelineCreateInfo_2.addState(PipelineCreateInfo::RasterizerState());
		pipelineCreateInfo_2.addState(PipelineCreateInfo::MultiSampleState());
		pipelineCreateInfo_2.addState(PipelineCreateInfo::DynamicState());

		m_pipeline_1 = vk::createGraphicsPipeline(m_vk, device, DE_NULL, &pipelineCreateInfo_1);
		m_pipeline_2 = vk::createGraphicsPipeline(m_vk, device, DE_NULL, &pipelineCreateInfo_2);

		std::vector<vk::VkImageView> attachments(2);
		attachments[0] = *m_colorTargetView;
		attachments[1] = *m_attachmentView;

		const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);

		m_framebuffer = vk::createFramebuffer(m_vk, device, &framebufferCreateInfo);

		const vk::VkDeviceSize dataSize = m_data.size() * sizeof(PositionColorVertex);
		m_vertexBuffer = Buffer::createAndAlloc(m_vk, device, BufferCreateInfo(dataSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
												m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

		deUint8* ptr = reinterpret_cast<unsigned char *>(m_vertexBuffer->getBoundMemory().getHostPtr());
		deMemcpy(ptr, &m_data[0], dataSize);

		vk::flushMappedMemoryRange(m_vk, device,
			m_vertexBuffer->getBoundMemory().getMemory(),
			m_vertexBuffer->getBoundMemory().getOffset(),
			sizeof(dataSize));

		const CmdPoolCreateInfo cmdPoolCreateInfo(m_context.getUniversalQueueFamilyIndex());
		m_cmdPool = vk::createCommandPool(m_vk, device, &cmdPoolCreateInfo);

		const vk::VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			*m_cmdPool,											// VkCommandPool			commandPool;
			vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u,													// deUint32					bufferCount;
		};
		m_cmdBuffer = vk::allocateCommandBuffer(m_vk, device, &cmdBufferAllocateInfo);
	}

	virtual tcu::TestStatus iterate (void)
	{
		DE_ASSERT(false);
		return tcu::TestStatus::fail("Implement iterate() method!");
	}

	void beginRenderPass (void)
	{
		const vk::VkClearColorValue clearColor = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		beginRenderPassWithClearColor(clearColor);
	}

	void beginRenderPassWithClearColor (const vk::VkClearColorValue &clearColor)
	{
		const CmdBufferBeginInfo beginInfo;
		m_vk.beginCommandBuffer(*m_cmdBuffer, &beginInfo);

		initialTransitionColor2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL);
		initialTransitionDepthStencil2DImage(m_vk, *m_cmdBuffer, m_depthStencilImage->object(), vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		const ImageSubresourceRange subresourceRangeImage(vk::VK_IMAGE_ASPECT_COLOR_BIT);
		m_vk.cmdClearColorImage(*m_cmdBuffer, m_colorTargetImage->object(),
			vk::VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresourceRangeImage);

		const vk::VkClearDepthStencilValue depthStencilClearValue = { 0.0f, 0 };

		const ImageSubresourceRange subresourceRangeDepthStencil[2] = { vk::VK_IMAGE_ASPECT_DEPTH_BIT, vk::VK_IMAGE_ASPECT_STENCIL_BIT };
		m_vk.cmdClearDepthStencilImage(*m_cmdBuffer, m_depthStencilImage->object(),
			vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depthStencilClearValue, 2, subresourceRangeDepthStencil);

		const vk::VkRect2D renderArea = { { 0, 0 }, { WIDTH, HEIGHT } };
		const RenderPassBeginInfo renderPassBegin(*m_renderPass, *m_framebuffer, renderArea);

		m_vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBegin, vk::VK_SUBPASS_CONTENTS_INLINE);
	}

	void setDynamicViewportState (const deUint32 width, const deUint32 height)
	{
		vk::VkViewport viewport;
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(width);
		viewport.height = static_cast<float>(height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		m_vk.cmdSetViewport(*m_cmdBuffer, 1, &viewport);

		vk::VkRect2D scissor;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = width;
		scissor.extent.height = height;
		m_vk.cmdSetScissor(*m_cmdBuffer, 1, &scissor);
	}

	void setDynamicViewportState(const deUint32 viewportCount, const vk::VkViewport* pViewports, const vk::VkRect2D* pScissors)
	{
		m_vk.cmdSetViewport(*m_cmdBuffer, viewportCount, pViewports);
		m_vk.cmdSetScissor(*m_cmdBuffer, viewportCount, pScissors);
	}

	void setDynamicRasterizationState(const float lineWidth = 1.0f,
							   const float depthBiasConstantFactor = 0.0f,
							   const float depthBiasClamp = 0.0f,
							   const float depthBiasSlopeFactor = 0.0f)
	{
		m_vk.cmdSetLineWidth(*m_cmdBuffer, lineWidth);
		m_vk.cmdSetDepthBias(*m_cmdBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
	}

	void setDynamicBlendState(const float const1 = 0.0f, const float const2 = 0.0f,
							  const float const3 = 0.0f, const float const4 = 0.0f)
	{
		float blendConstantsants[4] = { const1, const2, const3, const4 };
		m_vk.cmdSetBlendConstants(*m_cmdBuffer, blendConstantsants);
	}

	void setDynamicDepthStencilState(const float minDepthBounds = -1.0f, 
									 const float maxDepthBounds = 1.0f,
									 const deUint32 stencilFrontCompareMask = 0xffffffffu, 
									 const deUint32 stencilFrontWriteMask = 0xffffffffu,
									 const deUint32 stencilFrontReference = 0, 
									 const deUint32 stencilBackCompareMask = 0xffffffffu,
									 const deUint32 stencilBackWriteMask = 0xffffffffu, 
									 const deUint32 stencilBackReference = 0)
	{
		m_vk.cmdSetDepthBounds(*m_cmdBuffer, minDepthBounds, maxDepthBounds);
		m_vk.cmdSetStencilCompareMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontCompareMask);
		m_vk.cmdSetStencilWriteMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontWriteMask);
		m_vk.cmdSetStencilReference(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontReference);
		m_vk.cmdSetStencilCompareMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackCompareMask);
		m_vk.cmdSetStencilWriteMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackWriteMask);
		m_vk.cmdSetStencilReference(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackReference);
	}
};

class DepthBoundsParamTestInstance : public DepthStencilBaseCase
{
public:
	DepthBoundsParamTestInstance (Context &context, ShaderMap shaders)
		: DepthStencilBaseCase (context, shaders[glu::SHADERTYPE_VERTEX], shaders[glu::SHADERTYPE_FRAGMENT])
	{
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 0.375f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.0f, 1.0f, 0.375f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 0.375f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.0f, -1.0f, 0.375f, 1.0f), tcu::RGBA::green().toVec()));

		m_data.push_back(PositionColorVertex(tcu::Vec4(0.0f, 1.0f, 0.625f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 0.625f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.0f, -1.0f, 0.625f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 0.625f, 1.0f), tcu::RGBA::green().toVec()));

		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));

		m_depthStencilState_1 = PipelineCreateInfo::DepthStencilState(
			vk::VK_TRUE, vk::VK_TRUE, vk::VK_COMPARE_OP_ALWAYS, vk::VK_FALSE);

		// enable depth bounds test
		m_depthStencilState_2 = PipelineCreateInfo::DepthStencilState(
			vk::VK_FALSE, vk::VK_FALSE, vk::VK_COMPARE_OP_NEVER, vk::VK_TRUE);

		DepthStencilBaseCase::initialize();
	}

	virtual tcu::TestStatus iterate (void)
	{
		vk::VkPhysicalDeviceFeatures features;
		m_context.getInstanceInterface().getPhysicalDeviceFeatures(m_context.getPhysicalDevice(), &features);

		if (!features.depthBounds)
			return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "depthBounds Vulkan feature is not supported");

		tcu::TestLog &log = m_context.getTestContext().getLog();
		const vk::VkQueue queue = m_context.getUniversalQueue();

		beginRenderPass();

		// set states here
		setDynamicViewportState(WIDTH, HEIGHT);
		setDynamicRasterizationState();
		setDynamicBlendState();
		setDynamicDepthStencilState(0.5f, 0.75f);

		const vk::VkDeviceSize vertexBufferOffset = 0;
		const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline_1);
		m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);
		m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 4, 0);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline_2);
		m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 8, 0);

		m_vk.cmdEndRenderPass(*m_cmdBuffer);
		m_vk.endCommandBuffer(*m_cmdBuffer);

		vk::VkSubmitInfo submitInfo =
		{
			vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
			DE_NULL,							// const void*				pNext;
			0, 									// deUint32					waitSemaphoreCount;
			DE_NULL, 							// const VkSemaphore*		pWaitSemaphores;
			1, 									// deUint32					commandBufferCount;
			&m_cmdBuffer.get(),					// const VkCommandBuffer*	pCommandBuffers;
			0, 									// deUint32					signalSemaphoreCount;
			DE_NULL								// const VkSemaphore*		pSignalSemaphores;
		};
		m_vk.queueSubmit(queue, 1, &submitInfo, DE_NULL);

		// validation
		{
			VK_CHECK(m_vk.queueWaitIdle(queue));

			tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + WIDTH), (int)(0.5 + HEIGHT));
			referenceFrame.allocLevel(0);

			const deInt32 frameWidth = referenceFrame.getWidth();
			const deInt32 frameHeight = referenceFrame.getHeight();

			tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

			for (int y = 0; y < frameHeight; y++)
			{
				const float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

				for (int x = 0; x < frameWidth; x++)
				{
					const float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

					if (xCoord >= 0.0f && xCoord <= 1.0f && yCoord >= -1.0f && yCoord <= 1.0f)
						referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
					else
						referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
				}
			}

			const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
			const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
				vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

			if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
				referenceFrame.getLevel(0), renderedFrame, 0.05f,
				tcu::COMPARE_LOG_RESULT))
			{
				return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
			}

			return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
		}
	}
};

class StencilParamsBasicTestInstance : public DepthStencilBaseCase
{
protected:
	deUint32 m_writeMask;
	deUint32 m_readMask;
	deUint32 m_expectedValue;
	tcu::Vec4 m_expectedColor;

public:
	StencilParamsBasicTestInstance (Context& context, const char* vertexShaderName, const char* fragmentShaderName,
									const deUint32 writeMask, const deUint32 readMask, 
									const deUint32 expectedValue, const tcu::Vec4 expectedColor)
		: DepthStencilBaseCase  (context, vertexShaderName, fragmentShaderName)
		, m_expectedColor		(1.0f, 1.0f, 1.0f, 1.0f)
	{
		m_writeMask = writeMask;
		m_readMask = readMask;
		m_expectedValue = expectedValue;
		m_expectedColor = expectedColor;

		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));

		const PipelineCreateInfo::DepthStencilState::StencilOpState frontState_1 =
			PipelineCreateInfo::DepthStencilState::StencilOpState(
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_COMPARE_OP_ALWAYS);

		const PipelineCreateInfo::DepthStencilState::StencilOpState backState_1 =
			PipelineCreateInfo::DepthStencilState::StencilOpState(
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_COMPARE_OP_ALWAYS);

		const PipelineCreateInfo::DepthStencilState::StencilOpState frontState_2 =
			PipelineCreateInfo::DepthStencilState::StencilOpState(
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_COMPARE_OP_EQUAL);

		const PipelineCreateInfo::DepthStencilState::StencilOpState backState_2 =
			PipelineCreateInfo::DepthStencilState::StencilOpState(
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_COMPARE_OP_EQUAL);

		// enable stencil test
		m_depthStencilState_1 = PipelineCreateInfo::DepthStencilState(
			vk::VK_FALSE, vk::VK_FALSE, vk::VK_COMPARE_OP_NEVER, vk::VK_FALSE, vk::VK_TRUE, frontState_1, backState_1);

		m_depthStencilState_2 = PipelineCreateInfo::DepthStencilState(
			vk::VK_FALSE, vk::VK_FALSE, vk::VK_COMPARE_OP_NEVER, vk::VK_FALSE, vk::VK_TRUE, frontState_2, backState_2);

		DepthStencilBaseCase::initialize();
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog &log = m_context.getTestContext().getLog();
		const vk::VkQueue queue = m_context.getUniversalQueue();

		beginRenderPass();

		// set states here
		setDynamicViewportState(WIDTH, HEIGHT);
		setDynamicRasterizationState();
		setDynamicBlendState();

		const vk::VkDeviceSize vertexBufferOffset = 0;
		const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline_1);
		setDynamicDepthStencilState(-1.0f, 1.0f, 0xFF, m_writeMask, 0x0F, 0xFF, m_writeMask, 0x0F);
		m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline_2);
		setDynamicDepthStencilState(-1.0f, 1.0f, m_readMask, 0xFF, m_expectedValue, m_readMask, 0xFF, m_expectedValue);
		m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 4, 0);

		m_vk.cmdEndRenderPass(*m_cmdBuffer);
		m_vk.endCommandBuffer(*m_cmdBuffer);

		vk::VkSubmitInfo submitInfo =
		{
			vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
			DE_NULL,							// const void*				pNext;
			0, 									// deUint32					waitSemaphoreCount;
			DE_NULL, 							// const VkSemaphore*		pWaitSemaphores;
			1, 									// deUint32					commandBufferCount;
			&m_cmdBuffer.get(),					// const VkCommandBuffer*	pCommandBuffers;
			0, 									// deUint32					signalSemaphoreCount;
			DE_NULL								// const VkSemaphore*		pSignalSemaphores;
		};
		m_vk.queueSubmit(queue, 1, &submitInfo, DE_NULL);

		// validation
		{
			VK_CHECK(m_vk.queueWaitIdle(queue));

			tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + WIDTH), (int)(0.5 + HEIGHT));
			referenceFrame.allocLevel(0);

			const deInt32 frameWidth = referenceFrame.getWidth();
			const deInt32 frameHeight = referenceFrame.getHeight();

			for (int y = 0; y < frameHeight; y++)
			{
				const float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

				for (int x = 0; x < frameWidth; x++)
				{
					const float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

					if (xCoord >= -1.0f && xCoord <= 1.0f && yCoord >= -1.0f && yCoord <= 1.0f)
						referenceFrame.getLevel(0).setPixel(m_expectedColor, x, y);
				}
			}

			const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
			const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
				vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

			if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
				referenceFrame.getLevel(0), renderedFrame, 0.05f,
				tcu::COMPARE_LOG_RESULT))
			{
				return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
			}

			return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
		}
	}
};

class StencilParamsBasicTestCase : public TestCase
{
protected:
	TestInstance* createInstance(Context& context) const
	{
		return new StencilParamsBasicTestInstance(context, "VertexFetch.vert", "VertexFetch.frag",
			m_writeMask, m_readMask, m_expectedValue, m_expectedColor);
	}

	virtual void initPrograms(vk::SourceCollections& programCollection) const
	{
		programCollection.glslSources.add("VertexFetch.vert") <<
			glu::VertexSource(ShaderSourceProvider::getSource(m_testCtx.getArchive(), "vulkan/dynamic_state/VertexFetch.vert"));

		programCollection.glslSources.add("VertexFetch.frag") <<
			glu::FragmentSource(ShaderSourceProvider::getSource(m_testCtx.getArchive(), "vulkan/dynamic_state/VertexFetch.frag"));
	}

	deUint32 m_writeMask;
	deUint32 m_readMask;
	deUint32 m_expectedValue;
	tcu::Vec4 m_expectedColor;

public:
	StencilParamsBasicTestCase (tcu::TestContext& context, const char *name, const char *description,
								const deUint32 writeMask, const deUint32 readMask, 
								const deUint32 expectedValue, const tcu::Vec4 expectedColor)
		: TestCase				(context, name, description)
		, m_writeMask			(writeMask)
		, m_readMask			(readMask)
		, m_expectedValue		(expectedValue)
		, m_expectedColor		(expectedColor)
	{
	}
};

class StencilParamsAdvancedTestInstance : public DepthStencilBaseCase
{
public:
	StencilParamsAdvancedTestInstance (Context& context, ShaderMap shaders)
		: DepthStencilBaseCase (context, shaders[glu::SHADERTYPE_VERTEX], shaders[glu::SHADERTYPE_FRAGMENT])
	{
		m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, 0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, -0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, -0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));

		const PipelineCreateInfo::DepthStencilState::StencilOpState frontState_1 =
			PipelineCreateInfo::DepthStencilState::StencilOpState(
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_COMPARE_OP_ALWAYS);

		const PipelineCreateInfo::DepthStencilState::StencilOpState backState_1 =
			PipelineCreateInfo::DepthStencilState::StencilOpState(
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_COMPARE_OP_ALWAYS);

		const PipelineCreateInfo::DepthStencilState::StencilOpState frontState_2 =
			PipelineCreateInfo::DepthStencilState::StencilOpState(
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_COMPARE_OP_NOT_EQUAL);

		const PipelineCreateInfo::DepthStencilState::StencilOpState backState_2 =
			PipelineCreateInfo::DepthStencilState::StencilOpState(
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_STENCIL_OP_REPLACE,
			vk::VK_COMPARE_OP_NOT_EQUAL);

		// enable stencil test
		m_depthStencilState_1 = PipelineCreateInfo::DepthStencilState(
			vk::VK_FALSE, vk::VK_FALSE, vk::VK_COMPARE_OP_NEVER, vk::VK_FALSE, vk::VK_TRUE, frontState_1, backState_1);

		m_depthStencilState_2 = PipelineCreateInfo::DepthStencilState(
			vk::VK_FALSE, vk::VK_FALSE, vk::VK_COMPARE_OP_NEVER, vk::VK_FALSE, vk::VK_TRUE, frontState_2, backState_2);

		DepthStencilBaseCase::initialize();
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog &log = m_context.getTestContext().getLog();
		const vk::VkQueue queue = m_context.getUniversalQueue();

		beginRenderPass();

		// set states here
		setDynamicViewportState(WIDTH, HEIGHT);
		setDynamicRasterizationState();
		setDynamicBlendState();

		const vk::VkDeviceSize vertexBufferOffset = 0;
		const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline_1);
		setDynamicDepthStencilState(-1.0f, 1.0f, 0xFF, 0x0E, 0x0F, 0xFF, 0x0E, 0x0F);
		m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline_2);
		setDynamicDepthStencilState(-1.0f, 1.0f, 0xFF, 0xFF, 0x0E, 0xFF, 0xFF, 0x0E);
		m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 4, 0);

		m_vk.cmdEndRenderPass(*m_cmdBuffer);
		m_vk.endCommandBuffer(*m_cmdBuffer);

		vk::VkSubmitInfo submitInfo =
		{
			vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
			DE_NULL,							// const void*				pNext;
			0, 									// deUint32					waitSemaphoreCount;
			DE_NULL, 							// const VkSemaphore*		pWaitSemaphores;
			1, 									// deUint32					commandBufferCount;
			&m_cmdBuffer.get(),					// const VkCommandBuffer*	pCommandBuffers;
			0, 									// deUint32					signalSemaphoreCount;
			DE_NULL								// const VkSemaphore*		pSignalSemaphores;
		};
		m_vk.queueSubmit(queue, 1, &submitInfo, DE_NULL);

		// validation
		{
			VK_CHECK(m_vk.queueWaitIdle(queue));

			tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + WIDTH), (int)(0.5 + HEIGHT));
			referenceFrame.allocLevel(0);

			const deInt32 frameWidth = referenceFrame.getWidth();
			const deInt32 frameHeight = referenceFrame.getHeight();

			for (int y = 0; y < frameHeight; y++)
			{
				const float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

				for (int x = 0; x < frameWidth; x++)
				{
					const float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

					if (xCoord >= -0.5f && xCoord <= 0.5f && yCoord >= -0.5f && yCoord <= 0.5f)
						referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
					else
						referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
				}
			}

			const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
			const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
				vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

			if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
				referenceFrame.getLevel(0), renderedFrame, 0.05f,
				tcu::COMPARE_LOG_RESULT))
			{
				return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
			}

			return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
		}
	}
};

} //anonymous

DynamicStateDSTests::DynamicStateDSTests (tcu::TestContext& testCtx)
	: TestCaseGroup (testCtx, "ds_state", "Tests for depth stencil state")
{
	/* Left blank on purpose */
}

DynamicStateDSTests::~DynamicStateDSTests ()
{
}

void DynamicStateDSTests::init (void)
{
	ShaderMap shaderPaths;
	shaderPaths[glu::SHADERTYPE_VERTEX] = "vulkan/dynamic_state/VertexFetch.vert";
	shaderPaths[glu::SHADERTYPE_FRAGMENT] = "vulkan/dynamic_state/VertexFetch.frag";

	addChild(new InstanceFactory<DepthBoundsParamTestInstance>(m_testCtx, "depth_bounds", "Perform depth bounds test", shaderPaths));
	addChild(new StencilParamsBasicTestCase(m_testCtx, "stencil_params_basic_1", "Perform basic stencil test 1", 0x0D, 0x06, 0x05, tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f)));
	addChild(new StencilParamsBasicTestCase(m_testCtx, "stencil_params_basic_2", "Perform basic stencil test 2", 0x06, 0x02, 0x05, tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f)));
	addChild(new InstanceFactory<StencilParamsAdvancedTestInstance>(m_testCtx, "stencil_params_advanced", "Perform advanced stencil test", shaderPaths));
}

} // DynamicState
} // vkt
