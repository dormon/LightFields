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

#include<assimp/Importer.hpp>
#include <assimp/scene.h>

#include<fstream>
#include<sstream>
#include<string>

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
    std::ifstream ifs("../src/shader/lf.vert");
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER, "#version 450\n", buffer.str());

    ifs.close();
    buffer.str("");

    ifs.open("../src/shader/lf.frag");
    buffer << ifs.rdbuf();
    auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER, "#version 450\n", buffer.str());

    vars.reCreate<ge::gl::Program>("lfProgram",vs,fs);

    ifs.close();
    buffer.str("");

    ifs.open("../src/shader/geom.vert");
    buffer << ifs.rdbuf();
    vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER, "#version 450\n", buffer.str());

    ifs.close();
    buffer.str("");

    ifs.open("../src/shader/geom.frag");
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
				format = GL_RGBA16;
				loadFormat = GL_RGBA;
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

void loadTextues(vars::Vars&vars)
{
	int size = loadLfImage(vars, "../data/t", false);
    size = glm::sqrt(size);
    vars.add<glm::uvec2>("gridSize",glm::uvec2(static_cast<unsigned int>(size)));
	loadLfImage(vars, "../data/td", true);
	
    fipImage img;
    img.load("../data/brick.jpg");
    ge::gl::Texture* geomTex = vars.reCreate<ge::gl::Texture>("geomTexture",GL_TEXTURE_2D,GL_RGB8,1,img.getWidth(), img.getHeight());
    ge::gl::glTextureSubImage2D(geomTex->getId(), 0, 0,0,img.getWidth(), img.getHeight(), GL_BGR, GL_UNSIGNED_BYTE, (void*)FreeImage_GetBits(img));
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
	float *texCoords = new float[scene->mMeshes[0]->mNumVertices * 2];
		for(int i = 0; i < scene->mMeshes[0]->mNumVertices; ++i) {
			texCoords[i * 2] = scene->mMeshes[0]->mTextureCoords[0][i].x;
			texCoords[i * 2 + 1] = scene->mMeshes[0]->mTextureCoords[0][i].y;
		}

    vbo = vars.add<ge::gl::Buffer>("vboUv",scene->mMeshes[0]->mNumVertices*sizeof(GL_FLOAT)*2, texCoords);
	vao->addAttrib(vbo,ATTR_UV,2,GL_FLOAT,2*sizeof(GL_FLOAT),0);
    //vbo = vars.add<ge::gl::Buffer>("vboUv",scene->mMeshes[0]->mNumVertices*sizeof(aiVector3D), scene->mMeshes[0]->mTextureCoords);
//	vao->addAttrib(vbo,ATTR_UV,2,GL_FLOAT,3*sizeof(GL_FLOAT),0);
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
    vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
    vars.addFloat("xSelect",0.f);
    vars.addFloat("ySelect",0.f);
    vars.addFloat("focusDistance",1.f);
    vars.addUint32("mode",2);
    createProgram(vars);
    createCamera(vars);
    loadTextues(vars);
    loadGeometry(vars);

    ge::gl::glEnable(GL_DEPTH_TEST);
}

void LightFields::draw()
{
    createCamera(vars);
    /*
       auto view = vars.get<basicCamera::FreeLookCamera>("view");
       float freeCameraSpeed = 0.01f;
       auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
       for (int a = 0; a < 3; ++a)
       view->move(a, float((*keys)["d s"[a]] - (*keys)["acw"[a]]) *
       freeCameraSpeed);
     */
    auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");

    ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
    ge::gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    vars.get<ge::gl::VertexArray>("emptyVao")->bind();
    auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");

    drawGrid(vars);

    ge::gl::glBindTextureUnit(0,vars.get<ge::gl::Texture>("texture.color")->getId());
    ge::gl::glBindTextureUnit(1,vars.get<ge::gl::Texture>("texture.depth")->getId());
    ge::gl::glBindTextureUnit(2,vars.get<ge::gl::Texture>("geomTexture")->getId());

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
    ->set1i("depth",vars.getBool("depth"))
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
   
	 ge::gl::glDrawArrays(GL_TRIANGLES,0,1000);

    vars.get<ge::gl::VertexArray>("vao")->unbind();

    drawImguiVars(vars);

    GLint nCurAvailMemoryInKB = 0;
    /*ge::gl::glGetIntegerv( GL_TEXTURE_FREE_MEMORY_ATI,
                           &nCurAvailMemoryInKB );*/
    ImGui::LabelText("freeMemory","%i MB",nCurAvailMemoryInKB / 1024);
    swap();
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
    /*
       auto const xrel           = static_cast<float>(e.motion.xrel);
       auto const yrel           = static_cast<float>(e.motion.yrel);
       auto view = vars.get<basicCamera::FreeLookCamera>("view");
       auto sensitivity = vars.getFloat("input.sensitivity");
       if (e.motion.state & SDL_BUTTON_LMASK) {
       view->setAngle(
       1, view->getAngle(1) + xrel * sensitivity);
       view->setAngle(
       0, view->getAngle(0) + yrel * sensitivity);
       }
     */

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
