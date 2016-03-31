#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <sstream>
#include <fstream>

#include <GL/glew.h>

#include "glm-0.9.2.6/glm/glm.hpp"
#include "glm-0.9.2.6/glm/gtc/matrix_transform.hpp"
#include "glm-0.9.2.6/glm/gtc/type_ptr.hpp"

#include "src/keyboard.h"
#include "src/joystick.h"
#include "src/glhelper.h"
#include "src/timer.h"
#include "src/misc.h"
#include "src/obj.h"

#define WIDTH  1280
#define HEIGHT 720

struct vertex_ {
	GLfloat  x,  y,  z;
	GLfloat nx, ny, nz;
	GLfloat tx, ty, tz;
	GLfloat _tx, _ty, _tz;
	GLfloat _bx, _by, _bz;
	GLfloat _nx, _ny, _nz;
	GLfloat padding[14];
};

void computeTangentSpaceMatrix(vertex_& p0, const vertex_& p1, const vertex_& p2) {
	GLfloat v1x, v1y, v1z, v2x, v2y, v2z, u1x, u1y, u2x, u2y, det;

	v1x = p1.x - p0.x;
	v1y = p1.y - p0.y;
	v1z = p1.z - p0.z;

	v2x = p2.x - p0.x;
	v2y = p2.y - p0.y;
	v2z = p2.z - p0.z;

	u1x = p1.tx - p0.tx;
	u1y = p1.ty - p0.ty;

	u2x = p2.tx - p0.tx;
	u2y = p2.ty - p0.ty;

	det = u1x * u2y - u2x * u1y;

	p0._tx = (v1x * u2y - v2x * u1y) / det;
	p0._ty = (v1y * u2y - v2y * u1y) / det;
	p0._tz = (v1z * u2y - v2z * u1y) / det;

	p0._bx = (-v1x * u2x + v2x * u1x) / det;
	p0._by = (-v1y * u2x + v2y * u1x) / det;
	p0._bz = (-v1z * u2x + v2z * u1x) / det;

	p0._nx = p0._by * p0._tz - p0._bz * p0._ty;
	p0._ny = p0._bz * p0._tx - p0._bx * p0._tz;
	p0._nz = p0._bx * p0._ty - p0._by * p0._tx;
}

int main(int argc, char *argv[]) {
	// input devices
	cKeyboard kb;
	cJoystick js; joystick_position jp[2];
	
	// teapot
//	cObj obj("media/dragon_smooth.obj");
	cObj obj("media/teapot.obj");

	// timers
	cTimer t0; double elapsed0;
	cTimer t1; double elapsed1;
	cTimer t2; double elapsed2;

	// buffer for screen grabs
	unsigned char *buffer = new unsigned char[WIDTH * HEIGHT * 4];

	// application is active.. fullscreen flag.. screen grab.. video grab..
	bool active = true, fullscreen = false, grab = false, video = false, normal = true;

	// setup an opengl context
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,   8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,  16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_Surface *screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, (fullscreen ? SDL_FULLSCREEN : 0) | SDL_HWSURFACE | SDL_OPENGL);

	// initialize the extension wrangler
	glewInit();

	// for handling events
	SDL_Event event;

	// some output
	std::cout << glGetString(GL_VERSION)<< std::endl;
	std::cout << glGetString(GL_SHADING_LANGUAGE_VERSION)<< std::endl;
	std::cout << glewGetString(GLEW_VERSION)<< std::endl;
	std::cout << glGetString(GL_EXTENSIONS)<< std::endl;

	// set up the cube map texture
	SDL_Surface *xpos = IMG_Load("media/xpos.png");	SDL_Surface *xneg = IMG_Load("media/xneg.png");
	SDL_Surface *ypos = IMG_Load("media/ypos.png");	SDL_Surface *yneg = IMG_Load("media/yneg.png");
	SDL_Surface *zpos = IMG_Load("media/zpos.png");	SDL_Surface *zneg = IMG_Load("media/zneg.png");
	GLuint cubemap_texture;
	setupCubeMap(cubemap_texture, xpos, xneg, ypos, yneg, zpos, zneg);
	SDL_FreeSurface(xneg);	SDL_FreeSurface(xpos);
	SDL_FreeSurface(yneg);	SDL_FreeSurface(ypos);
	SDL_FreeSurface(zneg);	SDL_FreeSurface(zpos);

	// set our viewport, clear color and depth, and enable depth testing
	glViewport(0, 0, WIDTH, HEIGHT);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// load our shaders and compile them.. create a program and link it
	GLuint glProgram, glShaderV, glShaderF;
	createProgram(glProgram, glShaderV, glShaderF, "src/vertex.sh", "src/fragment.sh");
	GLint PVM    = glGetUniformLocation(glProgram, "PVM");
	GLint vertex = glGetAttribLocation(glProgram, "vertex");
	// cube vertices for vertex buffer object
	GLfloat cube_vertices[] = {
	  -1.0,  1.0,  1.0,
	  -1.0, -1.0,  1.0,
	   1.0, -1.0,  1.0,
	   1.0,  1.0,  1.0,
	  -1.0,  1.0, -1.0,
	  -1.0, -1.0, -1.0,
	   1.0, -1.0, -1.0,
	   1.0,  1.0, -1.0,
	};
	GLuint vbo_cube_vertices;
	glGenBuffers(1, &vbo_cube_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_vertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
	//glBindBuffer(GL_ARRAY_BUFFER, 0);
	glEnableVertexAttribArray(vertex);
	glVertexAttribPointer(vertex, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// cube indices for index buffer object
	GLushort cube_indices[] = {
	  0, 1, 2, 3,
	  3, 2, 6, 7,
	  7, 6, 5, 4,
	  4, 5, 1, 0,
	  0, 3, 7, 4,
	  1, 2, 6, 5,
	};
	GLuint ibo_cube_indices;
	glGenBuffers(1, &ibo_cube_indices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_cube_indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	GLuint glProgram1, glShaderV1, glShaderF1;
	createProgram(glProgram1, glShaderV1, glShaderF1, "src/vertex1.sh", "src/fragment1.sh");
	GLint vertex1         = glGetAttribLocation(glProgram1, "vertex");
	GLint normal1         = glGetAttribLocation(glProgram1, "normal");
	GLint light_position1 = glGetUniformLocation(glProgram1, "light_position");
	GLint Projection1     = glGetUniformLocation(glProgram1, "Projection");
	GLint View1           = glGetUniformLocation(glProgram1, "View");
	GLint Model1          = glGetUniformLocation(glProgram1, "Model");
	obj.setupBufferObjects();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	GLuint texture2;
	SDL_Surface *tex2 = IMG_Load("media/texture_4.png");
	setupTexture(texture2, tex2);
	SDL_FreeSurface(tex2);
	GLuint texture2normal;
	SDL_Surface *tex2nor = IMG_Load("media/normal_4.png");
	setupTexture(texture2normal, tex2nor);
	SDL_FreeSurface(tex2nor);

	GLuint glProgram2, glShaderV2, glShaderF2;
	createProgram(glProgram2, glShaderV2, glShaderF2, "src/vertex2.sh", "src/fragment2.sh");
	GLint vertex2         = glGetAttribLocation(glProgram2, "vertex");
	GLint normal2         = glGetAttribLocation(glProgram2, "normal");
	GLint texcoord2       = glGetAttribLocation(glProgram2, "texcoord");
	GLint _tangent2       = glGetAttribLocation(glProgram2, "_tangent");
	GLint _bitangent2     = glGetAttribLocation(glProgram2, "_bitangent");
	GLint _normal2        = glGetAttribLocation(glProgram2, "_normal");
	GLint texture_sample2 = glGetUniformLocation(glProgram2, "texture_sample");
	GLint normal_sample2  = glGetUniformLocation(glProgram2, "normal_sample");
	GLint light_position2 = glGetUniformLocation(glProgram2, "light_position");
	GLint Projection2     = glGetUniformLocation(glProgram2, "Projection");
	GLint View2           = glGetUniformLocation(glProgram2, "View");
	GLint Model2          = glGetUniformLocation(glProgram2, "Model");
	GLint flag2           = glGetUniformLocation(glProgram2, "flag");

	vertex_ vertices[] = {
		{-1.0,  1.0,  1.0,  0.0,  0.0,  1.0,  0.0,  0.0,  0.0},
		{-1.0, -1.0,  1.0,  0.0,  0.0,  1.0,  0.0,  1.0,  0.0},
		{ 1.0, -1.0,  1.0,  0.0,  0.0,  1.0,  1.0,  1.0,  0.0},
		{ 1.0,  1.0,  1.0,  0.0,  0.0,  1.0,  1.0,  0.0,  0.0},

		{ 1.0,  1.0, -1.0,  0.0,  0.0, -1.0,  0.0,  0.0,  0.0},
		{ 1.0, -1.0, -1.0,  0.0,  0.0, -1.0,  0.0,  1.0,  0.0},
		{-1.0, -1.0, -1.0,  0.0,  0.0, -1.0,  1.0,  1.0,  0.0},
		{-1.0,  1.0, -1.0,  0.0,  0.0, -1.0,  1.0,  0.0,  0.0},

		{ 1.0,  1.0,  1.0,  1.0,  0.0,  0.0,  0.0,  0.0,  0.0},
		{ 1.0, -1.0,  1.0,  1.0,  0.0,  0.0,  0.0,  1.0,  0.0},
		{ 1.0, -1.0, -1.0,  1.0,  0.0,  0.0,  1.0,  1.0,  0.0},
		{ 1.0,  1.0, -1.0,  1.0,  0.0,  0.0,  1.0,  0.0,  0.0},

		{-1.0,  1.0, -1.0, -1.0,  0.0,  0.0,  0.0,  0.0,  0.0},
		{-1.0, -1.0, -1.0, -1.0,  0.0,  0.0,  0.0,  1.0,  0.0},
		{-1.0, -1.0,  1.0, -1.0,  0.0,  0.0,  1.0,  1.0,  0.0},
		{-1.0,  1.0,  1.0, -1.0,  0.0,  0.0,  1.0,  0.0,  0.0},
	};

	computeTangentSpaceMatrix(vertices[0], vertices[1], vertices[2]);
	computeTangentSpaceMatrix(vertices[1], vertices[2], vertices[3]);
	computeTangentSpaceMatrix(vertices[2], vertices[3], vertices[0]);
	computeTangentSpaceMatrix(vertices[3], vertices[0], vertices[1]);

	computeTangentSpaceMatrix(vertices[4], vertices[5], vertices[6]);
	computeTangentSpaceMatrix(vertices[5], vertices[6], vertices[7]);
	computeTangentSpaceMatrix(vertices[6], vertices[7], vertices[4]);
	computeTangentSpaceMatrix(vertices[7], vertices[4], vertices[5]);

	computeTangentSpaceMatrix(vertices[8], vertices[9], vertices[10]);
	computeTangentSpaceMatrix(vertices[9], vertices[10], vertices[11]);
	computeTangentSpaceMatrix(vertices[10], vertices[11], vertices[8]);
	computeTangentSpaceMatrix(vertices[11], vertices[8], vertices[9]);

	computeTangentSpaceMatrix(vertices[12], vertices[13], vertices[14]);
	computeTangentSpaceMatrix(vertices[13], vertices[14], vertices[15]);
	computeTangentSpaceMatrix(vertices[14], vertices[15], vertices[12]);
	computeTangentSpaceMatrix(vertices[15], vertices[12], vertices[13]);

	unsigned int indices[] = {
		 0,  1,  2,  3,
		 4,  5,  6,  7,
		 8,  9, 10, 11,
		12, 13, 14, 15,
	};

	GLuint vbo_vertices, vbo_indices;
	glGenBuffers(1, &vbo_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glGenBuffers(1, &vbo_indices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glm::mat4 Projection = glm::perspective(45.0f, (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f); 
	glm::mat4 View       = glm::mat4(1.0f);
	glm::mat4 Model      = glm::mat4(1.0f);
	glm::mat4 M          = glm::mat4(1.0f);

	// rotation angles
	float alpha = 0.0f, beta = 0.0f, gamma = 0.0f;
	float yaw = 0.0f, pitch = 0.0f, x = 0.0f, y = 0.0f, z = 0.0f;

	while(active) {
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				active = false;
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case SDLK_g:
					grab = true;
					break;
				case SDLK_v:
					video ^= true; elapsed1 = 0.0;
					break;
				case SDLK_n:
					normal ^= true;
					break;
				case SDLK_f:
					fullscreen ^= true;
					screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, (fullscreen ? SDL_FULLSCREEN : 0) | SDL_HWSURFACE | SDL_OPENGL);
					break;
				}
				break;
			}
		}

		// time elapsed since last frame
		elapsed0 = t0.elapsed(true);

		// update frame based on input state
		if (kb.getKeyState(KEY_UP))    alpha += 180.0f*elapsed0;
		if (kb.getKeyState(KEY_DOWN))  alpha -= 180.0f*elapsed0;
		if (kb.getKeyState(KEY_LEFT))  beta  -= 180.0f*elapsed0;
		if (kb.getKeyState(KEY_RIGHT)) beta  += 180.0f*elapsed0;
		jp[0] = js.joystickPosition(0);

		jp[1]  = js.joystickPosition(1);
		yaw   += jp[1].x*elapsed0*90;
		pitch += jp[1].y*elapsed0*90;

		x     -= cos(-yaw*M_PI/180.0f)*jp[0].x*elapsed0*30 - sin(-yaw*M_PI/180.0f)*jp[0].y*elapsed0*30;
		z     += cos(-yaw*M_PI/180.0f)*jp[0].y*elapsed0*30 + sin(-yaw*M_PI/180.0f)*jp[0].x*elapsed0*30;
		//alpha += jp[0].y*elapsed0*180.0f;
		//beta  += jp[0].x*elapsed0*180.0f;
		gamma += 45.0f*elapsed0;
		//jp[1] = js.joystickPosition(1); 

		// rendering
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		View  = glm::mat4(1.0f);
		View  = glm::rotate(View, pitch, glm::vec3(-1.0f, 0.0f, 0.0f));
		View  = glm::rotate(View, yaw,   glm::vec3(0.0f, 1.0f, 0.0f));
		View  = glm::translate(View, glm::vec3(x, 0.0f, z));
		//View  = glm::translate(View, glm::vec3(-beta*0.01f, 0.0f, alpha*0.01f));
		//std::cout << alpha << std::endl;
		//View  = glm::rotate(View, beta, glm::vec3(0.0f, 1.0f, 0.0f));

		// render teapot
		Model = glm::mat4(1.0f);
		Model = glm::translate(Model, glm::vec3(0.0f,0.0f,-10.0f));
		Model = glm::rotate(Model, gamma*.667f, glm::vec3(0.0f,1.0f,0.0f));
		Model = glm::rotate(Model, gamma*.667f, glm::vec3(1.0f,0.0f,0.0f));
		Model = glm::rotate(Model, gamma*.667f, glm::vec3(0.0f,0.0f,1.0f));
		Model = glm::translate(Model, glm::vec3(0.0f,-0.5f,0.0f));
		glm::vec3 light_position = glm::vec3(0.0f, 100.0f, 100.0f);
		glUseProgram(glProgram1);
		glUniform3f(light_position1, light_position.x, light_position.y, light_position.z);
		glUniformMatrix4fv(Projection1, 1, GL_FALSE, glm::value_ptr(Projection));
		glUniformMatrix4fv(View1,       1, GL_FALSE, glm::value_ptr(View));
		glUniformMatrix4fv(Model1,      1, GL_FALSE, glm::value_ptr(Model));
		obj.render(vertex1, normal1);




		// render cube
		Model = glm::mat4(1.0f);
		Model = glm::translate(Model, glm::vec3(0.0f,0.0f,0.0f));
		light_position = glm::vec3(100.0f, 0.0f, 100.0f);

		glUseProgram(glProgram2);
		glUniform3f(light_position2, light_position.x, light_position.y, light_position.z);
		glUniformMatrix4fv(Projection2, 1, GL_FALSE, glm::value_ptr(Projection));
		glUniformMatrix4fv(View2,       1, GL_FALSE, glm::value_ptr(View));
		glUniformMatrix4fv(Model2,      1, GL_FALSE, glm::value_ptr(Model));
		glUniform1i(flag2, (int)normal);
		glUniform1i(texture_sample2, 0);
		glUniform1i(normal_sample2,  1);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture2);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, texture2normal);

		glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
		glEnableVertexAttribArray(vertex2);
		glVertexAttribPointer(vertex2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_), 0);
		glEnableVertexAttribArray(normal2);
		glVertexAttribPointer(normal2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_), (char *)NULL + 12);
		glEnableVertexAttribArray(texcoord2);
		glVertexAttribPointer(texcoord2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_), (char *)NULL + 24);
		glEnableVertexAttribArray(_tangent2);
		glVertexAttribPointer(_tangent2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_), (char *)NULL + 36);
		glEnableVertexAttribArray(_bitangent2);
		glVertexAttribPointer(_bitangent2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_), (char *)NULL + 48);
		glEnableVertexAttribArray(_normal2);
		glVertexAttribPointer(_normal2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_), (char *)NULL + 60);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indices);

		glDrawElements(GL_QUADS, 16, GL_UNSIGNED_INT, 0);
		Model = glm::translate(Model, glm::vec3(2.0f, 0.0f, 0.0f));
		glUniformMatrix4fv(Model2,      1, GL_FALSE, glm::value_ptr(Model));
		glDrawElements(GL_QUADS, 16, GL_UNSIGNED_INT, 0);
		Model = glm::translate(Model, glm::vec3(2.0f, 0.0f, 0.0f));
		glUniformMatrix4fv(Model2,      1, GL_FALSE, glm::value_ptr(Model));
		glDrawElements(GL_QUADS, 16, GL_UNSIGNED_INT, 0);
		Model = glm::translate(Model, glm::vec3(0.0f, 0.0f, -2.0f));
		glUniformMatrix4fv(Model2,      1, GL_FALSE, glm::value_ptr(Model));
		glDrawElements(GL_QUADS, 16, GL_UNSIGNED_INT, 0);
		Model = glm::translate(Model, glm::vec3(0.0f, 0.0f, -2.0f));
		glUniformMatrix4fv(Model2,      1, GL_FALSE, glm::value_ptr(Model));
		glDrawElements(GL_QUADS, 16, GL_UNSIGNED_INT, 0);
		Model = glm::translate(Model, glm::vec3(0.0f, 0.0f, -2.0f));
		glUniformMatrix4fv(Model2,      1, GL_FALSE, glm::value_ptr(Model));
		glDrawElements(GL_QUADS, 16, GL_UNSIGNED_INT, 0);





		// render skybox
		Model = glm::scale(glm::mat4(1.0f), glm::vec3(500,500,500));
		//View = glm::mat4(1.0f);
		//View = glm::rotate(View, beta, glm::vec3(0.0f, 1.0f, 0.0f));
		View = glm::translate(View, glm::vec3(-x, 0.0f, -z));
		M = Projection * View * Model;
		glUseProgram(glProgram);
		glUniformMatrix4fv(PVM, 1, GL_FALSE, glm::value_ptr(M));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_vertices);
		glEnableVertexAttribArray(vertex);
		glVertexAttribPointer(vertex, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_cube_indices);
		glDrawElements(GL_QUADS, sizeof(cube_indices)/sizeof(GLushort), GL_UNSIGNED_SHORT, 0);

		SDL_GL_SwapBuffers();

		if (grab) {
			grab = false;
			saveTGA(buffer, WIDTH, HEIGHT);
		}

		if (video) {
			elapsed1 += t1.elapsed(true);
			if (elapsed1 >= 1/24.0) {
				saveTGA(buffer, WIDTH, HEIGHT, true);
				elapsed1 = 0.0;
			}
		}
	}

	// release vertex and index buffer object
	glDeleteBuffers(1, &vbo_indices);
	glDeleteBuffers(1, &vbo_vertices);

	obj.releaseBufferObjects();
	
	// release vertex and index buffer object
	glDeleteBuffers(1, &ibo_cube_indices);
	glDeleteBuffers(1, &vbo_cube_vertices);

	// release textures
	deleteTexture(texture2);
	deleteTexture(texture2normal);
	
	// release cube map
	deleteCubeMap(cubemap_texture);

	// detach shaders from program and release
	releaseProgram(glProgram,  glShaderV,  glShaderF);
	releaseProgram(glProgram1, glShaderV1, glShaderF1);
	releaseProgram(glProgram2, glShaderV2, glShaderF2);

	SDL_Quit();

	delete[] buffer;

	return 0;
}