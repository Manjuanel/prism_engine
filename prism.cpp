#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdlib>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <limits>
#include <algorithm>
#include <fstream>

#define WIDTH 800
#define HEIGHT 600

class testApp {
  public: 
    void run() {
      initWindow();
      initVulkan();
      mainLoop();
      cleanup();
    }
  private:
    
    GLFWwindow* window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice graphicsCard;

    struct QueueFamilyIndices{
      std::optional<uint32_t> graphicsQueue;
      std::optional<uint32_t> presentQueue;
    };    
    struct SwapChainSupportDetails{
      VkSurfaceCapabilitiesKHR capabilities;
      std::vector<VkSurfaceFormatKHR> formats;
      std::vector<VkPresentModeKHR> presentModes;
    };
  
    QueueFamilyIndices queueIndices;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkDevice device;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> imageViews;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    VkPipelineLayout pipelineLayout;

    const std::vector<char*> requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    void initWindow(){
      glfwInit();
      
      ///// WINDOW FLAGS /////
      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
      glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

      window = glfwCreateWindow(WIDTH, HEIGHT,  "TEST", nullptr, nullptr);

    }
    void initVulkan(){
      createVkInstance();
      createSurface();
      selectGraphicCard();  
      createLogicalDevice();
      createSwapChain();
      createImageViews();
      createGraphicsPipeline();
    }
    void mainLoop(){
      while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
      } 
    }
    void cleanup(){
      ///// CLEAN VULKAN /////
      vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
      for(auto imageView : imageViews) vkDestroyImageView(device, imageView, nullptr);
      vkDestroySwapchainKHR(device, swapChain, nullptr);
      vkDestroySurfaceKHR(instance, surface, nullptr);
      vkDestroyInstance(instance, nullptr);
      vkDestroyDevice(device, nullptr);
      ///// CLEAN WINDOW /////
      glfwDestroyWindow(window); 
      glfwTerminate(); 
    }

    void createSurface(){
      if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS){
        throw std::runtime_error("ERROR: No se pudo crear la superficie de la ventana...");
      }
    }
    void createVkInstance(){
      VkInstanceCreateInfo createInfo {};
      createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

      
      //createInfo.pApplicationInfo = &appinfo;                 ////No aplica porque no cree un VkApplicationInfo (opcional)

      uint32_t extensionCount = 0;
      const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
      
      createInfo.enabledExtensionCount = extensionCount;
      createInfo.ppEnabledExtensionNames = glfwExtensions;

      if(!(vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS)){
        throw std::runtime_error("ERROR: No se pudo generar la instancia de Vulkan...");
      } 
    }
    void selectGraphicCard(){
      uint32_t graphicsCount = 0;
      vkEnumeratePhysicalDevices(instance, &graphicsCount, nullptr);
      
      if(graphicsCount == 0){
        throw std::runtime_error("ERROR: No se pudieron encontrar graficas compatibles con Vulkan...");
      }
      std::vector<VkPhysicalDevice> graphicsList(graphicsCount); 
      vkEnumeratePhysicalDevices(instance, &graphicsCount, graphicsList.data());

      filterBestSuitablePhysicalDevice(graphicsList);
      findQueueFamilies(graphicsCard);
    }
    bool physicalDeviceSupportsExtensions(VkPhysicalDevice device){
      uint32_t extensionCount; 

      vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
      std::vector<VkExtensionProperties> availableExtensions(extensionCount);
      vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

      std::set<std::string> allExtensions(requiredExtensions.begin(), requiredExtensions.end());

      for(const auto& extension : availableExtensions){
        allExtensions.erase(extension.extensionName);
      }
      if(!allExtensions.empty()){
        return false;
      } else return true;
    }
    void filterBestSuitablePhysicalDevice(std::vector<VkPhysicalDevice> devices){
      std::multimap<int, VkPhysicalDevice> orderedDevices;
      int score = 0;

      VkPhysicalDeviceProperties properties;
      VkPhysicalDeviceFeatures features;
      
      for(const auto& device: devices){
        vkGetPhysicalDeviceProperties(device, &properties);
        vkGetPhysicalDeviceFeatures(device, &features); //No lo uso por ahora (tengo que ver que onda)

        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        
        if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
          score++;
        }
        if(!physicalDeviceSupportsExtensions(device) || swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()){
          score = -1;
        }

        orderedDevices.insert(std::make_pair(score, device));
        std::cout << "DEVICE DETECTED: " << properties.deviceName << " \t SCORE: " << score << "pt" << std::endl;
        score = 0;
      }
      if(orderedDevices.rbegin()->first < 0){
        throw std::runtime_error("ERROR: Ninguna grafica cumple con los requisitos...");
      }        
      graphicsCard = orderedDevices.rbegin()->second; // Asignar grafica con mayor puntaje (inicio del mapa ordenado)
    }
    void findQueueFamilies(VkPhysicalDevice device){ //TODO: Ver si borro queueFamilies de la clase, y hago que esta funcion returnee VkQueueFamilyProperties
      uint32_t queueFamilyCount = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
      if(queueFamilyCount == 0){
        throw std::runtime_error("ERROR: No se encontraron queue families para la tarjeta grafica seleccionada...");
      }
      std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
      
      VkBool32 presentSupport = false;

      int i = 0;
      for(auto& queueFamily : queueFamilies){
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT){ // VK_QUEUE_GRAPHICS_BIT es una mascara que aisla solo 1 bit,
          queueIndices.graphicsQueue = i;                   // por ende si ese bit no esta presente en queueFlags, el resultado da 0 = FALSE
          if(presentSupport){ 
            queueIndices.presentQueue = i;
            break; // Priorizo que ambos sean el mismo queue
          }
        }
        if(presentSupport){
          queueIndices.presentQueue = i;
        }
        i++;
      }
      if(!(queueIndices.presentQueue.has_value() && queueIndices.graphicsQueue.has_value())){
          throw std::runtime_error("ERROR: Tarjeta grafica no soporta dibujo en superficies...");
        }
    }
    void createLogicalDevice(){
      VkPhysicalDeviceFeatures deviceFeatures {}; //No pedi ninguna feature antes, asi que aca no hago nada

      std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
      std::set<uint32_t> uniqueQueueFamilies = {queueIndices.presentQueue.value(),queueIndices.graphicsQueue.value()};
      float queuePriority = 1.0f;

      for(uint32_t queueFamily : uniqueQueueFamilies){
        VkDeviceQueueCreateInfo queueCreateInfo {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; 
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
      }

      VkDeviceCreateInfo createInfo {};
      createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; 
      createInfo.pQueueCreateInfos = queueCreateInfos.data();
      createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
      createInfo.pEnabledFeatures = &deviceFeatures; 
      createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()); 
      createInfo.ppEnabledExtensionNames = requiredExtensions.data(); 

      if(vkCreateDevice(graphicsCard, &createInfo, nullptr, &device) != VK_SUCCESS){
        throw std::runtime_error("ERROR: No se pudo crear un dispositivo logico...");
      }
      vkGetDeviceQueue(device, queueIndices.graphicsQueue.value(), 0, &graphicsQueue);
      vkGetDeviceQueue(device, queueIndices.presentQueue.value(), 0, &presentQueue);
    }
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device){
      SwapChainSupportDetails details;
      
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

      uint32_t formatCounts = 0;
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCounts, nullptr);
      if (formatCounts != 0){
        details.formats.resize(formatCounts);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCounts, details.formats.data());
      }
      uint32_t presentModesCount = 0;
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, nullptr);
      if (formatCounts != 0){
        details.presentModes.resize(presentModesCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, details.presentModes.data());
      }
return details;
    }
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats){
      for(const auto& availableFormat : availableFormats){
        if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return availableFormat;
      }
      return availableFormats[0];
    }
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes){
      for(const auto& availablePresentMode : availablePresentModes){
        if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode;
      }
      return VK_PRESENT_MODE_FIFO_KHR;
    }
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities){
      if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()){
        return capabilities.currentExtent;
      } else {
        int width, height;
  
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
          static_cast<uint32_t>(width),
          static_cast<uint32_t>(height)
        };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width); 
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height); 
        
        return actualExtent;
      } 
    }
    void createSwapChain(){
      SwapChainSupportDetails details = querySwapChainSupport(graphicsCard);
      
      VkSurfaceFormatKHR format = chooseSurfaceFormat(details.formats);
      VkPresentModeKHR presentMode = choosePresentMode(details.presentModes);
      VkExtent2D extent = chooseSwapExtent(details.capabilities);

      swapChainImageFormat = format.format;      
      swapChainExtent = extent;

      uint32_t imageCount = details.capabilities.minImageCount + 1;
      
      if(imageCount > 0 && imageCount > details.capabilities.maxImageCount){
        imageCount = details.capabilities.maxImageCount;
      }
      
      VkSwapchainCreateInfoKHR createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
      createInfo.surface = surface;
      createInfo.minImageCount = imageCount;
      createInfo.imageFormat = format.format;
      createInfo.imageColorSpace = format.colorSpace;
      createInfo.presentMode = presentMode;
      createInfo.imageExtent = extent;
      createInfo.clipped = VK_TRUE;
      createInfo.oldSwapchain = VK_NULL_HANDLE;
      createInfo.imageArrayLayers = 1;
      createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //TODO: ver lo del post-procesado
      createInfo.preTransform = details.capabilities.currentTransform;
      createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

      uint32_t queueFamilyIndices[] = { queueIndices.graphicsQueue.value(), queueIndices.presentQueue.value() };

      if(queueIndices.graphicsQueue != queueIndices.presentQueue){
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
      } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
      }
      if(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS){
        throw std::runtime_error("ERROR: No se pudo crear la Swapchain...");
      }
      vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
      swapChainImages.resize(imageCount);      
      vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    }
    void createImageViews(){
      imageViews.resize(swapChainImages.size());

      for(int i = 0; i < swapChainImages.size(); i++){ //Usamos un indice a diferencia de en otros casos, porque tenemos que referirnos a objetos de 2 listas
        VkImageViewCreateInfo createInfo{};
        
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; //Interesante, te permite tratar con texturas 3D
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
    
        if(vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS){
          throw std::runtime_error("ERROR: No pudieron generarse las imagenes de la swapchain...");
        }
      }
    }
    void createGraphicsPipeline(){
      auto fragShader = readShader("shaders/compiled/frag.spv"); 
      auto vertShader = readShader("shaders/compiled/vert.spv"); 

      VkShaderModule fragShaderModule = createShaderModule(fragShader);
      VkShaderModule vertShaderModule = createShaderModule(vertShader);

      VkPipelineShaderStageCreateInfo fragShaderCreateInfo {};
      fragShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; 
      fragShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; //Hay una flag para cada estado programable de la pipeline (vertex, fragment, geometry)
      fragShaderCreateInfo.module = fragShaderModule;
      fragShaderCreateInfo.pName = "main";
      
      VkPipelineShaderStageCreateInfo vertShaderCreateInfo {};
      vertShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; 
      vertShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
      vertShaderCreateInfo.module = vertShaderModule;
      vertShaderCreateInfo.pName = "main";

      VkPipelineShaderStageCreateInfo shaderStages[] = {fragShaderCreateInfo, vertShaderCreateInfo};

      std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
      };

      VkPipelineDynamicStateCreateInfo dynamicState {};
      dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
      dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
      dynamicState.pDynamicStates = dynamicStates.data();
      
      VkPipelineVertexInputStateCreateInfo vertexInput {};
      vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vertexInput.vertexBindingDescriptionCount = 0;
      vertexInput.pVertexBindingDescriptions = nullptr;
      vertexInput.vertexAttributeDescriptionCount = 0;
      vertexInput.pVertexAttributeDescriptions = nullptr;

      VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
      inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      inputAssembly.primitiveRestartEnable = VK_FALSE;

      // Solo se declaran aca si la implementacion las mantiene estaticas (no es el caso)
      //
      //VkViewport viewport {};
      //viewport.x = 0.0f;
      //viewport.y = 0.0f;
      //viewport.height = (float) swapChainExtent.height;
      //viewport.width  = (float) swapChainExtent.width;
      //viewport.minDepth = 0.0f;
      //viewport.maxDepth = 1.0f;
      //
      //VkRect2D scissor {};
      //scissor.offset = {0, 0};
      //scissor.extent = swapChainExtent;

      VkPipelineViewportStateCreateInfo viewportState {};
      viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      viewportState.viewportCount = 1;
      //viewportState.pViewports = &viewport;       // Solo para implementaciones estaticas
      viewportState.scissorCount = 1;
      //viewportState.pScissors = &scissor;       // Solo para implementaciones estaticas

      VkPipelineRasterizationStateCreateInfo rasterizer {};
      rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      rasterizer.depthClampEnable = VK_FALSE;
      rasterizer.rasterizerDiscardEnable = VK_FALSE;
      rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
      rasterizer.lineWidth = 1.0f;
      rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
      rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
      rasterizer.depthBiasEnable = VK_FALSE;

      VkPipelineMultisamplingStateCreateInfo multisampling {};
      multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLING_STATE_CREATE_INFO;
      multisampling.sampleShadingEnable = VK_FALSE;
      multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

      // Para implementar transparencia, mirar esta estructura
      VkPipelineColorBlendAttachmentState colorBlendAttachment {};
      colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      colorBlendAttachment.blendEnable = VK_FALSE;

      VkPipelineColorBlendStateCreateInfo colorBlend {};
      colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_CREATE_INFO;
      colorBlend.LogicOpEnable = VK_FALSE;
      colorBlend.attachmentCount = 1;
      colorBlend.pAttachments = &colorBlendAttachment;

      VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
      pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      
      if(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS){
        throw std::runtime_error("ERROR: No se pudo crear la pipeline...");
      }

      // Una vez creado el pipeline grafico, no necesitamos los shader modules, asi que podemos eliminarlos aca
      vkDestroyShaderModule(device, fragShaderModule, nullptr);
      vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }
    static std::vector<char> readShader(const std::string& filename){
      std::ifstream file(filename, std::ios::ate | std::ios::binary);
      if(!file.is_open()) throw std::runtime_error("ERROR: No se pudo abrir el shader " + filename);
      
      size_t fileSize = (size_t) file.tellg();
      std::vector <char> buffer(fileSize);
      
      std::cout << filename << " size is: " << fileSize << "bytes." << std::endl;

      file.seekg(0); //Ir al inicio del archivo
      file.read(buffer.data(), fileSize);
      file.close();
      
      return buffer;
    }
    VkShaderModule createShaderModule(const std::vector<char>& file){
      VkShaderModuleCreateInfo createInfo {}; 
      createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      createInfo.codeSize = file.size();
      createInfo.pCode = reinterpret_cast<const uint32_t*>(file.data());

      VkShaderModule shaderModule;
      if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo crear el shader module...");

      return shaderModule;
    }
};

int main(){
  testApp app;
  try {
    app.run();
  } catch(const std::exception& e){
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
