#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include "third-party/tinyply/source/tinyply.h"
#include "build/third-party/glad/include/glad/glad.h"
#include "third-party/glfw/include/GLFW/glfw3.h"

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}
void error_callback(int code, const char* description)
{
  std::cerr << "GLFW CODE: " << code << std::endl;
  std::cerr << description << std::endl;
}

class RenderWindow
{
public:
  RenderWindow()
    : pointStride(6), windowWidth(1024), windowHeight(1024), failState(false)
  {
    window = setupWindow();
    if (window)
    {
      setupShaders();
    }
  }
  ~RenderWindow()
  {
    glDeleteProgram(pointProgram);
    glDeleteShader(pointVertShader);
    glDeleteShader(pointFragShader);
    glfwDestroyWindow(window);
    glfwTerminate();
  }
  void load(std::vector<float> PLYData)
  {

  }
  bool render()
  {
    if (!window || glfwWindowShouldClose(window))
    {
      return false;
    }
    if (failState)
    {
      return false;
    }

    glfwPollEvents();
    return true;
  }
private:
  bool assignShaderUniform(GLuint programID, GLint& locID, const GLchar* locName)
  {
    locID = glGetUniformLocation(programID, locName);
    if (locID < 0)
    {
      failState = true;
      std::cerr << "COULD NOT ASSIGN UNIFORM " << locName << std::endl;
    }
    return locID >= 0;
  }
  bool checkShaderCompile(GLuint shaderID)
  {
    GLint isCompiled = 0;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
      GLint maxLength = 0;
      glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &maxLength);
      std::string errorLog;
      errorLog.resize(maxLength);
      glGetShaderInfoLog(shaderID, maxLength, &maxLength, &errorLog[0]);
      std::cerr << errorLog << std::endl;
      failState = true;
    }
    return isCompiled != GL_FALSE;
  }
  
  void setupShaders()
  {
    /* Point Vertex Shader */
    {
      const GLchar* pointVertText = R"foo(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)foo";
      pointVertShader = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(pointVertShader, 1, &pointVertText, 0);
      glCompileShader(pointVertShader);
      if (!checkShaderCompile(pointVertShader))
      {
        return;
      }
    }
    
    /* Point Fragment Shader*/
    {
      const char* pointFragText = R"foo(
#version 330 core
out vec4 FragColor;

in vec3 Normal;  
in vec3 FragPos;  
  
uniform vec3 lightPos; 
uniform vec3 viewPos;

void main()
{
    vec3 lightColor = vec3(.1,.1,.1);
    vec3 objectColor = vec3(.9,.9,.9);
    // ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;  
        
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
} 
)foo";
      pointFragShader = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(pointFragShader, 1, &pointFragText, 0);
      glCompileShader(pointFragShader);
      if (!checkShaderCompile(pointFragShader))
      {
        return;
      }
    }

    /* Point Program */
    {
      pointProgram = glCreateProgram();
      glAttachShader(pointProgram, pointVertShader);
      glAttachShader(pointProgram, pointFragShader);
      glLinkProgram(pointProgram);
      glUseProgram(pointProgram);
      if (!assignShaderUniform(pointProgram, pointModelLoc, "model"))
      {
        return;
      }
      if (!assignShaderUniform(pointProgram, pointViewLoc, "view"))
      {
        return;
      }
      if (!assignShaderUniform(pointProgram, pointProjectionLoc, "projection"))
      {
        return;
      }
      if (!assignShaderUniform(pointProgram, pointLightPosLoc, "lightPos"))
      {
        return;
      }
      if (!assignShaderUniform(pointProgram, pointViewPosLoc, "viewPos"))
      {
        return;
      }
    }
  }
  GLFWwindow* setupWindow()
  {
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
    {
      return nullptr;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Rosenthal-Linsen-Lars-2008", NULL, NULL);
    glfwSetKeyCallback(window, key_callback);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(1);
    return window;
  }
  bool failState;
  GLFWwindow* window;
  int pointStride;
  int windowHeight;
  int windowWidth;
  GLuint pointVertShader;
  GLuint pointFragShader;
  GLuint pointProgram;
  GLint pointModelLoc;
  GLint pointViewLoc;
  GLint pointProjectionLoc;
  GLint pointLightPosLoc;
  GLint pointViewPosLoc;
};

std::vector<float> readPLY(std::filesystem::path const& PLYpath)
{
  
  if (!std::filesystem::exists(PLYpath))
  {
    throw std::invalid_argument(PLYpath.string() + " does not exist");
  }
  std::ifstream ss(PLYpath, std::ios::binary);
  if (ss.fail())
  {
    throw std::runtime_error(PLYpath.string() + " failed to open");
  }
  tinyply::PlyFile file;
  file.parse_header(ss);
  auto vertices = file.request_properties_from_element("vertex", { "x", "y", "z", "nx", "ny", "nz" });
  file.read(ss);
  if (!vertices)
  {
    throw std::invalid_argument(PLYpath.string() + " is missing elements required");
  }
  const size_t numVerticesBytes = vertices->buffer.size_bytes();
  std::vector<float> PLYdata(6*vertices->count);
  std::memcpy(PLYdata.data(), vertices->buffer.get(), numVerticesBytes);
  return PLYdata;
}

void displayHelp()
{
  std::cout << "Usage: Rosenthal-Linsen-Lars-2008 \"PLY PATH\"" << std::endl;
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    displayHelp();
    return 1;
  }
  std::vector<float> PLYdata;
  try
  {
    PLYdata = readPLY(argv[1]);
  }
  catch (std::invalid_argument const& e)
  {
    std::cout << "BAD PLY FILE" << std::endl;
    std::cerr << e.what() << std::endl;
    return 2;
  }
  catch (std::runtime_error const& e)
  {
    std::cout << "ENCOUNTERED ERROR READING PLY FILE " << std::endl;
    std::cerr << e.what() << std::endl;
    return 3;
  }
  catch (std::exception const& e)
  {
    std::cout << "ENCOUNTERED ERROR READING PLY FILE " << std::endl;
    std::cerr << e.what() << std::endl;
    return 4;
  }
  RenderWindow viewWindow;
  viewWindow.load(std::move(PLYdata));
  while (viewWindow.render()){}
  return 0;
}