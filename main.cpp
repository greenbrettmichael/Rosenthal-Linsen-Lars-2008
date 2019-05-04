#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include "third-party/tinyply/source/tinyply.h"
#include "build/third-party/glad/include/glad/glad.h"
#include "third-party/glfw/include/GLFW/glfw3.h"
#include "third-party/glm/glm/glm.hpp"
#include "third-party/glm/glm/gtc/matrix_transform.hpp"

bool firstMouse = true;
int windowHeight = 800;
int windowWidth = 640;
float fov = 45.f;
float pitch = 0.f;
float yaw = 0.f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
glm::vec3 viewPos = glm::vec3(0.f, 0.f, 0.f);
glm::vec3 cameraFront = glm::vec3(0.f, 0.f, -1.f);
glm::vec3 cameraUp = glm::vec3(0.f, 1.f, 0.f);

void processInput(GLFWwindow* window)
{
  if (glfwGetKey(window,GLFW_KEY_ESCAPE) == GLFW_PRESS)
  {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
  float cameraSpeed = 2.5f * deltaTime;
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
  {
    viewPos += cameraSpeed * cameraFront;
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
  {
    viewPos -= cameraSpeed * cameraFront;
  }
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
  {
    viewPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
  }
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
  {
    viewPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
  }
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
  firstMouse = true;
  windowWidth = width;
  windowHeight = height;
  glViewport(0, 0, width, height);
}
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
  static double lastX;
  static double lastY;
  if (firstMouse)
  {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }
  float xoffset = (float)(xpos - lastX);
  float yoffset = (float)(lastY - ypos);
  lastX = xpos;
  lastY = ypos;
  float sensitivity = 0.1f;
  xoffset *= sensitivity;
  yoffset *= sensitivity;
  yaw += xoffset;
  pitch += yoffset;
  pitch = std::min(pitch, 89.f);
  pitch = std::max(pitch, -89.f);
  glm::vec3 front;
  front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
  front.y = std::sin(glm::radians(pitch));
  front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
  cameraFront = glm::normalize(front);
}
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  fov -= (float)yoffset;
  fov = std::min(fov, 45.f);
  fov = std::max(fov, 1.f);
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
    : failState(false),
    pointStride(6),
    pointCount(0),
    model(glm::mat4(1.0)),
    view(glm::mat4(1.0)),
    projection(glm::mat4(1.0)),
    lightPos(glm::vec3(0.0,2.0,0.0))
  {
    window = setupWindow();
    if (window)
    {
      setupShaders();
    }
  }
  ~RenderWindow()
  {
    glDeleteVertexArrays(1, &pointVAO);
    glDeleteBuffers(1, &pointVBO);
    glDeleteProgram(pointProgram);
    glDeleteShader(pointVertShader);
    glDeleteShader(pointFragShader);
    glfwDestroyWindow(window);
    glfwTerminate();
  }
  void load(std::vector<float> PLYData)
  {
    pointCount = (int)PLYData.size() / pointStride;
    glGenVertexArrays(1, &pointVAO);
    glBindVertexArray(pointVAO);
    glGenBuffers(1, &pointVBO);
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * PLYData.size(), PLYData.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * pointStride, (GLvoid*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * pointStride, (GLvoid*)(sizeof(float) * 3));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
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
    processCamera();
    glClearColor(1.f, 1.f, 1.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(pointProgram);
    glBindVertexArray(pointVAO);
    glDrawArrays(GL_POINTS, 0, pointCount);
    glBindVertexArray(0);
    glfwSwapBuffers(window);
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
  void processCamera()
  {
    float currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    processInput(window);
    view = glm::lookAt(viewPos, viewPos + cameraFront, cameraUp);
    projection = glm::perspective(glm::radians(fov), (float)windowWidth / (float)windowHeight, .1f, 100.f);
    glUseProgram(pointProgram);
    glUniformMatrix4fv(pointModelLoc, 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(pointViewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(pointProjectionLoc, 1, GL_FALSE, &projection[0][0]);
    glUniform3fv(pointLightPosLoc, 1, &viewPos[0]);
    glUniform3fv(pointViewPosLoc, 1, &viewPos[0]);
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
    if(dot(lightDir,norm) < 0.0) discard;
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
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(1);
    return window;
  }
  bool failState;
  GLFWwindow* window;
  int pointStride;
  int pointCount;
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
  glm::vec3 lightPos;
  GLuint pointVAO;
  GLuint pointVBO;
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