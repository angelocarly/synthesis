#include "pixelsort/TestPresenter.h"

#include "vkt/Buffer.h"
#include "vkt/Device.h"
#include "vkt/ForwardDecl.h"
#include "vkt/GraphicsPipeline.h"
#include "vkt/RenderPass.h"
#include "vkt/Shader.h"
#include "vkt/ComputePipeline.h"

#include <imgui.h>
#include <ImGuiFileDialog.h>

#include <glm/glm.hpp>

pixelsort::TestPresenter::TestPresenter( burst::PresentContext const & inContext )
:
    mContext( inContext ),
    mComputeDescriptorSetLayout(
        vkt::DescriptorSetLayoutBuilder( mContext.mDevice )
        .AddLayoutBinding( 0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute )
        .AddLayoutBinding( 1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute )
        .Build( vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR )
    ),
    mGraphicsDescriptorSetLayout(
        vkt::DescriptorSetLayoutBuilder( mContext.mDevice )
        .AddLayoutBinding( 0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment )
        .Build( vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR )
    )
{
    // Buffer
    std::size_t pointCount = 1024 * 1024;
    mPointBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
    (
        sizeof( glm::vec4 ) * pointCount,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
        "PointBuffer"
    );

    auto bufferData = ( glm::vec4 * ) mPointBuffer->MapMemory();
    {
        float radius = 1000;
        for( std::size_t i = 0; i < pointCount; i++ )
        {
            auto pos = glm::vec4( radius * 10 );
            while( sqrt( pos.x * pos.x + pos.y * pos.y + pos.z * pos.z ) > radius )
            {
                pos = glm::vec4
                (
                    ( rand() % int( radius * 2 ) ) - radius,
                    ( rand() % int( radius * 2 ) ) - radius,
                    ( rand() % int( radius * 2 ) ) - radius,
                    0
                );
            }
            bufferData[ i ] = pos;
        }
    }
    mPointBuffer->UnMapMemory();

    // Image
    mImage = vkt::ImageFactory( mContext.mDevice ).CreateImage
    (
        mContext.mWidth,
        mContext.mHeight,
        vk::Format::eR32Uint,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vma::AllocationCreateFlagBits::eDedicatedMemory,
        "Test Image",
        1
    );

    // Image layout
    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    mImage->MemoryBarrier
    (
        commandBuffer,
        vk::ImageLayout::eGeneral,
        vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eShaderRead,
        vk::PipelineStageFlagBits::eAllCommands,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::DependencyFlagBits::eByRegion
    );
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );

    // Sampler
    mSampler = mContext.mDevice.GetVkDevice().createSampler
    (
        vk::SamplerCreateInfo
        (
            vk::SamplerCreateFlags(),
            vk::Filter::eNearest,
            vk::Filter::eNearest,
            vk::SamplerMipmapMode::eNearest,
            vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode::eRepeat,
            0.0f,
            VK_FALSE,
            16.0f,
            VK_FALSE,
            vk::CompareOp::eAlways,
            0.0f,
            0.0f,
            vk::BorderColor::eIntOpaqueBlack,
            VK_FALSE
        )
    );

    // Image view
    mImageView = mContext.mDevice.GetVkDevice().createImageView
    (
        vk::ImageViewCreateInfo
        (
            vk::ImageViewCreateFlags(),
            mImage->GetVkImage(),
            vk::ImageViewType::e2D,
            vk::Format::eR32Uint,
            vk::ComponentMapping(),
            vk::ImageSubresourceRange
            (
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1
            )
        )
    );

    // Compute shader
    auto computeShader = vkt::Shader::CreateVkShaderModule( mContext.mDevice, "resources/shaders/Compute.comp" );

    std::vector< vk::PushConstantRange > pushConstants =
    {
        vk::PushConstantRange
        (
            vk::ShaderStageFlagBits::eCompute,
            0,
            sizeof( PushConstants )
        )
    };

    mComputePipeline = vkt::ComputePipelineBuilder( mContext.mDevice )
        .SetComputeShader( computeShader )
        .SetDescriptorSetLayouts( mComputeDescriptorSetLayout )
        .SetPushConstants( pushConstants )
        .Build();

    mContext.mDevice.GetVkDevice().destroy( computeShader );

    // Present shader
    auto vertexShader = vkt::Shader::CreateVkShaderModule( mContext.mDevice, "resources/shaders/ScreenRect.vert" );
    auto fragmentShader = vkt::Shader::CreateVkShaderModule( mContext.mDevice, "resources/shaders/Sampler.frag" );

    mPipeline = vkt::GraphicsPipelineBuilder( mContext.mDevice )
        .SetDescriptorSetLayouts( mGraphicsDescriptorSetLayout )
        .SetVertexShader( vertexShader )
        .SetFragmentShader( fragmentShader )
        .SetRenderPass( mContext.mRenderPass->GetVkRenderPass() )
        .Build();

    mContext.mDevice.GetVkDevice().destroy( vertexShader );
    mContext.mDevice.GetVkDevice().destroy( fragmentShader );

}

pixelsort::TestPresenter::~TestPresenter()
{
    mContext.mDevice.GetVkDevice().waitIdle();

    // TODO: Destroy image handles automatically
    mContext.mDevice.GetVkDevice().destroy( mImageView );
    mContext.mDevice.GetVkDevice().destroy( mSampler );
}

void
pixelsort::TestPresenter::Compute( vk::CommandBuffer inCommandBuffer ) const
{
    // Reset image
    inCommandBuffer.clearColorImage
    (
        mImage->GetVkImage(),
        vk::ImageLayout::eGeneral,
        vk::ClearColorValue( std::array< float, 4 >{ 0.0f, 0.0f, 0.0f, 0.0f } ),
        vk::ImageSubresourceRange
        (
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1
        )
    );

    // Begin pipeline
    mComputePipeline->Bind( inCommandBuffer );

    // Push image descriptor set
    auto imageInfo = vk::DescriptorImageInfo
    (
        mSampler,
        mImageView,
        vk::ImageLayout::eGeneral
    );

    auto theWriteDescriptorSet = vk::WriteDescriptorSet();
    theWriteDescriptorSet.setDstBinding( 0 );
    theWriteDescriptorSet.setDstArrayElement( 0 );
    theWriteDescriptorSet.setDescriptorType( vk::DescriptorType::eStorageImage );
    theWriteDescriptorSet.setDescriptorCount( 1 );
    theWriteDescriptorSet.setPImageInfo( & imageInfo );

    mComputePipeline->BindPushDescriptorSet( inCommandBuffer, theWriteDescriptorSet );

    // Push buffer descriptor set
    auto bufferInfo = vk::DescriptorBufferInfo
    (
        mPointBuffer->GetVkBuffer(),
        0,
        mPointBuffer->GetSize()
    );
    auto writeDescriptorSet = vk::WriteDescriptorSet();
    writeDescriptorSet.setDstBinding( 1 );
    writeDescriptorSet.setDstArrayElement( 0 );
    writeDescriptorSet.setDescriptorType( vk::DescriptorType::eStorageBuffer );
    writeDescriptorSet.setDescriptorCount( 1 );
    writeDescriptorSet.setPBufferInfo( & bufferInfo );

    mComputePipeline->BindPushDescriptorSet( inCommandBuffer, writeDescriptorSet );

    // Push constants
    auto time = std::chrono::duration_cast<std::chrono::microseconds>
    (
        std::chrono::system_clock::now().time_since_epoch()
    );
    PushConstants thePushConstants
    {
        ( float ) ( mStartTime - time ).count() / 1000.0f
    };
    inCommandBuffer.pushConstants
    (
        mComputePipeline->GetVkPipelineLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof( PushConstants ),
        & thePushConstants
    );

    int pointCount = 256;
    int groupsize = 16;
    inCommandBuffer.dispatch( ceil( pointCount / groupsize ), ceil( pointCount / groupsize), 1 );
}

void
pixelsort::TestPresenter::Present( vk::CommandBuffer inCommandBuffer ) const
{
    // Draw screen rect
    mPipeline->Bind( inCommandBuffer );

    // Push descriptor set
    auto imageInfo = vk::DescriptorImageInfo
    (
        mSampler,
        mImageView,
        vk::ImageLayout::eGeneral
    );

    auto theWriteDescriptorSet = vk::WriteDescriptorSet();
    theWriteDescriptorSet.setDstBinding( 0 );
    theWriteDescriptorSet.setDstArrayElement( 0 );
    theWriteDescriptorSet.setDescriptorType( vk::DescriptorType::eCombinedImageSampler );
    theWriteDescriptorSet.setDescriptorCount( 1 );
    theWriteDescriptorSet.setPImageInfo( & imageInfo );

    mPipeline->BindPushDescriptorSet( inCommandBuffer, theWriteDescriptorSet );

    inCommandBuffer.draw( 3, 1, 0, 0 );
}

void
pixelsort::TestPresenter::Update( float inDelta )
{
}


