#include "synthesis/TestPresenter.h"

#include "vkt/Buffer.h"
#include "vkt/Device.h"
#include "vkt/ForwardDecl.h"
#include "vkt/GraphicsPipeline.h"
#include "vkt/RenderPass.h"
#include "vkt/Shader.h"
#include "vkt/ComputePipeline.h"
#include "imgui_impl_vulkan.h"

#include <imgui.h>
#include <ImGuiFileDialog.h>

#include <glm/glm.hpp>

#include <spdlog/spdlog.h>
#include <iostream>

synthesis::TestPresenter::TestPresenter( burst::PresentContext const & inContext, burst::ImageAsset inImage )
:
    mContext( inContext ),
    mComputeDescriptorSetLayout(
        vkt::DescriptorSetLayoutBuilder( mContext.mDevice )
        .AddLayoutBinding( 0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute )
        .AddLayoutBinding( 1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute )
        .Build( vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR )
    ),
    mGraphicsDescriptorSetLayout(
        vkt::DescriptorSetLayoutBuilder( mContext.mDevice )
        .AddLayoutBinding( 0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment )
        .Build( vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR )
    ),
    mInitialImage( inImage ),
    mMaskImage( CreateImageData( vk::Extent2D( inImage.mWidth, inImage.mHeight ) ) ),
    mDrawImage( CreateImageData( vk::Extent2D( inImage.mWidth, inImage.mHeight ) ) ),
    mDisplayImage( CreateImageData( vk::Extent2D( inImage.mWidth, inImage.mHeight ) ) ),
    mDisplayInspector(
        "Display",
        vk::Extent2D( mDisplayImage.mImage->GetWidth(), mDisplayImage.mImage->GetHeight() ),
        {
            { mDisplayImage.mSampler, mDisplayImage.mImageView },
            { mDrawImage.mSampler, mDrawImage.mImageView },
            { mMaskImage.mSampler, mMaskImage.mImageView }
        },
        [this]( glm::vec2 inPos ){ PaintDrawImage( inPos ); }
    )
{
    WriteImage( inImage );

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

synthesis::TestPresenter::~TestPresenter()
{
    mContext.mDevice.GetVkDevice().waitIdle();

    DestroyImageData( mDrawImage );
    DestroyImageData( mDisplayImage );
    DestroyImageData( mMaskImage );
}

void
synthesis::TestPresenter::Compute( vk::CommandBuffer inCommandBuffer ) const
{
    if( !mCompute ) return;

    // Begin pipeline
    mComputePipeline->Bind( inCommandBuffer );

    // Push image descriptor set
    auto imageInfo = vk::DescriptorImageInfo
    (
        mDisplayImage.mSampler,
        mDisplayImage.mImageView,
        vk::ImageLayout::eGeneral
    );

    auto theWriteDescriptorSet = vk::WriteDescriptorSet();
    theWriteDescriptorSet.setDstBinding( 0 );
    theWriteDescriptorSet.setDstArrayElement( 0 );
    theWriteDescriptorSet.setDescriptorType( vk::DescriptorType::eStorageImage );
    theWriteDescriptorSet.setDescriptorCount( 1 );
    theWriteDescriptorSet.setPImageInfo( & imageInfo );

    mComputePipeline->BindPushDescriptorSet( inCommandBuffer, theWriteDescriptorSet );

    // Push image mask descriptor set
    auto maskImageInfo = vk::DescriptorImageInfo
    (
        mMaskImage.mSampler,
        mMaskImage.mImageView,
        vk::ImageLayout::eGeneral
    );

    auto writeDescriptorSet = vk::WriteDescriptorSet();
    writeDescriptorSet.setDstBinding( 1 );
    writeDescriptorSet.setDstArrayElement( 0 );
    writeDescriptorSet.setDescriptorType( vk::DescriptorType::eStorageImage );
    writeDescriptorSet.setDescriptorCount( 1 );
    writeDescriptorSet.setPImageInfo( & maskImageInfo );

    mComputePipeline->BindPushDescriptorSet( inCommandBuffer, writeDescriptorSet );

    // Push constants
    auto time = std::chrono::duration_cast<std::chrono::microseconds>
    (
        std::chrono::system_clock::now().time_since_epoch()
    );
    static bool sortEven = false;
    sortEven = !sortEven;
    PushConstants thePushConstants
    {
        ( float ) ( mStartTime - time ).count() / 1000.0f,
        std::uint32_t( mDrawImage.mImage->GetWidth() ),
        std::uint32_t( mDrawImage.mImage->GetHeight() ),
        sortEven
    };
    inCommandBuffer.pushConstants
    (
        mComputePipeline->GetVkPipelineLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof( PushConstants ),
        & thePushConstants
    );

    int groupsize = 16;
    inCommandBuffer.dispatch( ceil( mDisplayImage.mImage->GetWidth() / groupsize ), 1, 1 );
}

void
synthesis::TestPresenter::Present( vk::CommandBuffer inCommandBuffer ) const
{
    return;
    // Draw screen rect
    mPipeline->Bind( inCommandBuffer );

    // Push descriptor set
    auto imageInfo = vk::DescriptorImageInfo
    (
        mDisplayImage.mSampler,
        mDisplayImage.mImageView,
        vk::ImageLayout::eGeneral
    );
    if( mShowMask )
    {
        imageInfo = vk::DescriptorImageInfo
        (
            mMaskImage.mSampler,
            mMaskImage.mImageView,
            vk::ImageLayout::eGeneral
        );
    }

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
synthesis::TestPresenter::Update( float inDelta )
{
    ImGui::Begin("synthesis");
    {
        ImGui::Button( "Compute" );
        mCompute = ImGui::IsItemActive();
        if( ImGui::Button( "Reset" ) )
        {
            WriteImage( mInitialImage );
            ClearPaintImage();
        }

        if( ImGui::Button( "Store mask") )
        {
            WriteMaskImage();
        }

        if( ImGui::Button( "Export image") )
        {
            SaveImage();
        }

        ImGui::Checkbox( "Show mask", & mShowMask );
        ImGui::Checkbox( "Show draw", & mShowDraw );
        ImGui::Checkbox( "Blend edges", & mBlendEdges );
        ImGui::SliderInt( "Blend range", & mBlendRange, 1, 500);
        ImGui::SliderInt( "Mask chance", & mMaskChance, 1, 800);
        ImGui::SliderInt( "Pencil size", & mPencilSize, 2, 500);
    }
    ImGui::End();

    mDisplayInspector.Update( { true, mShowDraw, mShowMask } );
}

void
synthesis::TestPresenter::WriteImage( burst::ImageAsset inImage )
{
    // Buffer
    auto stagingBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
    (
        sizeof( glm::vec4 ) * inImage.mWidth * inImage.mHeight,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
        vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
        "PointBuffer"
    );

    // Store image data
    auto bufferData = ( std::uint32_t * ) stagingBuffer->MapMemory();
    {
        std::memcpy( bufferData, inImage.mPixels.data(), sizeof( std::uint8_t ) * 4  * inImage.mWidth * inImage.mHeight );
    }
    stagingBuffer->UnMapMemory();


    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        // Copy the staging buffer into the image
        commandBuffer.copyBufferToImage
        (
            stagingBuffer->GetVkBuffer(),
            mDisplayImage.mImage->GetVkImage(),
            vk::ImageLayout::eGeneral,
            vk::BufferImageCopy
                (
                    0,
                    0,
                    0,
                    vk::ImageSubresourceLayers
                        (
                            vk::ImageAspectFlagBits::eColor,
                            0,
                            0,
                            1
                        ),
                    vk::Offset3D( 0, 0, 0 ),
                    vk::Extent3D( inImage.mWidth, inImage.mHeight, 1 )
                )
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );
}

void
synthesis::TestPresenter::WriteMaskImage()
{
    glm::vec2 imageSize = glm::vec2( mMaskImage.mImage->GetWidth(), mMaskImage.mImage->GetHeight() );
    auto drawBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
        (
            sizeof( glm::vec4 ) * imageSize.x * imageSize.y,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vma::AllocationCreateFlagBits::eHostAccessRandom,
            "MaskStagingBuffer"
        );

    // Buffer
    auto stagingBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
        (
            sizeof( glm::vec4 ) * imageSize.x * imageSize.y,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
            vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
            "MaskStagingBuffer"
        );

    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {

        commandBuffer.copyImageToBuffer
            (
                mDrawImage.mImage->GetVkImage(),
                vk::ImageLayout::eGeneral,
                drawBuffer->GetVkBuffer(),
                vk::BufferImageCopy
                    (
                        0,
                        0,
                        0,
                        vk::ImageSubresourceLayers
                            (
                                vk::ImageAspectFlagBits::eColor,
                                0,
                                0,
                                1
                            ),
                        vk::Offset3D( 0, 0, 0 ),
                        vk::Extent3D( imageSize.x, imageSize.y, 1 )
                    )
            );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );
    commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        std::uint32_t * drawData = ( std::uint32_t * ) drawBuffer->MapMemory();

        // Store image data
        auto bufferData = ( std::uint8_t * ) stagingBuffer->MapMemory();
        {
            std::size_t i = 0;
            for( std::size_t y = 0; y < imageSize.x; y++ )
            {
                for( std::size_t x = 0; x < imageSize.y; x++ )
                {
                    bufferData[ i + 0 ] = 0;
                    bufferData[ i + 1 ] = 0;
                    bufferData[ i + 2 ] = 0;
                    bufferData[ i + 3 ] = 0;
                    i += 4;
                }
            }

            for( std::size_t x = 0; x < imageSize.x; x++ )
            {
                for( std::size_t y = 0; y < imageSize.y; y++ )
                {
                    if( drawData[ std::uint32_t( ( y * imageSize.x + x ) ) ] > 128 )
                    {
                        if( mBlendEdges ) y += rand() % mBlendRange;
                        std::size_t tailBlend = 0;
                        if( mBlendEdges ) tailBlend = rand() % mBlendRange;
                        int final_pix = -1;

                        std::uint32_t weight = 255;
                        for (; y < imageSize.y; y++)
                        {
                            if( final_pix < 0 && drawData[ std::uint32_t( ( y * imageSize.x + x ) ) ] < 128 )
                            {
                                final_pix = y;
                            }
                            if( final_pix > -1 && y > final_pix + tailBlend ) break;

                            if( rand() % mMaskChance < 1 ) weight = rand() % 255;
                            if (y >= imageSize.y ) break;

                            std::size_t index = ( y * imageSize.x + x ) * 4;
                            bufferData[index + 0] = weight;
                            bufferData[index + 1] = 0;
                            bufferData[index + 2] = 0;
                            bufferData[index + 3] = 255;
                        }
                    }
                }
            }
        }
        stagingBuffer->UnMapMemory();
        drawBuffer->UnMapMemory();

        // Clear the mask image
        commandBuffer.copyBufferToImage
        (
            stagingBuffer->GetVkBuffer(),
            mMaskImage.mImage->GetVkImage(),
            vk::ImageLayout::eGeneral,
            vk::BufferImageCopy
            (
                0,
                0,
                0,
                vk::ImageSubresourceLayers
                    (
                        vk::ImageAspectFlagBits::eColor,
                        0,
                        0,
                        1
                    ),
                vk::Offset3D( 0, 0, 0 ),
                vk::Extent3D( mMaskImage.mImage->GetWidth(), mMaskImage.mImage->GetHeight(), 1 )
            )
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );

}

void
synthesis::TestPresenter::PaintDrawImage( const glm::vec2 inPos )
{
    glm::vec2 pencilSize = glm::vec2( mPencilSize, mPencilSize );

    if( inPos.x < 0 || inPos.y < 0 || inPos.x + pencilSize.x > mDrawImage.mImage->GetWidth() || inPos.y + pencilSize.y > mDrawImage.mImage->GetHeight() )
    {
        return;
    }

    // Update the mask image at the position
    // Buffer
    auto stagingBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
    (
        sizeof( glm::vec4 ) * pencilSize.x * pencilSize.y,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
        vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
        "MaskStagingBuffer"
    );

    // Store pencil data
    auto bufferData = ( std::uint8_t * ) stagingBuffer->MapMemory();
    {
        std::uint8_t value = 255;
        if( ImGui::IsKeyDown(ImGuiKey::ImGuiKey_LeftShift)) value = 0;

        std::size_t i = 0;
        for( std::size_t y = 0; y < pencilSize.y; y++ )
        {
            for( std::size_t x = 0; x < pencilSize.x; x++ )
            {
                bufferData[ i + 0 ] = value;
                bufferData[ i + 1 ] = 0;
                bufferData[ i + 2 ] = 0;
                bufferData[ i + 3 ] = value;
                i += 4;
            }
        }
    }
    stagingBuffer->UnMapMemory();

    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        // Clear the mask image
        commandBuffer.copyBufferToImage
        (
            stagingBuffer->GetVkBuffer(),
            mDrawImage.mImage->GetVkImage(),
            vk::ImageLayout::eGeneral,
            vk::BufferImageCopy
            (
                0,
                0,
                0,
                vk::ImageSubresourceLayers
                (
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    0,
                    1
                ),
                vk::Offset3D( inPos.x, inPos.y, 0 ),
                vk::Extent3D( pencilSize.x, pencilSize.y, 1 )
            )
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );
}

synthesis::TestPresenter::ImageData
synthesis::TestPresenter::CreateImageData( vk::Extent2D inExtent )
{
    ImageData data;

    // Image
    data.mImage = vkt::ImageFactory( mContext.mDevice ).CreateImage
    (
        inExtent.width,
        inExtent.height,
        vk::Format::eR8G8B8A8Unorm,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
        vma::AllocationCreateFlagBits::eDedicatedMemory,
        "Test Image",
        1
    );

    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        // Image layout
        data.mImage->MemoryBarrier
        (
            commandBuffer,
            vk::ImageLayout::eGeneral,
            vk::AccessFlagBits::eNone,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::DependencyFlagBits::eByRegion
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );

    // Sampler
    data.mSampler = mContext.mDevice.GetVkDevice().createSampler
    (
        vk::SamplerCreateInfo
        (
            vk::SamplerCreateFlags(),
            vk::Filter::eNearest,
            vk::Filter::eNearest,
            vk::SamplerMipmapMode::eLinear,
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
    data.mImageView = mContext.mDevice.GetVkDevice().createImageView
    (
        vk::ImageViewCreateInfo
        (
            vk::ImageViewCreateFlags(),
            data.mImage->GetVkImage(),
            vk::ImageViewType::e2D,
            vk::Format::eR8G8B8A8Unorm,
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

    return data;
}

void
synthesis::TestPresenter::DestroyImageData( const synthesis::TestPresenter::ImageData & inImageData )
{
    mContext.mDevice.GetVkDevice().destroy( inImageData.mImageView );
    mContext.mDevice.GetVkDevice().destroy( inImageData.mSampler );
}

void
synthesis::TestPresenter::ClearPaintImage()
{
    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        vk::ClearColorValue cv = vk::ClearColorValue( 0.0f, 0.0f, 0.0f, 0.0f );

        auto subResourceRange = vk::ImageSubresourceRange
        (
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1
        );

//        commandBuffer.clearColorImage
//        (
//            mDrawImage.mImage->GetVkImage(),
//            mDrawImage.mImage->GetVkImageLayout(),
//            & cv,
//            1,
//            & subResourceRange
//        );

        commandBuffer.clearColorImage
        (
            mMaskImage.mImage->GetVkImage(),
            mMaskImage.mImage->GetVkImageLayout(),
            & cv,
            1,
            & subResourceRange
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );
}

void
synthesis::TestPresenter::SaveImage()
{
    // Create a buffer to store/read the image
    std::size_t byteSize = mDisplayImage.mImage->GetWidth() * mDisplayImage.mImage->GetHeight() * 4;
    auto imageBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
    (
        byteSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vma::AllocationCreateFlagBits::eHostAccessRandom,
        "MaskStagingBuffer"
    );

    // Copy the image
    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        commandBuffer.copyImageToBuffer
        (
            mDisplayImage.mImage->GetVkImage(),
            vk::ImageLayout::eGeneral,
            imageBuffer->GetVkBuffer(),
            vk::BufferImageCopy
            (
                0,
                mDisplayImage.mImage->GetWidth(),
                mDisplayImage.mImage->GetHeight(),
                vk::ImageSubresourceLayers
                (
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    0,
                    1
                ),
                vk::Offset3D( 0, 0, 0 ),
                vk::Extent3D( mDisplayImage.mImage->GetWidth(), mDisplayImage.mImage->GetHeight(), 1 )
            )
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );

    burst::ImageAsset imageAsset = burst::ImageAsset();
    imageAsset.mWidth = mDisplayImage.mImage->GetWidth();
    imageAsset.mHeight = mDisplayImage.mImage->GetHeight();

    imageAsset.mPixels.resize( byteSize );

    std::uint8_t * bufferData = ( std::uint8_t * ) imageBuffer->MapMemory();
    std::memcpy( imageAsset.mPixels.data(), bufferData, byteSize );
    imageBuffer->UnMapMemory();

    burst::AssetLoader::SaveImage( imageAsset, "output.png" );
}



