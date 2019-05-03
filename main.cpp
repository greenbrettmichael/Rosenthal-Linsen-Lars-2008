#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include "third-party/tinyply/source/tinyply.h"
#include "build/third-party/glad/include/glad/glad.h"
#include "third-party/glfw/include/GLFW/glfw3.h"

int windowHeight = 1024;
int windowWidth = 1024;
int pointStride = 6;

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
  glfwSetKeyCallback(window, key_callback);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
  glfwSwapInterval(1);
  return window;
}

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
  std::vector<float> PLYdata(pointStride*vertices->count);
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
  GLFWwindow* window = setupWindow();
  if (!window)
  {
    std::cout << "FAILED TO CREATE GLFW WINDOW " << std::endl;
    glfwTerminate();
    return 5;
  }
  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}