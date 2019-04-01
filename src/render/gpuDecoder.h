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
        GpuDecoder(char* path);
        
    private:
        AVFormatContext *formatContext;    
        AVCodec *codec;
        AVCodecContext *codecContext;
        AVBufferRef *deviceContext;
        VdpGetProcAddress *get_proc_address;
        VdpOutputSurfaceCreate *vdp_output_surface_create;
        VdpVideoMixerCreate *vdp_video_mixer_create;
        VdpVideoMixerRender * vdp_video_mixer_render;
        VdpVideoMixer mixer;
        AVVDPAUDeviceContext *vdpauContext;
        int videoStreamId;
        AVPixelFormat pixFmt;
        VdpRect flipRect;
};
