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

#include<fstream>
#include<sstream>
#include<string>

namespace fs = std::experimental::filesystem;

std::vector<std::string>getDirectoryImages(std::string const&dir){
	if(!fs::is_directory(dir))
		throw std::invalid_argument(std::string("path: ")+dir+" is not directory");
	std::vector<std::string>result;
	for(auto& p: fs::directory_iterator(dir))
		result.push_back(p.path().c_str());
	return result;
}

std::vector<std::string>sortImages(std::vector<std::string>const&imgs){
	std::vector<std::string>res = imgs;
	std::regex number(".*[^0-9]([0-9]+)\\..*");
	for(size_t i=0;i<res.size();++i)
		for(size_t i=0;i<res.size()-1;++i){
			auto a = res.at(i);
			auto b = res.at(i+1);
			auto an = std::atoi(std::regex_replace(a,number,"$1").c_str());
			auto bn = std::atoi(std::regex_replace(b,number,"$1").c_str());
			if(an>bn){
				res[i] = b;
				res[i+1] = a;
			}
		}
	return res;
}

std::vector<std::string>getLightFieldImageNames(std::string const&d){
	return sortImages(getDirectoryImages(d));
}



class LightFields: public simple3DApp::Application{
	public:
		LightFields(int argc, char* argv[]) : Application(argc, argv) {}
		virtual ~LightFields(){}
		virtual void draw() override;

		vars::Vars vars;

		virtual void                init() override;
		//void                        parseArguments();
		virtual void                mouseMove(SDL_Event const& event) override;
		virtual void                key(SDL_Event const& e, bool down) override;
		virtual void                resize(uint32_t x,uint32_t y) override;
};


void createProgram(vars::Vars&vars){

	std::ifstream vss("../src/shader/vertex.vert");
	std::ifstream fss("../src/shader/fragment.frag");
	std::stringstream buffer;
	buffer << vss.rdbuf();

	auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
			"#version 450\n",
			buffer.str()
			);

	buffer.str("");	
	buffer << fss.rdbuf();
	auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
			"#version 450\n",
			buffer.str()
			);
	vars.reCreate<ge::gl::Program>("program",vs,fs);
}

void createView(vars::Vars&vars){
	if(notChanged(vars,"all",__FUNCTION__,{}))return;

	//vars.reCreate<basicCamera::FreeLookCamera>("view");

	vars.add<basicCamera::OrbitCamera>("view");
}

void createProjection(vars::Vars&vars){
	if(notChanged(vars,"all",__FUNCTION__,{"windowSize","camera.fovy","camera.near","camera.far"}))return;

	auto windowSize = vars.get<glm::uvec2>("windowSize");
	auto width = windowSize->x;
	auto height = windowSize->y;
	auto aspect = (float)width/(float)height;
	auto near = vars.getFloat("camera.near");
	auto far  = vars.getFloat("camera.far" );
	auto fovy = vars.getFloat("camera.fovy");

	vars.reCreate<basicCamera::PerspectiveCamera>("projection",fovy,aspect,near,far);
}

void createCamera(vars::Vars&vars){
	createProjection(vars);
	createView(vars);
}



void loadImage(vars::Vars&vars){
	if(notChanged(vars,"all",__FUNCTION__,{}))return;

	//auto imgs = getLightFieldImageNames("/media/data/Pavilion/100mm-baseline_8x8-grid/");
	//auto imgs = getLightFieldImageNames("/media/data/Pavilion/200mm-baseline_8x8-grid");
	//auto imgs = getLightFieldImageNames("../data/200pav");
	//auto imgs = getLightFieldImageNames("../data/100class");
	auto imgs = getLightFieldImageNames("../data/bun");
	//auto imgs = getLightFieldImageNames("../data/lego");
	//auto imgs = getLightFieldImageNames("../data/100cornel");
	//auto imgs = getLightFieldImageNames("/media/data/Cornell/100mm-baseline_8x8-grid");
	//auto imgs = getLightFieldImageNames("/media/data/Cornell/200mm-baseline_8x8-grid");
	//auto imgs = getLightFieldImageNames("/home/dormon/Desktop/000065");

	fipImage img;
	FIBITMAP* bm;
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint8_t>data;
	ge::gl::Texture*tex;
	uint32_t counter = 0;
	
	std::cout << "Loading images:" << std::endl;
	for(auto const&i:imgs){
		std::cout << '.' << std::flush;
		img.load(i.c_str());
		if(width == 0){
			width = img.getWidth();
			height = img.getHeight();
			tex = vars.reCreate<ge::gl::Texture>("texture",GL_TEXTURE_2D_ARRAY,GL_RGB8,1,width,height,imgs.size());
			data.resize(width*height*3);
		}

		/*for(size_t y=0;y<height;++y){
			for(size_t x=0;x<width;++x){
				RGBQUAD p;
				img.getPixelColor(x,y,&p);
				data[(y*img.getWidth()+x)*3+0] = p.rgbRed;
				data[(y*img.getWidth()+x)*3+1] = p.rgbGreen;
				data[(y*img.getWidth()+x)*3+2] = p.rgbBlue;
			}
		}*/
		//tex->setData3D(data.data(),GL_RGB,GL_UNSIGNED_BYTE,0,GL_TEXTURE_2D_ARRAY,0,0,counter++,img.getWidth(),img.getHeight(),1);
		//ge::gl::glTextureSubImage3D(tex->getId(), 0, 0 ,0, counter++, width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, data.data());
		ge::gl::glTextureSubImage3D(tex->getId(), 0, 0 ,0, counter++, width, height, 1, GL_BGR, GL_UNSIGNED_BYTE, (void*)FreeImage_GetBits(img));
		tex->texParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		tex->texParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
//    ge::gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	vars.addFloat("texture.aspect",(float)width/(float)height);
	std::cout << std::endl;
}

void LightFields::init(){


	vars.add<ge::gl::VertexArray>("emptyVao");
	vars.addFloat("input.sensitivity",0.01f);
	vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
	vars.addFloat("camera.fovy",glm::half_pi<float>());
	vars.addFloat("camera.near",.1f);
	vars.addFloat("camera.far",1000.f);
	vars.addFloat("scale",1.f);
	vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
	vars.addFloat("xSelect",0.f);
	vars.addFloat("ySelect",0.f);
	vars.addFloat("focusDistance",1.f);
	vars.addUint32("mode",2);
	createProgram(vars);
	createCamera(vars);
	loadImage(vars);	
}

void LightFields::draw(){
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
	ge::gl::glClear(GL_COLOR_BUFFER_BIT);
	vars.get<ge::gl::VertexArray>("emptyVao")->bind();
	auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");

	drawGrid(vars);

	ge::gl::glBindTextureUnit(0,vars.get<ge::gl::Texture>("texture")->getId());
	
	vars.get<ge::gl::Program>("program")
		->setMatrix4fv("mvp",glm::value_ptr(projection->getProjection()*view->getView()))
		->set1f("aspect",vars.getFloat("texture.aspect"))
		->set1f("xSelect",vars.getFloat("xSelect"))
		->set1f("ySelect",vars.getFloat("ySelect"))
		->set1f("far",vars.getFloat("camera.far"))
		->set1f("scale",vars.getFloat("scale"))
		->set1f("focusDistance",vars.getFloat("focusDistance"))
		->set1i("mode",vars.getBool("mode"))
		->set2uiv("winSize",glm::value_ptr(*vars.get<glm::uvec2>("windowSize")))
		->set2uiv("gridSize",glm::value_ptr(glm::uvec2(8,8)))
		->setMatrix4fv("view",glm::value_ptr(view->getView()))
		->setMatrix4fv("proj",glm::value_ptr(projection->getProjection()))
		->use();
	ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
	vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

	drawImguiVars(vars);

	GLint nCurAvailMemoryInKB = 0;
	ge::gl::glGetIntegerv( GL_TEXTURE_FREE_MEMORY_ATI,
			&nCurAvailMemoryInKB );
	ImGui::LabelText("freeMemory","%i MB",nCurAvailMemoryInKB / 1024);
	swap();
}

void LightFields::key(SDL_Event const& event, bool DOWN) {
	auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
	(*keys)[event.key.keysym.sym] = DOWN;
}

void LightFields::mouseMove(SDL_Event const& e) {
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
	if (mState & SDL_BUTTON_LMASK) {
		if (orbitCamera) {
			orbitCamera->addXAngle(yrel * sensitivity);
			orbitCamera->addYAngle(xrel * sensitivity);
		}
	}
	if (mState & SDL_BUTTON_RMASK) {
		if (orbitCamera) orbitCamera->addDistance(yrel * orbitZoomSpeed);
	}
	if (mState & SDL_BUTTON_MMASK) {
		orbitCamera->addXPosition(+orbitCamera->getDistance() * xrel /
				float(windowSize->x) * 2.f);
		orbitCamera->addYPosition(-orbitCamera->getDistance() * yrel /
				float(windowSize->y) * 2.f);
	}

}

void LightFields::resize(uint32_t x,uint32_t y){
	auto windowSize = vars.get<glm::uvec2>("windowSize");
	windowSize->x = x;
	windowSize->y = y;
	vars.updateTicks("windowSize");
	ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
	LightFields app{argc, argv};
	app.start();
	return EXIT_SUCCESS;
}
