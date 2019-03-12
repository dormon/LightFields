#include<Simple3DApp/Application.h>
#include<Vars/Vars.h>
#include<geGL/StaticCalls.h>
#include<geGL/geGL.h>
#include<BasicCamera/FreeLookCamera.h>
#include<BasicCamera/PerspectiveCamera.h>
#include <BasicCamera/OrbitCamera.h>
#include<Barrier.h>
#include<imguiSDL2OpenGL/imgui.h>
#include<imguiVars.h>
#include<DrawGrid.h>
#include<FreeImagePlus.h>
#include <experimental/filesystem>
#include <regex>
extern "C" { 
#include <libavdevice/avdevice.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext_vdpau.h>
#include <libavcodec/vdpau.h>
}
#include <CL/cl.h>
#include<assimp/Importer.hpp>
#include <assimp/scene.h>

#include<fstream>
#include<sstream>
#include<string>
#include<thread>


namespace fs = std::experimental::filesystem;

std::vector<std::string>getDirectoryImages(std::string const&dir)
{
    if(!fs::is_directory(dir))
        throw std::invalid_argument(std::string("path: ")+dir+" is not directory");
    std::vector<std::string>result;
    for(auto& p: fs::directory_iterator(dir))
        result.push_back(p.path().c_str());
    return result;
}

std::vector<std::string>sortImages(std::vector<std::string>const&imgs)
{
    std::vector<std::string>res = imgs;
    std::regex number(".*[^0-9]([0-9]+)\\..*");
    for(size_t i=0; i<res.size(); ++i)
        for(size_t i=0; i<res.size()-1; ++i)
        {
            auto a = res.at(i);
            auto b = res.at(i+1);
            auto an = std::atoi(std::regex_replace(a,number,"$1").c_str());
            auto bn = std::atoi(std::regex_replace(b,number,"$1").c_str());
            if(an>bn)
            {
                res[i] = b;
                res[i+1] = a;
            }
        }
    return res;
}

std::vector<std::string>getLightFieldImageNames(std::string const&d)
{
    return sortImages(getDirectoryImages(d));
}



class LightFields: public simple3DApp::Application
{
public:
    LightFields(int argc, char* argv[]) : Application(argc, argv) {}
    virtual ~LightFields() {}
    virtual void draw() override;

    vars::Vars vars;

    virtual void                init() override;
    //void                        parseArguments();
    virtual void                mouseMove(SDL_Event const& event) override;
    virtual void                key(SDL_Event const& e, bool down) override;
    virtual void                resize(uint32_t x,uint32_t y) override;
};

void createProgram(vars::Vars&vars)
{
    std::ifstream ifs("../src/render/shader/lf.vert");
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER, "#version 450\n", buffer.str());

    ifs.close();
    buffer.str("");

    ifs.open("../src/render/shader/lf.frag");
    buffer << ifs.rdbuf();
    auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER, "#version 450\n", buffer.str());

    vars.reCreate<ge::gl::Program>("lfProgram",vs,fs);

    ifs.close();
    buffer.str("");

    ifs.open("../src/render/shader/geom.vert");
    buffer << ifs.rdbuf();
    vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER, "#version 450\n", buffer.str());

    ifs.close();
    buffer.str("");

    ifs.open("../src/render/shader/geom.frag");
    buffer << ifs.rdbuf();
    fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER, "#version 450\n", buffer.str());

    vars.reCreate<ge::gl::Program>("geomProgram",vs,fs);

    if(vars.get<ge::gl::Program>("lfProgram")->getLinkStatus() == GL_FALSE)
        throw std::runtime_error("Cannot link shader program.");
}

void createView(vars::Vars&vars)
{
    if(notChanged(vars,"all",__FUNCTION__, {}))return;

    //vars.reCreate<basicCamera::FreeLookCamera>("view");

    vars.add<basicCamera::OrbitCamera>("view");
}

void createProjection(vars::Vars&vars)
{
    if(notChanged(vars,"all",__FUNCTION__, {"windowSize","camera.fovy","camera.near","camera.far"}))return;

    auto windowSize = vars.get<glm::uvec2>("windowSize");
    auto width = windowSize->x;
    auto height = windowSize->y;
    auto aspect = (float)width/(float)height;
    auto near = vars.getFloat("camera.near");
    auto far  = vars.getFloat("camera.far" );
    auto fovy = vars.getFloat("camera.fovy");

    vars.reCreate<basicCamera::PerspectiveCamera>("projection",fovy,aspect,near,far);
}

void createCamera(vars::Vars&vars)
{
    createProjection(vars);
    createView(vars);
}

int loadLfImage(vars::Vars&vars, const char* path, bool depth)
{
    //if(notChanged(vars,"all",__FUNCTION__, {}))return;

    auto imgs = getLightFieldImageNames(path);

    fipImage img;
    FIBITMAP* bm;
    uint32_t width = 0;
    uint32_t height = 0;
    ge::gl::Texture*tex;
    uint32_t counter = 0;
	uint32_t format = GL_RGB8;
	uint32_t type = GL_UNSIGNED_BYTE;
	uint32_t loadFormat = GL_BGR;

    std::cout << "Loading images:" << std::endl;
    for(auto const&i:imgs)
    {
        std::cout << '.' << std::flush;
        img.load(i.c_str());
        if(width == 0)
        {
            width = img.getWidth();
            height = img.getHeight();
			std::string name = "texture";
			if(!depth)
			{
				name = "texture.color";
			}
			else
			{
				name = "texture.depth";
				type = GL_FLOAT;
				format = GL_RGBA32F;
				loadFormat = GL_RGBA;

                /*img.convertToGrayscale();
	            type = GL_UNSIGNED_BYTE;
                format = GL_R8;
                loadFormat = GL_RED;*/
			}
            tex = vars.reCreate<ge::gl::Texture>(name.c_str(),GL_TEXTURE_2D_ARRAY,format,1,width,height,imgs.size());
        }
        //tex->setData3D((void*)FreeImage_GetBits(img),loadFormat,type,0,GL_TEXTURE_2D_ARRAY,0,0,counter++,width,height,1);
        ge::gl::glTextureSubImage3D(tex->getId(), 0, 0,0, counter++, width, height, 1, loadFormat, type, (void*)FreeImage_GetBits(img));
        tex->texParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        tex->texParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }
    std::cout << std::endl;


	if(!depth)	
    	vars.addFloat("texture.aspect",(float)width/(float)height);

    return imgs.size();
}

void loadTextures(vars::Vars&vars)
{
	int size = loadLfImage(vars, "../data/pavfhd", false);
    size = 8;//glm::sqrt(size);
    vars.add<glm::uvec2>("gridSize",glm::uvec2(static_cast<unsigned int>(size)));
	//loadLfImage(vars, "../data/dummy", true);
	
    fipImage img;
    img.load("../data/brick.jpg");
    ge::gl::Texture* geomTex = vars.reCreate<ge::gl::Texture>("geomTexture",GL_TEXTURE_2D,GL_RGB8,1,img.getWidth(), img.getHeight());
    ge::gl::glTextureSubImage2D(geomTex->getId(), 0, 0,0,img.getWidth(), img.getHeight(), GL_BGR, GL_UNSIGNED_BYTE, (void*)(img.accessPixels()));
}

void loadGeometry(vars::Vars&vars)
{
    auto vao = vars.add<ge::gl::VertexArray>("vao");
    vao->bind();
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile("../data/untitled.obj", 0);
    enum Attribs {ATTR_POSITION=0, ATTR_NORMAL, ATTR_UV};
    auto vbo = vars.add<ge::gl::Buffer>("vboPos",scene->mMeshes[0]->mNumVertices*sizeof(aiVector3D), scene->mMeshes[0]->mVertices);
	vao->addAttrib(vbo,ATTR_POSITION,3,GL_FLOAT);
    vbo = vars.add<ge::gl::Buffer>("vboNorm",scene->mMeshes[0]->mNumVertices*sizeof(aiVector3D), scene->mMeshes[0]->mNormals);
	vao->addAttrib(vbo,ATTR_NORMAL,3,GL_FLOAT);
	std::vector<float> texCoords;
    texCoords.reserve(scene->mMeshes[0]->mNumVertices * 2);
		for(int i = 0; i < scene->mMeshes[0]->mNumVertices; ++i) {
			texCoords[i * 2] = scene->mMeshes[0]->mTextureCoords[0][i].x;
			texCoords[i * 2 + 1] = scene->mMeshes[0]->mTextureCoords[0][i].y;
		}

    vbo = vars.add<ge::gl::Buffer>("vboUv",scene->mMeshes[0]->mNumVertices*sizeof(GL_FLOAT)*2, texCoords.data());
	vao->addAttrib(vbo,ATTR_UV,2,GL_FLOAT,2*sizeof(GL_FLOAT),0);
    //vbo = vars.add<ge::gl::Buffer>("vboUv",scene->mMeshes[0]->mNumVertices*sizeof(aiVector3D), scene->mMeshes[0]->mTextureCoords);
//	vao->addAttrib(vbo,ATTR_UV,2,GL_FLOAT,3*sizeof(GL_FLOAT),0);
}

    GLuint tex;
//convert as -c:v libx265 -pix_fmt yuv420p (pix fmt, only 420 avaliable for gpu dec)
void loadVideoFrames(const char *path, vars::Vars&vars)
{
    AVFormatContext *formatContext = avformat_alloc_context();
    if(!formatContext)
        throw std::runtime_error("Cannot allocate format context memory");

    if(avformat_open_input(&formatContext, path, NULL, NULL) != 0)
        throw std::runtime_error("Cannot open the video file"); 

    if(avformat_find_stream_info(formatContext, NULL) < 0)
        throw std::runtime_error("Cannot get the stream info");

    AVCodec *codec;
    int videoStreamId = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if(videoStreamId < 0)
        throw std::runtime_error("No video stream available");
    if(!codec)
        throw std::runtime_error("No suitable codec found");
    
    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    if(!codecContext)
        throw std::runtime_error("Cannot allocate codec context memory");

    if(avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamId]->codecpar)<0)
        throw std::runtime_error{"Cannot use the file parameters in context"};

    const AVCodecHWConfig *config;
    AVPixelFormat pixFmt;
    for(int i=0;; i++)
    {
        if(!(config = avcodec_get_hw_config(codec, i)))
            throw std::runtime_error("No HW config for codec");
        //vaapi libva-vdpau-driver for nvidia needed
        else if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == AV_HWDEVICE_TYPE_VDPAU)
        {   
            pixFmt = config->pix_fmt;//AV_PIX_FMT_RGB24;
            //std::cerr << av_get_pix_fmt_name(pixFmt) << std::endl;
            break;
        }
        //std::cerr << av_hwdevice_get_type_name(config->device_type) << std::endl;
    }

//default one returns first one that is not hw accel only
/*codecContext->get_format = [](AVCodecContext *ctx,const enum AVPixelFormat *pix_fmts)
   {
       const enum AVPixelFormat *p;
   
       for (p = pix_fmts; *p != -1; p++) {
           if (*p == AV_PIX_FMT_VDPAU )
               return *p;
       }
   
       fprintf(stderr, "Failed to get HW surface format.\n");
        return AV_PIX_FMT_GBRP;
       return AV_PIX_FMT_NONE;
   };
*/

    AVBufferRef *deviceContext = nullptr;
    if(av_hwdevice_ctx_create(&deviceContext, config->device_type, NULL, NULL, 0) < 0)
        throw std::runtime_error("Cannot create HW device");
    codecContext->hw_device_ctx = av_buffer_ref(deviceContext);

    if(avcodec_open2(codecContext, codec, NULL) < 0)
        throw std::runtime_error("Cannot open codec.");

/*
    AVBufferRef *avClContext = nullptr;
                std::vector<char> buff(100);
                int e;
    if(e = av_hwdevice_ctx_create_derived(&avClContext, AV_HWDEVICE_TYPE_OPENCL, deviceContext, 0) < 0)
      {
                    av_make_error_string(buff.data(), buff.size(),e);
                    std::cerr<<buff.data() << std::endl;
                     throw std::runtime_error("Cannot create derived CL context");
    }*/
   AVHWDeviceContext *device_ctx = (AVHWDeviceContext*)deviceContext->data;
   AVVDPAUDeviceContext *k = reinterpret_cast<AVVDPAUDeviceContext*>(device_ctx->hwctx);
//             AVVDPAUDeviceContext *k = reinterpret_cast<AVVDPAUDeviceContext*>(reinterpret_cast<AVHWDeviceContext*>(deviceContext->data)->hwctx); 
    ge::gl::glVDPAUInitNV((void*)(uintptr_t)(k->device), (void*)k->get_proc_address);
    VdpVideoMixerParameter params[] = {
                VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
                VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
                VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT
        };

    uint32_t w = 1920;
    uint32_t h = 1080;
    VdpChromaType ct = VDP_CHROMA_TYPE_420;
    void *param_vals[] = {
                &ct,
                &w,
                &h
        };

    VdpGetProcAddress *get_proc_address = k->get_proc_address;
    /*VdpVideoSurfaceGetParameters *vdp_video_surface_get_parameters;
    get_proc_address(k->device, VDP_FUNC_ID_VIDEO_SURFACE_GET_PARAMETERS, (void**)&vdp_video_surface_get_parameters);*/
    VdpOutputSurfaceCreate *vdp_output_surface_create;
    get_proc_address(k->device, VDP_FUNC_ID_OUTPUT_SURFACE_CREATE, (void**)&vdp_output_surface_create);
    VdpVideoMixerCreate *vdp_video_mixer_create;
    get_proc_address(k->device, VDP_FUNC_ID_VIDEO_MIXER_CREATE, (void**)&vdp_video_mixer_create);
    VdpVideoMixerRender * vdp_video_mixer_render;
    get_proc_address(k->device, VDP_FUNC_ID_VIDEO_MIXER_RENDER, (void**)&vdp_video_mixer_render);
   
    ge::gl::glGenTextures(1, &tex);
 
    VdpOutputSurface surface;
    if(vdp_output_surface_create(k->device, VDP_RGBA_FORMAT_B8G8R8A8, w, h, &surface) != VDP_STATUS_OK)
        throw std::runtime_error("Cannot create VDPAU output surface.");
    VdpVideoMixer mixer;
    if(vdp_video_mixer_create(k->device, 0, nullptr, 3, params, param_vals, &mixer) != VDP_STATUS_OK)
        throw std::runtime_error("Cannot create VDPAU mixer.");
    //tex = vars.reCreate<ge::gl::Texture>("testTex",GL_TEXTURE_2D,GL_RGBA8,1,1920,1080)->getId();
    /*if(vdp_video_surface_get_parameters((VdpVideoSurface)(uintptr_t)frame->data[3], &ct, &w, &h) != VDP_STATUS_OK)
        throw std::runtime_error("Cannot get surface parameters.");*/
//    GLvdpauSurfaceNV nvSurf = ge::gl::glVDPAURegisterOutputSurfaceNV((void *)(uintptr_t)surface,GL_TEXTURE_2D,1,&tex);

    AVPacket packet;
    while(av_read_frame(formatContext, &packet) == 0)
    {
            if(packet.stream_index != videoStreamId)
            {
                av_packet_unref(&packet);
                continue;
            }

        //TODO send packets in batch until filled and read

        if(avcodec_send_packet(codecContext, &packet) < 0)
            throw std::runtime_error("Cannot send packet");  
        while(1)
        {
            AVFrame *frame = av_frame_alloc();
            AVFrame *swFrame = av_frame_alloc();
            if(!frame || !swFrame)
                throw std::runtime_error("Cannot allocate packet/frame");

            int err = avcodec_receive_frame(codecContext, frame);
            if(err == AVERROR_EOF || err == AVERROR(EAGAIN))
            {
                av_frame_free(&frame);
                av_frame_free(&swFrame);
                break;
            }
            else if(err < 0)
                throw std::runtime_error("Cannot receive frame");

            if(frame->format == pixFmt)
            {
                if(vdp_video_mixer_render(mixer, VDP_INVALID_HANDLE, nullptr, VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME, 0, nullptr, (VdpVideoSurface)(uintptr_t)frame->data[3], 0, nullptr, nullptr, surface, nullptr, nullptr, 0, nullptr) != VDP_STATUS_OK)
                    throw std::runtime_error("VDP mixer error!");
                //ge::gl::glVDPAUUnmapSurfacesNV (1, &nvSurf);
                GLvdpauSurfaceNV nvSurf = ge::gl::glVDPAURegisterOutputSurfaceNV((void *)(uintptr_t)surface,GL_TEXTURE_2D,1,&tex);
                ge::gl::glVDPAUSurfaceAccessNV(nvSurf, GL_READ_ONLY);
                ge::gl::glVDPAUMapSurfacesNV (1, &nvSurf);
                int w, h;
                int miplevel = 0;
                ge::gl::glBindTexture(tex, GL_TEXTURE_2D);
                ge::gl::glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_WIDTH, &w);
                ge::gl::glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_HEIGHT, &h);
                std::cerr << w << " " << h << std::endl;
                return;
           }
             
            /*            
            std::cerr << av_get_pix_fmt_name((AVPixelFormat)frame->format);
            if(frame->format == pixFmt)
                if(av_hwframe_transfer_data(swFrame, frame,0) < 0)
                    throw std::runtime_error("Cannot transfer data");
              
                std::vector<char> buff(100);
                int e;
                AVPixelFormat *fmts = new AVPixelFormat[100];
                if((e=av_hwframe_transfer_get_formats(codecContext->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_FROM, fmts.data(), 0)) == 0)
                {
                        std::cerr <<"dzzt";
                    for(int i=0; fmts[i] != AV_PIX_FMT_NONE; i++)
                        std::cerr << av_get_pix_fmt_name(fmts[i]) <<std::endl;
                } 
                 else
                 {
                    av_strerror(e, buff.data(), buff.size());
                    std::cerr<<"error:"<<buff.data();
                 }
                delete [] fmts;
            */

            

            av_frame_free(&frame);
            av_frame_free(&swFrame);
        }
        
        av_packet_unref(&packet);
    }

//av_hwframe_transfer_data(swFrame, frame,0);
//if(swFrame->format == AV_PIX_FMT_VDPAU)
//std::cerr<< "aaa";
/*std::vector<char> buffer(swFrame->width*swFrame->height);
memcpy(buffer.data(), swFrame->data[0], swFrame->width*swFrame->height);*/
}

void LightFields::init()
{
    vars.add<ge::gl::VertexArray>("emptyVao");
    vars.addFloat("input.sensitivity",0.01f);
    vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
    vars.addFloat("camera.fovy",glm::half_pi<float>());
    vars.addFloat("camera.near",.1f);
    vars.addFloat("camera.far",1000.f);
    vars.addFloat("scale",1.f);
    vars.addFloat("z",0.f);
    vars.addUint32("kernel",1);
    vars.addBool("depth",true);
    vars.addBool("printStats",false);
    vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
    vars.addFloat("xSelect",0.f);
    vars.addFloat("ySelect",0.f);
    vars.addFloat("focusDistance",1.f);
    vars.addUint32("mode",2);
    vars.addUint32("frame",0);
    createProgram(vars);
    createCamera(vars);
    loadTextures(vars);
    loadGeometry(vars);
    loadVideoFrames("../data/video.mkv", vars);

    auto tex = vars.get<ge::gl::Texture>("texture.color");
    auto grid = vars.get<glm::uvec2>("gridSize");
    auto stats = vars.add<ge::gl::Buffer>("statistics",tex->getWidth(0)*tex->getHeight(0)*grid->x*grid->y*4);
    unsigned int c=0;
    stats->clear(GL_R32UI, GL_RED, GL_UNSIGNED_INT, &c);
    stats->bindBase(GL_SHADER_STORAGE_BUFFER, 2);
    
    ge::gl::glEnable(GL_DEPTH_TEST);
}

void LightFields::draw()
{

    auto start = std::chrono::steady_clock::now();

    createCamera(vars);
    auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");

    ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
    ge::gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    vars.get<ge::gl::VertexArray>("emptyVao")->bind();
    auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");

    drawGrid(vars);

    ge::gl::glBindTextureUnit(0,vars.get<ge::gl::Texture>("texture.color")->getId());
    //ge::gl::glBindTextureUnit(1,vars.get<ge::gl::Texture>("texture.depth")->getId());
    ge::gl::glBindTextureUnit(2,vars.get<ge::gl::Texture>("geomTexture")->getId());
    ge::gl::glBindTextureUnit(3,tex);
    vars.get<ge::gl::Program>("lfProgram")
    ->setMatrix4fv("mvp",glm::value_ptr(projection->getProjection()*view->getView()))
    ->set1f("aspect",vars.getFloat("texture.aspect"))
    ->set1f("xSelect",vars.getFloat("xSelect"))
    ->set1f("ySelect",vars.getFloat("ySelect"))
    //->set1f("far",vars.getFloat("camera.far"))
    ->set1f("scale",vars.getFloat("scale"))
    ->set1f("z",vars.getFloat("z"))
    ->set1i("kernel",vars.getUint32("kernel"))
    ->set1f("focusDistance",vars.getFloat("focusDistance"))
    ->set1i("mode",vars.getBool("mode"))
    ->set1i("frame",vars.getUint32("frame"))
    ->set1i("depth",vars.getBool("depth"))
    ->set1i("printStats",vars.getBool("printStats"))
    //->set2uiv("winSize",glm::value_ptr(*vars.get<glm::uvec2>("windowSize")))
    ->set2uiv("gridSize",glm::value_ptr(*vars.get<glm::uvec2>("gridSize")))
    ->setMatrix4fv("view",glm::value_ptr(view->getView()))
    //->setMatrix4fv("proj",glm::value_ptr(projection->getProjection()))
    ->use();
    ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

    vars.get<ge::gl::Program>("geomProgram")
    ->setMatrix4fv("mvp",glm::value_ptr(projection->getProjection()*view->getView()))
    ->set1f("aspect",vars.getFloat("texture.aspect"))
    ->use();
    vars.get<ge::gl::VertexArray>("vao")->bind();
    vars.get<ge::gl::Buffer>("statistics")->bind(GL_SHADER_STORAGE_BUFFER);
   
    ge::gl::glDrawArrays(GL_TRIANGLES,0,1000);

    vars.get<ge::gl::Buffer>("statistics")->unbind(GL_SHADER_STORAGE_BUFFER);
    vars.get<ge::gl::VertexArray>("vao")->unbind();

    GLint nCurAvailMemoryInKB = 0;
    /*ge::gl::glGetIntegerv( GL_TEXTURE_FREE_MEMORY_ATI,
                           &nCurAvailMemoryInKB );*/
    
    if(vars.getBool("printStats"))
    {
        ge::gl::glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        GLuint *ptr = (GLuint*)vars.get<ge::gl::Buffer>("statistics")->map();
        auto tex = vars.get<ge::gl::Texture>("texture.color");
        auto grid = vars.get<glm::uvec2>("gridSize");
        const uint32_t gridSize = grid->x*grid->y;
        const uint32_t picSize = tex->getWidth(0)*tex->getHeight(0);
        const uint32_t statSize = picSize*gridSize;
        std::vector<uint8_t> data(statSize);
        for(int i=0; i<statSize; i++)
            data[i] = static_cast<uint8_t>(ptr[i])*100;

        vars.get<ge::gl::Buffer>("statistics")->unmap();

        for(int i=0; i<gridSize; i++)
        {
            fipImage img(FIT_BITMAP, tex->getWidth(0), tex->getHeight(0), 8);
            memcpy(img.accessPixels(), data.data()+i*picSize, picSize);
            std::string path = "./stats/"+std::to_string(i)+".pgm";
            img.save(path.c_str());           
        }

        vars.getBool("printStats") = false;
    }
    
    drawImguiVars(vars);
    ImGui::LabelText("freeMemory","%i MB",nCurAvailMemoryInKB / 1024);
    swap();
/*
    auto end = std::chrono::steady_clock::now();
    constexpr int frameTime = 41;
    int left = frameTime - std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if(left > 0)
       std::this_thread::sleep_for(std::chrono::milliseconds(left));
    vars.getUint32("frame") += (vars.getUint32("frame") == 19) ? -19 : 1; */
}

void LightFields::key(SDL_Event const& event, bool DOWN)
{
    auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
    (*keys)[event.key.keysym.sym] = DOWN;

    if(event.key.keysym.sym == SDLK_ESCAPE)
        exit(0); //I know, dirty...
}

void LightFields::mouseMove(SDL_Event const& e)
{
    auto sensitivity = vars.getFloat("input.sensitivity");
    auto orbitCamera =
        vars.getReinterpret<basicCamera::OrbitCamera>("view");
    auto const windowSize     = vars.get<glm::uvec2>("windowSize");
    auto const orbitZoomSpeed = 0.1f;//vars.getFloat("args.camera.orbitZoomSpeed");
    auto const xrel           = static_cast<float>(e.motion.xrel);
    auto const yrel           = static_cast<float>(e.motion.yrel);
    auto const mState         = e.motion.state;
    if (mState & SDL_BUTTON_LMASK)
    {
        if (orbitCamera)
        {
            orbitCamera->addXAngle(yrel * sensitivity);
            orbitCamera->addYAngle(xrel * sensitivity);
        }
    }
    if (mState & SDL_BUTTON_RMASK)
    {
        if (orbitCamera) orbitCamera->addDistance(yrel * orbitZoomSpeed);
    }
    if (mState & SDL_BUTTON_MMASK)
    {
        orbitCamera->addXPosition(+orbitCamera->getDistance() * xrel /
                                  float(windowSize->x) * 2.f);
        orbitCamera->addYPosition(-orbitCamera->getDistance() * yrel /
                                  float(windowSize->y) * 2.f);
    }
}

void LightFields::resize(uint32_t x,uint32_t y)
{
    auto windowSize = vars.get<glm::uvec2>("windowSize");
    windowSize->x = x;
    windowSize->y = y;
    vars.updateTicks("windowSize");
    ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[])
{
    LightFields app{argc, argv};
    app.start();
    return EXIT_SUCCESS;
}
