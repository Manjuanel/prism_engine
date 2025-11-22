#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//#define GLM_FORCE_RADIANS
//#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#include <glm/vec4.hpp>
//#include <glm/mat4x4.hpp>

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <limits>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <cstdlib>

#define WIDTH 800
#define HEIGHT 600
#define BACKGROUND {{{0.037, 0.017f, 0.069f, 1.0f}}}

class VkApp
{
  public: 
    void run (void)
    {
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

    struct QueueFamilyIndices { std::optional<uint32_t> graphicsQueue; std::optional<uint32_t> presentQueue; };    
    struct SwapChainSupportDetails {
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
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    const std::vector<char*> requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    //Sync
    VkSemaphore sImageAvailable;
    VkSemaphore sRenderFinished;
    VkFence fFrameEnded;

    void initWindow (void)
    {
      glfwInit();
      ///// WINDOW FLAGS /////
      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
      glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

      window = glfwCreateWindow(WIDTH, HEIGHT, "TEST", nullptr, nullptr);
    }
    void initVulkan (void)
    {
      createVkInstance();
      createSurface();
      selectGraphicCard();  
      createLogicalDevice();
      createSwapChain();
      createImageViews();
      createRenderPass();
      createGraphicsPipeline();
      createFramebuffers();
      createCommandPool();
      createCommandBuffer();
      createSyncObjects();
    }
    void mainLoop (void)
    {
      while(!glfwWindowShouldClose(window))
      {
        glfwPollEvents();
        drawFrame();
      } 
    }
    void cleanup (void)
    {
      ///// CLEAN SYNC /////
      vkDestroySemaphore(device, sImageAvailable, nullptr);
      vkDestroySemaphore(device, sRenderFinished, nullptr);
      vkDestroyFence(device, fFrameEnded, nullptr);
      ///// CLEAN VULKAN /////
      vkDestroyCommandPool(device, commandPool, nullptr);
      for(auto framebuffer : swapChainFramebuffers) vkDestroyFramebuffer(device, framebuffer, nullptr);
      vkDestroyPipeline(device, graphicsPipeline, nullptr);
      vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
      vkDestroyRenderPass(device, renderPass, nullptr);
      for(auto imageView : imageViews) vkDestroyImageView(device, imageView, nullptr);
      vkDestroySwapchainKHR(device, swapChain, nullptr);
      vkDestroySurfaceKHR(instance, surface, nullptr);
      vkDestroyInstance(instance, nullptr);
      vkDestroyDevice(device, nullptr);
      ///// CLEAN WINDOW /////
      glfwDestroyWindow(window); 
      glfwTerminate(); 
    }
    void drawFrame (void)
    {
      vkWaitForFences(device, 1, &fFrameEnded, VK_TRUE, UINT64_MAX);
      vkResetFences(device, 1, &fFrameEnded);
    
      uint32_t imageIndex;
      vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, sImageAvailable, VK_NULL_HANDLE, &imageIndex);

      vkResetCommandBuffer(commandBuffer, 0);
      recordCommandBuffer(commandBuffer, imageIndex); 
      
      VkSemaphore waitSemaphores[] = { sImageAvailable };
      VkSemaphore signalSemaphores[] = { sRenderFinished };
      VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT }; //Estudiar mas en profundidad
      VkSubmitInfo submitInfo {};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = waitSemaphores;
      submitInfo.pWaitDstStageMask = waitStages;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &commandBuffer;
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = signalSemaphores;
      if(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fFrameEnded) != VK_SUCCESS) throw std::runtime_error("ERROR: No fue posible completar un frame...");

      VkSwapchainKHR swapChains[] = { swapChain };
      VkPresentInfoKHR presentInfo {};
      presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pWaitSemaphores = signalSemaphores;
      presentInfo.swapchainCount = 1;
      presentInfo.pSwapchains = swapChains;
      presentInfo.pImageIndices = &imageIndex;
      vkQueuePresentKHR(presentQueue, &presentInfo);
    }
    void createSurface (void)
    {
      if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo crear la superficie de la ventana...");
    }
    void createVkInstance (void)
    {
      uint32_t extensionCount = 0;
      const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

      VkInstanceCreateInfo createInfo {};
      createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    //createInfo.pApplicationInfo = &appinfo;                 ////No aplica porque no cree un VkApplicationInfo (opcional)
      createInfo.enabledExtensionCount = extensionCount;
      createInfo.ppEnabledExtensionNames = glfwExtensions;
      if(vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo generar la instancia de Vulkan..."); 
    }
    void selectGraphicCard (void)
    {
      uint32_t graphicsCount = 0;
      vkEnumeratePhysicalDevices(instance, &graphicsCount, nullptr);
      
      if(graphicsCount == 0) throw std::runtime_error("ERROR: No se pudieron encontrar graficas compatibles con Vulkan...");

      std::vector<VkPhysicalDevice> graphicsList(graphicsCount); 
      vkEnumeratePhysicalDevices(instance, &graphicsCount, graphicsList.data());
      filterBestSuitablePhysicalDevice(graphicsList);
      findQueueFamilies(graphicsCard);
    }
    bool physicalDeviceSupportsExtensions (VkPhysicalDevice device)
    {
      uint32_t extensionCount; 

      vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
      std::vector<VkExtensionProperties> availableExtensions(extensionCount);
      vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

      std::set<std::string> allExtensions(requiredExtensions.begin(), requiredExtensions.end());
      for(const auto& extension : availableExtensions) allExtensions.erase(extension.extensionName);
      
      return allExtensions.empty() ? true : false;
    }
    void filterBestSuitablePhysicalDevice (std::vector<VkPhysicalDevice> devices)
    {
      std::multimap<int, VkPhysicalDevice> orderedDevices;
      VkPhysicalDeviceProperties properties;
      VkPhysicalDeviceFeatures features;
      
      for(const auto& device: devices)
      {
        int score = 0;
        vkGetPhysicalDeviceProperties(device, &properties);
      //vkGetPhysicalDeviceFeatures(device, &features); //No lo uso por ahora (tengo que ver que onda)
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
      // Preguntamos con opciones que preferamos
        if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
          score++;
        }
      // Descalificamos graficas que no cumplan nuestros requisitos
        if(!physicalDeviceSupportsExtensions(device) || swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
        {
          score = -1;
        }
        orderedDevices.insert(std::make_pair(score, device));
        std::cout << "DEVICE DETECTED: " << properties.deviceName << " \t SCORE: " << score << "pt" << std::endl;
        score = 0;
      }
    // Si la mayor puntuacion es menor a 0, todas fueron descalificadas
      if(orderedDevices.rbegin()->first < 0) throw std::runtime_error("ERROR: Ninguna grafica cumple con los requisitos...");
    // Asignar grafica con mayor puntaje (inicio del mapa ordenado)
      graphicsCard = orderedDevices.rbegin()->second;
    }
    void findQueueFamilies (VkPhysicalDevice device)
    { //TODO: Ver si borro queueFamilies de la clase, y hago que esta funcion returnee VkQueueFamilyProperties
      uint32_t queueFamilyCount = 0;

      vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
      if(queueFamilyCount == 0) throw std::runtime_error("ERROR: No se encontraron queue families para la tarjeta grafica seleccionada...");

      std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
      VkBool32 presentSupport = false;

      int i = 0; // probablemente habria que cambiar el for para que sea por indices, excepto que queueFamily tenga un metodo index() o algo asi
      for(auto& queueFamily : queueFamilies)
      {
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        { 
          queueIndices.graphicsQueue = i;
          if(presentSupport)
          { 
            queueIndices.presentQueue = i;
            break; // Priorizo que ambos sean el mismo queue
          }
        }
        if(presentSupport) queueIndices.presentQueue = i;

        i++;
      }
      if(!(queueIndices.presentQueue.has_value() && queueIndices.graphicsQueue.has_value())) throw std::runtime_error("ERROR: Tarjeta grafica no soporta dibujo en superficies...");
    }
    void createLogicalDevice (void)
    {
      VkPhysicalDeviceFeatures deviceFeatures {}; //No pedi ninguna feature antes, asi que aca no hago nada
      std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
      std::set<uint32_t> uniqueQueueFamilies = {queueIndices.presentQueue.value(),queueIndices.graphicsQueue.value()};
      float queuePriority = 1.0f;

      for(const uint32_t queueFamily : uniqueQueueFamilies)
      {
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
      if(vkCreateDevice(graphicsCard, &createInfo, nullptr, &device) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo crear un dispositivo logico...");

      vkGetDeviceQueue(device, queueIndices.graphicsQueue.value(), 0, &graphicsQueue);
      vkGetDeviceQueue(device, queueIndices.presentQueue.value(), 0, &presentQueue);
    }
    SwapChainSupportDetails querySwapChainSupport (VkPhysicalDevice device)
    {
      SwapChainSupportDetails details;
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
      uint32_t formatCounts = 0;
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCounts, nullptr);
      if(formatCounts != 0)
      {
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
    VkSurfaceFormatKHR chooseSurfaceFormat (const std::vector<VkSurfaceFormatKHR>& availableFormats) // probablemente hay que revisar esta funcion
    {
      for(const auto& availableFormat : availableFormats)
      {
        if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return availableFormat;
      }
      return availableFormats[0];
    }
    VkPresentModeKHR choosePresentMode (const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
      for(const auto& availablePresentMode : availablePresentModes)
      {
        if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode;
      }
      return VK_PRESENT_MODE_FIFO_KHR;
    }
    VkExtent2D chooseSwapExtent (const VkSurfaceCapabilitiesKHR& capabilities)
    {
      if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
      {
        return capabilities.currentExtent;
      } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width); 
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height); 
        return actualExtent;
      } 
    }
    void createSwapChain (void)
    {
      SwapChainSupportDetails details = querySwapChainSupport(graphicsCard);
      VkSurfaceFormatKHR format = chooseSurfaceFormat(details.formats);
      VkPresentModeKHR presentMode = choosePresentMode(details.presentModes);
      VkExtent2D extent = chooseSwapExtent(details.capabilities);
      swapChainImageFormat = format.format;      
      swapChainExtent = extent;
      uint32_t imageCount = details.capabilities.minImageCount + 1;
      
      if(imageCount > 0 && imageCount < details.capabilities.maxImageCount) imageCount = details.capabilities.maxImageCount;
      
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

      if(queueIndices.graphicsQueue != queueIndices.presentQueue)
      {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
      } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
      }
      if(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo crear la Swapchain...");

      vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
      swapChainImages.resize(imageCount);      
      vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    }
    void createImageViews (void)
    {
      imageViews.resize(swapChainImages.size());

      for(int i = 0; i < swapChainImages.size(); i++)
      {
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
    
        if(vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS) throw std::runtime_error("ERROR: No pudieron generarse las imagenes de la swapchain...");
      }
    }
    void createGraphicsPipeline (void)
    {
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

      std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

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

      VkPipelineMultisampleStateCreateInfo multisample {};
      multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
      multisample.sampleShadingEnable = VK_FALSE;
      multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

      // Para implementar transparencia, mirar esta estructura
      VkPipelineColorBlendAttachmentState colorBlendAttachment {};
      colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      colorBlendAttachment.blendEnable = VK_FALSE;

      VkPipelineColorBlendStateCreateInfo colorBlend {};
      colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      colorBlend.logicOpEnable = VK_FALSE;
      colorBlend.attachmentCount = 1;
      colorBlend.pAttachments = &colorBlendAttachment;

      VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
      pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      
      if(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo crear el pipeline layout...");

      VkGraphicsPipelineCreateInfo pipelineInfo {};
      pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
      pipelineInfo.stageCount = 2;
      pipelineInfo.pStages = shaderStages;
      pipelineInfo.pVertexInputState = &vertexInput;
      pipelineInfo.pInputAssemblyState = &inputAssembly;
      pipelineInfo.pViewportState = &viewportState;
      pipelineInfo.pRasterizationState = &rasterizer;
      pipelineInfo.pMultisampleState = &multisample;
      pipelineInfo.pDepthStencilState = nullptr;
      pipelineInfo.pColorBlendState = &colorBlend;
      pipelineInfo.pDynamicState = &dynamicState;
      pipelineInfo.layout = pipelineLayout;
      pipelineInfo.renderPass = renderPass;
      pipelineInfo.subpass = 0;

      if(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo crear la pipeline grafica...");

      // Una vez creado el pipeline grafico, no necesitamos los shader modules, asi que podemos eliminarlos aca
      vkDestroyShaderModule(device, fragShaderModule, nullptr);
      vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }
    void createRenderPass (void)
    {
      VkAttachmentDescription colorAttachment {};
      colorAttachment.format = swapChainImageFormat;
      colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Tiene que ver con MSAA
      colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      
      VkAttachmentReference colorAttachmentRef {};
      colorAttachmentRef.attachment = 0;
      colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      VkSubpassDescription subpass {};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 1;
      subpass.pColorAttachments = &colorAttachmentRef;

      VkRenderPassCreateInfo createInfo {};
      createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
      createInfo.attachmentCount = 1;
      createInfo.pAttachments = &colorAttachment;
      createInfo.subpassCount = 1;
      createInfo.pSubpasses = &subpass;

      if(vkCreateRenderPass(device, &createInfo, nullptr, &renderPass) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo crear el Render Pass...");
    }
    void createFramebuffers (void)
    {
      swapChainFramebuffers.resize(imageViews.size());
      for(int i = 0; i < imageViews.size(); i++)
      {
        VkImageView attachments[] = { imageViews[i] };

        VkFramebufferCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = attachments;
        createInfo.width = swapChainExtent.width;
        createInfo.height = swapChainExtent.height;
        createInfo.layers = 1;

        if(vkCreateFramebuffer(device, &createInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudieron crear los Frambuffers...");
      }
    }
    void createCommandPool (void)
    {
      VkCommandPoolCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      createInfo.queueFamilyIndex = queueIndices.graphicsQueue.value();
      if(vkCreateCommandPool(device, &createInfo, nullptr, &commandPool) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo crear la Command pool...");
    }
    void createCommandBuffer (void)
    {
      VkCommandBufferAllocateInfo createInfo {};
      createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      createInfo.commandPool = commandPool;
      createInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      createInfo.commandBufferCount = 1; // Por ahora probably
      if(vkAllocateCommandBuffers(device, &createInfo, &commandBuffer) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo alocar memoria para el command buffer...");
    }
    void recordCommandBuffer (VkCommandBuffer commandBuffer, uint32_t& imageIndex)
    {
      VkClearValue clearColor = BACKGROUND; //asumo

      VkCommandBufferBeginInfo beginInfo {};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = 0; // Buscar info al respecto
      beginInfo.pInheritanceInfo = nullptr; // Buscar info al respecto
      if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo comenzar a escribir el command buffer...");

      VkRenderPassBeginInfo renderBeginInfo {};
      renderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderBeginInfo.renderPass = renderPass;
      renderBeginInfo.framebuffer = swapChainFramebuffers[imageIndex];
      renderBeginInfo.renderArea.offset = {0, 0};
      renderBeginInfo.renderArea.extent = swapChainExtent;
      renderBeginInfo.clearValueCount = 1;
      renderBeginInfo.pClearValues = &clearColor;
      //Commands
      vkCmdBeginRenderPass(commandBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

      VkViewport viewport {};
      viewport.x = 0.0f;
      viewport.y = 0.0f;
      viewport.height = static_cast<float>(swapChainExtent.height);
      viewport.width = static_cast<float>(swapChainExtent.width);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

      VkRect2D scissor {};
      scissor.offset = {0, 0};
      scissor.extent = swapChainExtent;
      vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

      vkCmdDraw(commandBuffer, 3, 1, 0, 0);

      vkCmdEndRenderPass(commandBuffer);
      if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo terminar de escribir el command buffer...");
    }
    void createSyncObjects (void)
    {
      VkSemaphoreCreateInfo semaphoreInfo {};
      semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      VkFenceCreateInfo fenceInfo {};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //Flag para que empiece señalado.

      if(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &sImageAvailable) ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &sRenderFinished) ||
        vkCreateFence(device, &fenceInfo, nullptr, &fFrameEnded))
      {
        throw std::runtime_error("ERROR: No se pudieron crear los objetos de sincronizacion...");    
      }
    }
    static std::vector<char> readShader (const std::string& filename)
    {
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
    VkShaderModule createShaderModule (const std::vector<char>& file)
    {
      VkShaderModuleCreateInfo createInfo {}; 
      createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      createInfo.codeSize = file.size();
      createInfo.pCode = reinterpret_cast<const uint32_t*>(file.data());

      VkShaderModule shaderModule;
      if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) throw std::runtime_error("ERROR: No se pudo crear el shader module...");
      return shaderModule;
    }
};

int main (void)
{
  VkApp app;
  try {
    app.run();
  } catch(const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
