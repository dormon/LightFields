#include <vector>
#include <cstdint>
#include <stdexcept>
#include <geGL/geGL.h>
#include <geGL/StaticCalls.h>
extern "C" { 
#include <libavdevice/avdevice.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext_vdpau.h>
#include <libavcodec/vdpau.h>
}


class GpuDecoder
{
    public:
        std::vector<uint64_t> getFrames(uint32_t number);
        GpuDecoder(const char* path);
        
    private:
        std::vector<uint64_t> textureHandles;
        std::vector<uint32_t> textures;
        std::vector<uint32_t> vdpSurfaces;
        AVFormatContext *formatContext;    
        AVCodec *codec;
        AVCodecContext *codecContext;
        AVBufferRef *deviceContext;
        VdpGetProcAddress *get_proc_address;
        VdpOutputSurfaceCreate *vdp_output_surface_create;
        VdpOutputSurfaceDestroy *vdp_output_surface_destroy;
        VdpVideoMixerCreate *vdp_video_mixer_create;
        VdpVideoMixerRender * vdp_video_mixer_render;
        VdpVideoMixer mixer;
        AVVDPAUDeviceContext *vdpauContext;
        int videoStreamId;
        AVPixelFormat pixFmt;
        VdpRect flipRect;
        AVPacket packet;
};
