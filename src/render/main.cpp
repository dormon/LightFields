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
#include <CL/cl.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <SDL2CPP/Exception.h>

#include <gpuDecoder.h>

#include<fstream>
#include<sstream>
#include<string>
#include<thread>
#include<mutex>
#include<condition_variable>

namespace fs = std::experimental::filesystem;

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

void loadTextures(vars::Vars&vars)
{ 
    uint32_t size = 8;
    vars.addUint32("lf.width", 1920);
    vars.addUint32("lf.height", 1080);
    vars.addFloat("texture.aspect",1920.0/1080.0);
    //TODO get from container
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
    vars.addUint32("trianglesNum", scene->mMeshes[0]->mNumVertices);
}

void asyncVideoLoading(vars::Vars &vars)
{
    SDL_GLContext c = SDL_GL_CreateContext(*vars.get<SDL_Window*>("mainWindow")); 
    SDL_GL_MakeCurrent(*vars.get<SDL_Window*>("mainWindow"),c);
    ge::gl::init(SDL_GL_GetProcAddress);
    ge::gl::setHighDebugMessage();

    auto decoder = std::make_unique<GpuDecoder>( "../data/video.mkv");
  
    auto rdyMutex = vars.get<std::mutex>("rdyMutex");
    auto rdyCv = vars.get<std::condition_variable>("rdyCv");
    
    while(true)
    {
        int index = decoder->getActiveBufferIndex();
        std::string nextTexturesName = "lfTextures" + std::to_string(index);
        //TODO id this copy deep?
        vars.reCreateVector<GLuint64>(nextTexturesName, decoder->getFrames(64));
        
        GLsync fence = ge::gl::glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
        ge::gl::glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 9999999);
    
        vars.getUint32("lfTexturesIndex") = index;

        //TODO without waiting? async draw to increase responsitivity
        vars.getBool("loaded") = true;
        rdyCv->notify_all();   
    }
}

void LightFields::init()
{
    window->createContext("loading", 450u, sdl2cpp::Window::CORE, sdl2cpp::Window::DEBUG);
    vars.add<SDL_GLContext>("loadingContext", window->getContext("loading"));
    vars.add<SDL_GLContext>("renderingContext", window->getContext("rendering"));
    vars.add<SDL_Window*>("mainWindow", window->getWindow());
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);  
    
    vars.addUint32("lfTexturesIndex", 0);
    auto rdyMutex = vars.add<std::mutex>("rdyMutex");
    auto rdyCv = vars.add<std::condition_variable>("rdyCv");
    vars.addBool("loaded", false);
    vars.add<std::thread>("loadingThread",asyncVideoLoading, std::ref(vars));
    {
        std::unique_lock<std::mutex> lck(*rdyMutex);
        rdyCv->wait(lck, [this]{return vars.getBool("loaded");});
    }

    std::string currentTexturesName = "lfTextures" + std::to_string(vars.getUint32("lfTexturesIndex"));
    std::vector<GLuint64> t = vars.getVector<GLuint64>(currentTexturesName);
    for(auto a : t)
       	ge::gl::glMakeTextureHandleResidentARB(a);
 
    SDL_GL_MakeCurrent(*vars.get<SDL_Window*>("mainWindow"),window->getContext("rendering"));

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

    auto grid = vars.get<glm::uvec2>("gridSize");
    auto stats = vars.add<ge::gl::Buffer>("statistics",vars.getUint32("lf.width")*vars.getUint32("lf.height")*grid->x*grid->y*4);
    unsigned int c=0;
    stats->clear(GL_R32UI, GL_RED, GL_UNSIGNED_INT, &c);
    stats->bindBase(GL_SHADER_STORAGE_BUFFER, 2);
    
    ge::gl::glEnable(GL_DEPTH_TEST);
}

void LightFields::draw()
{
    std::string currentTexturesName = "lfTextures" + std::to_string(vars.getUint32("lfTexturesIndex"));
    
    std::vector<GLuint64> t = vars.getVector<GLuint64>(currentTexturesName);
    for(auto a : t)
       	ge::gl::glMakeTextureHandleResidentARB(a);
    
    auto start = std::chrono::steady_clock::now();

    createCamera(vars);
    auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");

    ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
    ge::gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    vars.get<ge::gl::VertexArray>("emptyVao")->bind();
    auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");

    drawGrid(vars);

    ge::gl::glBindTextureUnit(2,vars.get<ge::gl::Texture>("geomTexture")->getId());
    
    auto program = vars.get<ge::gl::Program>("lfProgram");
    program
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

    vars.get<ge::gl::Buffer>("statistics")->bind(GL_SHADER_STORAGE_BUFFER);
    ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("lfTextures"), 64, vars.getVector<GLuint64>(currentTexturesName).data());

    ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

    vars.get<ge::gl::Program>("geomProgram")
    ->setMatrix4fv("mvp",glm::value_ptr(projection->getProjection()*view->getView()))
    ->set1f("aspect",vars.getFloat("texture.aspect"))
    ->use();
    vars.get<ge::gl::VertexArray>("vao")->bind();
      
    ge::gl::glDrawArrays(GL_TRIANGLES,0,vars.getUint32("trianglesNum"));

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

    GLsync fence = ge::gl::glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    ge::gl::glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 9999999);

    
    for(auto a : t)
       	ge::gl::glMakeTextureHandleNonResidentARB(a);

    auto rdyMutex = vars.get<std::mutex>("rdyMutex");
    auto rdyCv = vars.get<std::condition_variable>("rdyCv");
    {
        std::unique_lock<std::mutex> lck(*rdyMutex);
        rdyCv->wait(lck, [this]{return vars.getBool("loaded");});
    }
    vars.getBool("loaded") = false;


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
    try
    {
    app.start();
    }
    catch(sdl2cpp::ex::Exception &e)
    {
    std::cerr << e.what();
   }
    app.vars.get<std::thread>("loadingThread")->join();
    return EXIT_SUCCESS;
}
