#pragma once


//#define LCA_DEBUG

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


//C
#include <string.h>
#include <time.h>
//C++
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <queue>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <bitset>
#include <memory>
#include <thread>
#include <random>
#include <functional>
#include <typeinfo>
#include <algorithm>
#include <thread>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glm/gtc/quaternion.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#define LCA_LOGI(X, Y, Z)\
{\
    std::cout << "Info@LCA: " << X << " :: " << Y << " :: " << Z << std::endl;\
}

#define LCA_LOGE(X, Y, Z)\
{\
    std::cerr << "Error@LCA: " << X << " :: " << Y << " :: " << Z << std::endl;\
    exit(1);\
}

#define LCA_CHECK_VULKAN(VulkanExpression, Function, VulkanFunction)\
{\
    VkResult vkRes;\
    if((vkRes = VulkanExpression) != VK_SUCCESS)\
    {\
        std::cerr << "Error@LCA: " << Function << " :: " << VulkanFunction << std::endl;\
        std::cerr << "Vulkan Error Code: " << vkRes << std::endl;\
        exit(1);\
    }\
}


#define LCA_ASSERT(Expression, Class, Funktion, Message)\
{\
    if(!(Expression))\
    {\
        LCA_LOGE(Class, Funktion, Message)\
    }\
}

#define init_random() srand(static_cast<unsigned int>(time(0)))
#define random(lower, upper) ((static_cast<float>(rand())/static_cast<float>(RAND_MAX))*((upper)-(lower)) + (lower))
