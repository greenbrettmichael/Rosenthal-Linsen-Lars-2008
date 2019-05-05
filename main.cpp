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
int backgroundFillIters = 1;
int occlusionFillIters = 1;
int pointStride = 6;
int windowHeight = 512;
int windowWidth = 512;
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
    pointCount(0),
    currBuffer(0),
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
    glDeleteFramebuffers(2, &gBuffer[0]);
    glDeleteTextures(2, &positionTexture[0]);
    glDeleteTextures(2, &normalTexture[0]);
    glDeleteTextures(2, &colorTexture[0]);
    glDeleteRenderbuffers(3, &depthRenderBuffer[0]);
    glDeleteVertexArrays(1, &fboVAO);
    glDeleteBuffers(1, &fboVBO);
    glDeleteProgram(backgroundProgram);
    glDeleteShader(backgroundVertShader);
    glDeleteShader(backgroundFragShader);
    glDeleteProgram(occlusionProgram);
    glDeleteShader(occlusionVertShader);
    glDeleteShader(occlusionFragShader);
    glDeleteProgram(smoothProgram);
    glDeleteShader(smoothVertShader);
    glDeleteShader(smoothFragShader);
    glDeleteProgram(aaHighProgram);
    glDeleteShader(aaVertShader);
    glDeleteShader(aaHighProgram);
    glDeleteProgram(aaLowProgram);
    glDeleteShader(aaFragLowShader);
    glDeleteProgram(illustrateProgram);
    glDeleteShader(illustrateVertShader);
    glDeleteShader(illustrateFragShader);
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
    if (pointStride > 6)
    {
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * pointStride, (GLvoid*)(sizeof(float) * 6));
    }
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
    glClearColor(1.f, 1.f, 1.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    processCamera();
    illuminatePoints();
    for (int i = 0; i < backgroundFillIters; ++i)
    {
      fillBackground();
    }
    for (int i = 0; i < occlusionFillIters; ++i)
    {
      fillOcclusion();
    }
    smooth();
    aliasing();
    illustrateEffect();
    glfwSwapBuffers(window);
    glfwPollEvents();
    return true;
  }
private:
  void aliasing()
  {
    int nextBuffer = currBuffer ^ 1;
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer[nextBuffer]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(aaHighProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, colorTexture[currBuffer]);
    glBindVertexArray(fboVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(aaLowProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, colorTexture[currBuffer]);
    glBindVertexArray(fboVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    currBuffer = nextBuffer;
  }
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
  bool checkShaderCompile(GLuint shaderID, const char* shaderName)
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
      std::cout << shaderName << " FAILED TO COMPILE" << std::endl;
      std::cerr << errorLog << std::endl;
      failState = true;
    }
    return isCompiled != GL_FALSE;
  } 
  void fillBackground()
  {
    int nextBuffer = currBuffer ^ 1;
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer[nextBuffer]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(backgroundProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, colorTexture[currBuffer]);
    glBindVertexArray(fboVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    currBuffer = nextBuffer;
  }
  void fillOcclusion()
  {
    int nextBuffer = currBuffer ^ 1;
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer[nextBuffer]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(occlusionProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, colorTexture[currBuffer]);
    glBindVertexArray(fboVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    currBuffer = nextBuffer;
  }
  void illuminatePoints()
  {
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer[currBuffer]);
    glClearColor(0.f, 0.f, 0.f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(pointProgram);
    glUniformMatrix4fv(pointModelLoc, 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(pointViewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(pointProjectionLoc, 1, GL_FALSE, &projection[0][0]);
    glUniform3fv(pointLightPosLoc, 1, &viewPos[0]);
    glUniform3fv(pointViewPosLoc, 1, &viewPos[0]);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(pointProgram);
    glBindVertexArray(pointVAO);
    glDrawArrays(GL_POINTS, 0, pointCount);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  void illustrateEffect()
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(illustrateProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, colorTexture[currBuffer]);
    glBindVertexArray(fboVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
  }
  void processCamera()
  {
    float currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    processInput(window);
    view = glm::lookAt(viewPos, viewPos + cameraFront, cameraUp);
    projection = glm::perspective(glm::radians(fov), (float)windowWidth / (float)windowHeight, .01f, 100.f);
  }
  void setupShaders()
  {
    /* Processing Buffers */
    {
      glGenFramebuffers(2, &gBuffer[0]);
      glGenTextures(2, &positionTexture[0]);
      glGenTextures(2, &normalTexture[0]);
      glGenTextures(2, &colorTexture[0]);
      glGenRenderbuffers(2, &depthRenderBuffer[0]);
      for (int i = 0; i < 2; ++i)
      {
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer[i]);
        glBindTexture(GL_TEXTURE_2D, positionTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, windowWidth, windowHeight, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, positionTexture[i], 0);
        glBindTexture(GL_TEXTURE_2D, normalTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, windowWidth, windowHeight, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTexture[i], 0);
        glBindTexture(GL_TEXTURE_2D, colorTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, windowWidth, windowHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, colorTexture[i], 0);
        GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
        glDrawBuffers(3, attachments);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRenderBuffer[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, windowWidth, windowHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderBuffer[i]);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
          std::cerr << "PROCESSING BUFFER COULD NOT BE CREATED" << std::endl;
          failState = true;
          return;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }
      glGenVertexArrays(1, &fboVAO);
      glBindVertexArray(fboVAO);
      glGenBuffers(1, &fboVBO);
      glBindBuffer(GL_ARRAY_BUFFER, fboVBO);
      float const quadVertices[] = { -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

        -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f };
      glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
      glBindVertexArray(0);
    }

    /* Point Vertex Shader */
    {
      const GLchar* pointVertText = pointStride > 6 ? R"foo(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

out vec3 FragPos;
out vec3 Normal;
out vec3 Color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    Color = aColor;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)foo" : R"foo(
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
      if (!checkShaderCompile(pointVertShader, "ILLUMINATE VERTEX"))
      {
        return;
      }
    }
    
    /* Point Fragment Shader*/
    {
      const char* pointFragText = pointStride > 6 ? R"foo(
#version 330 core

in vec3 Normal;  
in vec3 FragPos;
in vec3 Color;

layout (location = 0) out vec4 positionTexture;
layout (location = 1) out vec3 normalTexture;
layout (location = 2) out vec4 colorTexture;
uniform vec3 lightPos; 
uniform vec3 viewPos;

void main()
{
    vec3 lightColor = vec3(.1,.1,.1);
    vec3 objectColor = Color;
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
    
    float zFar = 100.0;  
    normalTexture = Normal;
    colorTexture.rgba = vec4((ambient + diffuse + specular) * objectColor, 1.0);
    positionTexture.xyz = FragPos;
    positionTexture.a =  distance(FragPos, viewPos) / zFar;
} 
)foo" : R"foo(
#version 330 core

in vec3 Normal;  
in vec3 FragPos;  

layout (location = 0) out vec4 positionTexture;
layout (location = 1) out vec3 normalTexture;
layout (location = 2) out vec4 colorTexture;
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
    
    float zFar = 100.0;
    normalTexture = Normal;
    colorTexture.rgba = vec4((ambient + diffuse + specular) * objectColor, 1.0);
    positionTexture.xyz = FragPos;
    positionTexture.a =  distance(FragPos, viewPos) / zFar;
} 
)foo";
      pointFragShader = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(pointFragShader, 1, &pointFragText, 0);
      glCompileShader(pointFragShader);
      if (!checkShaderCompile(pointFragShader, "ILLUMINATE FRAGMENT"))
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

    /* Background Pixel Vertex Shader */
    {
      const GLchar* backgroundVertText = R"foo(
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
  TexCoords = aTexCoords;
  gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
}
)foo";
      backgroundVertShader = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(backgroundVertShader, 1, &backgroundVertText, 0);
      glCompileShader(backgroundVertShader);
      if (!checkShaderCompile(backgroundVertShader, "BACKGROUND FILL VERTEX"))
      {
        return;
      }
    }

    /* Background Pixel Fragment Shader */
    {
      const char* backgroundFragText = R"foo(
#version 420

in vec2 TexCoords;

layout (location = 0) out vec4 positionTextureOut;
layout (location = 1) out vec3 normalTextureOut;
layout (location = 2) out vec4 colorTextureOut;
layout(binding=0) uniform sampler2D positionTextureIn;
layout(binding=1) uniform sampler2D normalTextureIn;
layout(binding=2) uniform sampler2D colorTextureIn;
const float zeroTol = 1e-6;

void main()
{
ivec2 texSize = textureSize(positionTextureIn, 0);
vec2 stepSize = 1.0/vec2(float(texSize.x), float(texSize.y));
vec2 offsets[9] = vec2[](
        vec2(-stepSize.x,  stepSize.y), // top-left
        vec2( 0.0f,    stepSize.y), // top-center
        vec2( stepSize.x,  stepSize.y), // top-right
        vec2(-stepSize.x,  0.0f),   // center-left
        vec2( 0.0f,    0.0f),   // center-center
        vec2( stepSize.x,  0.0f),   // center-right
        vec2(-stepSize.x, -stepSize.y), // bottom-left
        vec2( 0.0f,   -stepSize.y), // bottom-center
        vec2( stepSize.x, -stepSize.y)  // bottom-right    
    );
float sampleTex[9];
    for(int i = 0; i < 9; i++)
        sampleTex[i] = texture(positionTextureIn, TexCoords.st + offsets[i]).a;
if(abs(sampleTex[4]) > zeroTol)
{
  positionTextureOut = texture(positionTextureIn, TexCoords.st);
  normalTextureOut = texture(normalTextureIn, TexCoords.st).xyz;
  colorTextureOut = texture(colorTextureIn, TexCoords.st);
}
else
{
float kernel1[9] = float[](
        0, 1, 1,
        0, 1, 1,
        0, 1, 1
    );
  float sum1 = 0;
for(int i = 0; i < 9; i++)
        sum1 += sampleTex[i] * kernel1[i];
float kernel2[9] = float[](
        1, 1, 1,
        1, 1, 1,
        0, 0, 0
    );
  float sum2 = 0;
for(int i = 0; i < 9; i++)
        sum2 += sampleTex[i] * kernel2[i];
float kernel3[9] = float[](
        1, 1, 0,
        1, 1, 0,
        1, 1, 0
    );
  float sum3 = 0;
for(int i = 0; i < 9; i++)
        sum3 += sampleTex[i] * kernel3[i];
float kernel4[9] = float[](
        0, 0, 0,
        1, 1, 1,
        1, 1, 1
    );
  float sum4 = 0;
for(int i = 0; i < 9; i++)
        sum4 += sampleTex[i] * kernel4[i];
float kernel5[9] = float[](
        1, 1, 1,
        0, 1, 1,
        0, 0, 1
    );
  float sum5 = 0;
for(int i = 0; i < 9; i++)
        sum5 += sampleTex[i] * kernel5[i];
float kernel6[9] = float[](
        1, 1, 1,
        1, 1, 0,
        1, 0, 0
    );
  float sum6 = 0;
for(int i = 0; i < 9; i++)
        sum6 += sampleTex[i] * kernel6[i];
float kernel7[9] = float[](
        1, 0, 0,
        1, 1, 0,
        1, 1, 1
    );
  float sum7 = 0;
for(int i = 0; i < 9; i++)
        sum7 += sampleTex[i] * kernel7[i];
float kernel8[9] = float[](
        0, 0, 1,
        0, 1, 1,
        1, 1, 1
    );
  float sum8 = 0;
for(int i = 0; i < 9; i++)
        sum8 += sampleTex[i] * kernel8[i];
  float testProd = sum1*sum2*sum3*sum4*sum5*sum6*sum7*sum8;
if(abs(testProd) < zeroTol) discard;
  float smallestDepth = 100000.0;
  int smallestInd = 4;
  for(int i = 0; i < 9; i++)
  {
     if(abs(sampleTex[i]) > zeroTol && abs(sampleTex[i]) < smallestDepth)
     {
       smallestDepth = abs(sampleTex[i]);
       smallestInd = i;
     }
  }
  positionTextureOut = texture(positionTextureIn, TexCoords.st + offsets[smallestInd]);
  normalTextureOut = texture(normalTextureIn, TexCoords.st + offsets[smallestInd]).xyz;
  colorTextureOut = texture(colorTextureIn, TexCoords.st + offsets[smallestInd]);
}
} 
)foo";
      backgroundFragShader = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(backgroundFragShader, 1, &backgroundFragText, 0);
      glCompileShader(backgroundFragShader);
      if (!checkShaderCompile(backgroundFragShader, "BACKGROUND FILL FRAGMENT"))
      {
        return;
      }
    }

    /* Background Pixel Program */
    {
      backgroundProgram = glCreateProgram();
      glAttachShader(backgroundProgram, backgroundVertShader);
      glAttachShader(backgroundProgram, backgroundFragShader);
      glLinkProgram(backgroundProgram);
      glUseProgram(backgroundProgram);
    }

    /* Occlusion Pixel Vertex Shader */
    {
      const GLchar* occlusionVertText = R"foo(
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
  TexCoords = aTexCoords;
  gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
}
)foo";
      occlusionVertShader = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(occlusionVertShader, 1, &occlusionVertText, 0);
      glCompileShader(occlusionVertShader);
      if (!checkShaderCompile(occlusionVertShader, "OCCLUSION FILL VERTEX"))
      {
        return;
      }
    }

    /* Occlusion Pixel Fragment Shader */
    {
      const char* occlusionFragText = R"foo(
#version 420

in vec2 TexCoords;

layout (location = 0) out vec4 positionTextureOut;
layout (location = 1) out vec3 normalTextureOut;
layout (location = 2) out vec4 colorTextureOut;
layout(binding=0) uniform sampler2D positionTextureIn;
layout(binding=1) uniform sampler2D normalTextureIn;
layout(binding=2) uniform sampler2D colorTextureIn;
const float zeroTol = 1e-6;

void main()
{
ivec2 texSize = textureSize(positionTextureIn, 0);
vec2 stepSize = 1.0/vec2(float(texSize.x), float(texSize.y));
vec2 offsets[9] = vec2[](
        vec2(-stepSize.x,  stepSize.y), // top-left
        vec2( 0.0f,    stepSize.y), // top-center
        vec2( stepSize.x,  stepSize.y), // top-right
        vec2(-stepSize.x,  0.0f),   // center-left
        vec2( 0.0f,    0.0f),   // center-center
        vec2( stepSize.x,  0.0f),   // center-right
        vec2(-stepSize.x, -stepSize.y), // bottom-left
        vec2( 0.0f,   -stepSize.y), // bottom-center
        vec2( stepSize.x, -stepSize.y)  // bottom-right    
    );
float sampleTex[9];
    for(int i = 0; i < 9; i++)
        sampleTex[i] = texture(positionTextureIn, TexCoords.st + offsets[i]).a;
if(abs(sampleTex[4]) < zeroTol)
{
  positionTextureOut = texture(positionTextureIn, TexCoords.st);
  normalTextureOut = texture(normalTextureIn, TexCoords.st).xyz;
  colorTextureOut = texture(colorTextureIn, TexCoords.st);
}
else
{
float kernel1[9] = float[](
        0, 1, 1,
        0, 1, 1,
        0, 1, 1
    );
  float sum1 = 0;
for(int i = 0; i < 9; i++)
        sum1 += step(sampleTex[i], sampleTex[4]) * kernel1[i];
float kernel2[9] = float[](
        1, 1, 1,
        1, 1, 1,
        0, 0, 0
    );
  float sum2 = 0;
for(int i = 0; i < 9; i++)
        sum2 += step(sampleTex[i], sampleTex[4]) * kernel2[i];
float kernel3[9] = float[](
        1, 1, 0,
        1, 1, 0,
        1, 1, 0
    );
  float sum3 = 0;
for(int i = 0; i < 9; i++)
        sum3 += step(sampleTex[i], sampleTex[4]) * kernel3[i];
float kernel4[9] = float[](
        0, 0, 0,
        1, 1, 1,
        1, 1, 1
    );
  float sum4 = 0;
for(int i = 0; i < 9; i++)
        sum4 += step(sampleTex[i], sampleTex[4]) * kernel4[i];
float kernel5[9] = float[](
        1, 1, 1,
        0, 1, 1,
        0, 0, 1
    );
  float sum5 = 0;
for(int i = 0; i < 9; i++)
        sum5 += step(sampleTex[i], sampleTex[4]) * kernel5[i];
float kernel6[9] = float[](
        1, 1, 1,
        1, 1, 0,
        1, 0, 0
    );
  float sum6 = 0;
for(int i = 0; i < 9; i++)
        sum6 += step(sampleTex[i], sampleTex[4]) * kernel6[i];
float kernel7[9] = float[](
        1, 0, 0,
        1, 1, 0,
        1, 1, 1
    );
  float sum7 = 0;
for(int i = 0; i < 9; i++)
        sum7 += step(sampleTex[i], sampleTex[4]) * kernel7[i];
float kernel8[9] = float[](
        0, 0, 1,
        0, 1, 1,
        1, 1, 1
    );
  float sum8 = 0;
for(int i = 0; i < 9; i++)
        sum8 += step(sampleTex[i], sampleTex[4]) * kernel8[i];
  float testProd = sum1*sum2*sum3*sum4*sum5*sum6*sum7*sum8;
  if(abs(testProd) < zeroTol) 
{
  positionTextureOut = texture(positionTextureIn, TexCoords.st);
  normalTextureOut = texture(normalTextureIn, TexCoords.st).xyz;
  colorTextureOut = texture(colorTextureIn, TexCoords.st);
}
else
{
  float smallestDepth = 100000.0;
  int smallestInd = 4;
  for(int i = 0; i < 9; i++)
  {
     float depthDiff = (sampleTex[4] - sampleTex[i]);
     if(depthDiff > zeroTol && depthDiff < smallestDepth)
     {
       smallestDepth = depthDiff;
       smallestInd = i;
     }
  }
  positionTextureOut = texture(positionTextureIn, TexCoords.st + offsets[smallestInd]);
  normalTextureOut = texture(normalTextureIn, TexCoords.st + offsets[smallestInd]).xyz;
  colorTextureOut = texture(colorTextureIn, TexCoords.st + offsets[smallestInd]);
}
}
} 
)foo";
      occlusionFragShader = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(occlusionFragShader, 1, &occlusionFragText, 0);
      glCompileShader(occlusionFragShader);
      if (!checkShaderCompile(occlusionFragShader, "OCCLUSION FILL FRAGMENT"))
      {
        return;
      }
    }

    /* Occlusion Pixel Program */
    {
      occlusionProgram = glCreateProgram();
      glAttachShader(occlusionProgram, occlusionVertShader);
      glAttachShader(occlusionProgram, occlusionFragShader);
      glLinkProgram(occlusionProgram);
      glUseProgram(occlusionProgram);
    }

    /* Smoothing Vertex Shader */
    {
      const GLchar* smoothVertText = R"foo(
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
  TexCoords = aTexCoords;
  gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
}
)foo";
      smoothVertShader = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(smoothVertShader, 1, &smoothVertText, 0);
      glCompileShader(smoothVertShader);
      if (!checkShaderCompile(smoothVertShader, "SMOOTHING VERTEX"))
      {
        return;
      }
    }

    /* Smoothing Fragment Shader */
    {
      const char* smoothFragText = R"foo(
#version 420

in vec2 TexCoords;

layout (location = 0) out vec4 positionTextureOut;
layout (location = 1) out vec3 normalTextureOut;
layout (location = 2) out vec4 colorTextureOut;
layout(binding=0) uniform sampler2D positionTextureIn;
layout(binding=1) uniform sampler2D normalTextureIn;
layout(binding=2) uniform sampler2D colorTextureIn;
const float zeroTol = 1e-6;

void main()
{
ivec2 texSize = textureSize(positionTextureIn, 0);
vec2 stepSize = 1.0/vec2(float(texSize.x), float(texSize.y));
vec2 offsets[9] = vec2[](
        vec2(-stepSize.x,  stepSize.y), // top-left
        vec2( 0.0f,    stepSize.y), // top-center
        vec2( stepSize.x,  stepSize.y), // top-right
        vec2(-stepSize.x,  0.0f),   // center-left
        vec2( 0.0f,    0.0f),   // center-center
        vec2( stepSize.x,  0.0f),   // center-right
        vec2(-stepSize.x, -stepSize.y), // bottom-left
        vec2( 0.0f,   -stepSize.y), // bottom-center
        vec2( stepSize.x, -stepSize.y)  // bottom-right    
    );
float sampleTex[9];
    for(int i = 0; i < 9; i++)
        sampleTex[i] = texture(positionTextureIn, TexCoords.st + offsets[i]).a;
if(abs(sampleTex[4]) < zeroTol)
{
  positionTextureOut = texture(positionTextureIn, TexCoords.st);
  normalTextureOut = texture(normalTextureIn, TexCoords.st).xyz;
  colorTextureOut = texture(colorTextureIn, TexCoords.st);
}
else
{
float alleviatedGaussian[9] = float[](
        1.0/16.0, 2.0/16.0, 1.0/16.0,
        2.0/16.0, 16.0/28.0, 2.0/16.0,
        1.0/16.0, 2.0/16.0, 1.0/16.0
    );
float totalWeight = 0.0;
for(int i = 0; i < 9; i++)
{
  alleviatedGaussian[i] *= step(zeroTol, sampleTex[i]);
  totalWeight += alleviatedGaussian[i];
}
for(int i = 0; i < 9; i++)
{
  positionTextureOut += (alleviatedGaussian[i] / totalWeight) * texture(positionTextureIn, TexCoords.st + offsets[i]);
  normalTextureOut += (alleviatedGaussian[i] / totalWeight) * texture(normalTextureIn, TexCoords.st + offsets[i]).xyz;
  colorTextureOut += (alleviatedGaussian[i] / totalWeight) * texture(colorTextureIn, TexCoords.st + offsets[i]);
}
}
}
)foo";
      smoothFragShader = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(smoothFragShader, 1, &smoothFragText, 0);
      glCompileShader(smoothFragShader);
      if (!checkShaderCompile(smoothFragShader, "SMOOTHING FRAGMENT"))
      {
        return;
      }
    }

    /* Smoothing Program */
    {
      smoothProgram = glCreateProgram();
      glAttachShader(smoothProgram, smoothVertShader);
      glAttachShader(smoothProgram, smoothFragShader);
      glLinkProgram(smoothProgram);
      glUseProgram(smoothProgram);
    }

    /* Anti-Aliasing Vertex Shader */
    {
      const GLchar* aaVertText = R"foo(
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
  TexCoords = aTexCoords;
  gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
}
)foo";
      aaVertShader = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(aaVertShader, 1, &aaVertText, 0);
      glCompileShader(aaVertShader);
      if (!checkShaderCompile(aaVertShader, "ANTI ALIASING VERTEX"))
      {
        return;
      }
    }

    /* Anti-Aliasing Fragment Shader */
    {
      const char* aaFragHighText = R"foo(
#version 420

in vec2 TexCoords;

layout (location = 0) out vec4 positionTextureOut;
layout (location = 1) out vec3 normalTextureOut;
layout (location = 2) out vec4 colorTextureOut;
layout(binding=0) uniform sampler2D positionTextureIn;
layout(binding=1) uniform sampler2D normalTextureIn;
layout(binding=2) uniform sampler2D colorTextureIn;

void main()
{
ivec2 texSize = textureSize(positionTextureIn, 0);
vec2 stepSize = 1.0/vec2(float(texSize.x), float(texSize.y));
vec2 offsets[9] = vec2[](
        vec2(-stepSize.x,  stepSize.y), // top-left
        vec2( 0.0f,    stepSize.y), // top-center
        vec2( stepSize.x,  stepSize.y), // top-right
        vec2(-stepSize.x,  0.0f),   // center-left
        vec2( 0.0f,    0.0f),   // center-center
        vec2( stepSize.x,  0.0f),   // center-right
        vec2(-stepSize.x, -stepSize.y), // bottom-left
        vec2( 0.0f,   -stepSize.y), // bottom-center
        vec2( stepSize.x, -stepSize.y)  // bottom-right    
    );
float laplaceFilter[9] = float[](
        0.0, -1.0, 0.0,
        -1.0, 4.0, -1.0,
        0.0, -1.0, 0.0
    );
for(int i = 0; i < 9; i++)
{
  positionTextureOut += laplaceFilter[i] * texture(positionTextureIn, TexCoords.st + offsets[i]);
  normalTextureOut += laplaceFilter[i] * texture(normalTextureIn, TexCoords.st + offsets[i]).xyz;
  colorTextureOut += laplaceFilter[i] * texture(colorTextureIn, TexCoords.st + offsets[i]);
}
}
)foo";
      aaFragHighShader = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(aaFragHighShader, 1, &aaFragHighText, 0);
      glCompileShader(aaFragHighShader);
      if (!checkShaderCompile(aaFragHighShader, "ANTI ALIASING HIGH-PASS FRAGMENT"))
      {
        return;
      }
      const char* aaFragLowText = R"foo(
#version 420

in vec2 TexCoords;

layout (location = 0) out vec4 positionTextureOut;
layout (location = 1) out vec3 normalTextureOut;
layout (location = 2) out vec4 colorTextureOut;
layout(binding=0) uniform sampler2D positionTextureIn;
layout(binding=1) uniform sampler2D normalTextureIn;
layout(binding=2) uniform sampler2D colorTextureIn;
const float zeroTol = 1e-6;

void main()
{
  float fragPosDepth = texture(positionTextureIn, TexCoords.st).a;
if(fragPosDepth < zeroTol) discard;
  positionTextureOut = texture(positionTextureIn, TexCoords.st);
  normalTextureOut = texture(normalTextureIn, TexCoords.st).xyz;
  colorTextureOut = texture(colorTextureIn, TexCoords.st);
} 
)foo";
      aaFragLowShader = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(aaFragLowShader, 1, &aaFragLowText, 0);
      glCompileShader(aaFragLowShader);
      if (!checkShaderCompile(aaFragLowShader, "ANTI ALIASING LOW-PASS FRAGMENT"))
      {
        return;
      }
    }

    /* Anti-Aliasing Program */
    {
      aaHighProgram = glCreateProgram();
      glAttachShader(aaHighProgram, aaVertShader);
      glAttachShader(aaHighProgram, aaFragHighShader);
      glLinkProgram(aaHighProgram);
      glUseProgram(aaHighProgram);
      aaLowProgram = glCreateProgram();
      glAttachShader(aaLowProgram, aaVertShader);
      glAttachShader(aaLowProgram, aaFragLowShader);
      glLinkProgram(aaLowProgram);
      glUseProgram(aaLowProgram);
    }

    /* Illustration Effect Vertex Shader */
    {
      const GLchar* illustrateVertText = R"foo(
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
  TexCoords = aTexCoords;
  gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
}
)foo";
      illustrateVertShader = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(illustrateVertShader, 1, &illustrateVertText, 0);
      glCompileShader(illustrateVertShader);
      if (!checkShaderCompile(illustrateVertShader, "ILLUSTRATE VERTEX"))
      {
        return;
      }
    }

    /* Illustration Effect Fragment Shader */
    {
      const char* illustrateFragText = R"foo(
#version 420

out vec4 FragColor;
in vec2 TexCoords;

layout(binding=0) uniform sampler2D positionTextureIn;
layout(binding=1) uniform sampler2D normalTextureIn;
layout(binding=2) uniform sampler2D colorTextureIn;

void main()
{
  FragColor = texture(colorTextureIn, TexCoords.st);
} 
)foo";
      illustrateFragShader = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(illustrateFragShader, 1, &illustrateFragText, 0);
      glCompileShader(illustrateFragShader);
      if (!checkShaderCompile(illustrateFragShader, "ILLUSTRATION FRAGMENT"))
      {
        return;
      }
    }

    /* Illustration Effect Program */
    {
      illustrateProgram = glCreateProgram();
      glAttachShader(illustrateProgram, illustrateVertShader);
      glAttachShader(illustrateProgram, illustrateFragShader);
      glLinkProgram(illustrateProgram);
      glUseProgram(illustrateProgram);
    }
  }
  GLFWwindow* setupWindow()
  {
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
    {
      return nullptr;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Rosenthal-Linsen-Lars-2008", NULL, NULL);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(1);
    return window;
  }
  void smooth()
  {
    int nextBuffer = currBuffer ^ 1;
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer[nextBuffer]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(smoothProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTexture[currBuffer]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, colorTexture[currBuffer]);
    glBindVertexArray(fboVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    currBuffer = nextBuffer;
  }
  bool failState;
  GLFWwindow* window;
  
  int pointCount;
  int currBuffer;
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
  glm::vec3 lightPos;
  GLuint pointVAO;
  GLuint pointVBO;
  GLuint pointVertShader;
  GLuint pointFragShader;
  GLuint pointProgram;
  GLuint fboVAO;
  GLuint fboVBO;
  GLuint backgroundVertShader;
  GLuint backgroundFragShader;
  GLuint backgroundProgram;
  GLuint occlusionVertShader;
  GLuint occlusionFragShader;
  GLuint occlusionProgram;
  GLuint smoothVertShader;
  GLuint smoothFragShader;
  GLuint smoothProgram;
  GLuint aaVertShader;
  GLuint aaFragHighShader;
  GLuint aaFragLowShader;
  GLuint aaHighProgram;
  GLuint aaLowProgram;
  GLuint illustrateVertShader;
  GLuint illustrateFragShader;
  GLuint illustrateProgram;
  GLint pointModelLoc;
  GLint pointViewLoc;
  GLint pointProjectionLoc;
  GLint pointLightPosLoc;
  GLint pointViewPosLoc;
  GLuint gBuffer[2];
  GLuint positionTexture[2];
  GLuint normalTexture[2];
  GLuint colorTexture[2];
  GLuint depthRenderBuffer[2];
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
  std::shared_ptr<tinyply::PlyData> colors;
  try
  {
    colors = file.request_properties_from_element("vertex", { "red", "green", "blue" });
  }
  catch (...)
  {
  }
  file.read(ss);
  if (!vertices)
  {
    throw std::invalid_argument(PLYpath.string() + " is missing elements required");
  }
  pointStride = colors ? 9 : 6;
  std::vector<float> PLYdata(pointStride*vertices->count);
  if (colors)
  {
    std::vector<float> vertexPosData(6 * vertices->count);
    std::memcpy(vertexPosData.data(), vertices->buffer.get(), vertices->buffer.size_bytes());
    std::vector<std::uint8_t> colorData(3 * vertices->count);
    std::memcpy(colorData.data(), colors->buffer.get(), colors->buffer.size_bytes());
    for (int i = 0; i < vertices->count; ++i)
    {
      PLYdata[i*pointStride] = vertexPosData[i * 6];
      PLYdata[i*pointStride+1] = vertexPosData[i * 6 +1];
      PLYdata[i*pointStride +2] = vertexPosData[i * 6 + 2];
      PLYdata[i*pointStride + 3] = vertexPosData[i * 6 + 3];
      PLYdata[i*pointStride + 4] = vertexPosData[i * 6 + 4];
      PLYdata[i*pointStride + 5] = vertexPosData[i * 6 + 5];
      PLYdata[i*pointStride + 6] = colorData[i * 3] / 255.f;
      PLYdata[i*pointStride + 7] = colorData[i * 3 + 1] / 255.f;
      PLYdata[i*pointStride + 8] = colorData[i * 3 + 2] / 255.f;
    }
  }
  else
  {
    std::memcpy(PLYdata.data(), vertices->buffer.get(), vertices->buffer.size_bytes());
  }
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