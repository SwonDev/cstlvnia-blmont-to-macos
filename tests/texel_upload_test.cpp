#include "nvn/nvn_vk.h"
#include "nvn/nvn_pipeline.h"
#include <array>
#include <vector>
#include <cstring>
#undef NDEBUG
#include <cassert>
namespace {
nx::nvn::ObjectsLock lock;
std::vector<char> events;
std::array<unsigned char,4096> staging;
VkBufferImageCopy last_copy{};
void VKAPI_CALL on_barrier(VkCommandBuffer,VkPipelineStageFlags src,VkPipelineStageFlags dst,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t count,const VkImageMemoryBarrier* b) {
    assert(src==VK_PIPELINE_STAGE_TRANSFER_BIT && dst==VK_PIPELINE_STAGE_TRANSFER_BIT);
    assert(count==1 && b->oldLayout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && b->newLayout==b->oldLayout);
    assert(b->srcAccessMask==VK_ACCESS_TRANSFER_WRITE_BIT && b->dstAccessMask==VK_ACCESS_TRANSFER_WRITE_BIT);
    assert(b->srcQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED && b->dstQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED);
    assert(b->subresourceRange.baseMipLevel==0 && b->subresourceRange.baseArrayLayer==0);
    assert(b->subresourceRange.levelCount==VK_REMAINING_MIP_LEVELS && b->subresourceRange.layerCount==VK_REMAINING_ARRAY_LAYERS);
    events.push_back('B');
}
void VKAPI_CALL on_copy(VkCommandBuffer,VkBuffer,VkImage,VkImageLayout layout,uint32_t count,const VkBufferImageCopy* r) {
    assert(layout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && count==1);
    assert(!events.empty() && events.back()=='B');
    last_copy=*r;events.push_back('C');
}
}
namespace nx::vk {
PFN_vkCmdPipelineBarrier p_vkCmdPipelineBarrier=on_barrier;
PFN_vkCmdCopyBufferToImage p_vkCmdCopyBufferToImage=on_copy;
}
namespace nx::nvn {
Texture::~Texture()=default;
ObjectsLock& objects_lock(){return lock;}
Upload upload(const void* p,uint64_t size,uint64_t){assert(size<=staging.size());std::memcpy(staging.data(),p,size);return {VK_NULL_HANDLE,0,staging.data()};}
void transition(VkCommandBuffer,GpuImage& img,VkImageLayout to){if(img.layout!=to){events.push_back('T');img.layout=to;}}
}
int main(){
    using namespace nx::nvn;
    Texture t;GpuImage img;img.width=img.height=img.depth=4;img.layers=16;img.layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    Texture::TexelWrite a;a.level=0;a.w=a.h=2;a.d=1;a.z=7;a.data.assign(16,11);
    t.writes={a,a};t.has_writes=true;
    // La copia base ya está grabada en TRANSFER_DST; dos parches pueden solaparse después.
    apply_texel_writes(VK_NULL_HANDLE,t,img);
    assert((events==std::vector<char>{'B','C','B','C'}));
    assert(last_copy.imageSubresource.baseArrayLayer==7 && last_copy.imageSubresource.layerCount==1);
    assert(t.writes.empty() && !t.has_writes && img.texels);
    events.clear();apply_texel_writes(VK_NULL_HANDLE,t,img);assert(events.empty());
    img.layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;img.view_type=VK_IMAGE_VIEW_TYPE_3D;
    a.z=2;a.d=2;a.data.assign(32,22);t.writes={a};t.has_writes=true;
    apply_texel_writes(VK_NULL_HANDLE,t,img);
    assert((events==std::vector<char>{'T','B','C','T'}));
    assert(last_copy.imageSubresource.baseArrayLayer==0 && last_copy.imageSubresource.layerCount==1);
    assert(last_copy.imageOffset.z==2 && last_copy.imageExtent.depth==2 && staging[31]==22);
    assert(img.layout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    events.clear();a.level=4;img.levels=1;t.writes={a};t.has_writes=true;
    apply_texel_writes(VK_NULL_HANDLE,t,img);assert((events==std::vector<char>{'T','T'}));
    for(unsigned invalid=0;invalid<4;++invalid) {
        events.clear();a.level=0;a.x=invalid==0?4:0;a.y=invalid==1?4:0;
        a.z=invalid==2?4:0;a.w=invalid==3?5:2;a.h=2;a.d=1;
        t.writes={a};t.has_writes=true;apply_texel_writes(VK_NULL_HANDLE,t,img);
        assert((events==std::vector<char>{'T','T'}));
    }
}
