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
        std::vector<uint64_t> getFrames(size_t number);
        GpuDecoder(const char* path);
        //active means the one that is currently being loaded or is now prepared for loading
        int getActiveBufferIndex() {return bufferIndex;}
        
    private:
        static constexpr int BUFFER_COUNT{2};
        struct TextureBuffer
        {
            std::vector<uint64_t> textureHandles;
            std::vector<uint32_t> textures;
            std::vector<uint32_t> vdpSurfaces;
            std::vector<long int> nvSurfaces; 
        } buffers[BUFFER_COUNT];

        TextureBuffer* getCurrentBuffer() {return &buffers[bufferIndex];};
        void swapBuffers() {bufferIndex ^= 1;}; //(bufferIndex+1)%BUFFER_COUNT;};
        void recreateBuffer(size_t number);

        int bufferIndex{0};
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