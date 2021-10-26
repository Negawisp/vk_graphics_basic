#include "simple_compute.h"

#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <vk_utils.h>

SimpleCompute::SimpleCompute(uint32_t a_length) : m_length(a_length)
{
#ifdef NDEBUG
  m_enableValidation = false;
#else
  m_enableValidation = true;
#endif

  m_nWorkGroups = a_length % WORK_GROUP_SIZE == 0 ? a_length / WORK_GROUP_SIZE :
                                                    a_length / WORK_GROUP_SIZE + 1;

  std::cout << "Work groups number: " << m_nWorkGroups << "\n";
}

void SimpleCompute::SetupValidationLayers()
{
  m_validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  m_validationLayers.push_back("VK_LAYER_LUNARG_monitor");
}

void SimpleCompute::InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId)
{
  m_instanceExtensions.clear();
  for (uint32_t i = 0; i < a_instanceExtensionsCount; ++i) {
    m_instanceExtensions.push_back(a_instanceExtensions[i]);
  }
  SetupValidationLayers();
  VK_CHECK_RESULT(volkInitialize());
  CreateInstance();
  volkLoadInstance(m_instance);

  CreateDevice(a_deviceId);
  volkLoadDevice(m_device);

  m_commandPool = vk_utils::createCommandPool(m_device, m_queueFamilyIDXs.compute, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  m_cmdBufferCompute = vk_utils::createCommandBuffers(m_device, m_commandPool, 1)[0];
  
  m_pCopyHelper = std::make_shared<vk_utils::SimpleCopyHelper>(m_physicalDevice, m_device, m_transferQueue, m_queueFamilyIDXs.compute, 8*1024*1024);
}


void SimpleCompute::CreateInstance()
{
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pNext = nullptr;
  appInfo.pApplicationName = "VkRender";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.pEngineName = "SimpleCompute";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);

  m_instance = vk_utils::createInstance(m_enableValidation, m_validationLayers, m_instanceExtensions, &appInfo);
  if (m_enableValidation)
    vk_utils::initDebugReportCallback(m_instance, &debugReportCallbackFn, &m_debugReportCallback);
}

void SimpleCompute::CreateDevice(uint32_t a_deviceId)
{
  m_physicalDevice = vk_utils::findPhysicalDevice(m_instance, true, a_deviceId, m_deviceExtensions);

  m_device = vk_utils::createLogicalDevice(m_physicalDevice, m_validationLayers, m_deviceExtensions,
                                           m_enabledDeviceFeatures, m_queueFamilyIDXs,
                                           VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.compute, 0, &m_computeQueue);
  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.transfer, 0, &m_transferQueue);
}


void SimpleCompute::SetupPipelines()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             3}
  };

  // Создание и аллокация буферов
  m_buffInput = vk_utils::createBuffer(m_device, sizeof(float) * m_length, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  m_buffScans = vk_utils::createBuffer(m_device, sizeof(float) * m_length, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  m_buffOffsets = vk_utils::createBuffer(m_device, sizeof(float) * m_nWorkGroups, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

  auto buffers = { m_buffInput, m_buffScans, m_buffOffsets };
  vk_utils::allocateAndBindWithPadding(m_device, m_physicalDevice, buffers, 0);
  std::cout << "Buffers allocated" << std::endl;

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_device, dtypes, 3);

  // Создание descriptor set для передачи буферов в шейдеры
  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, m_buffScans);
  m_pBindings->BindBuffer(1, m_buffScans);
  m_pBindings->BindBuffer(2, m_buffOffsets);
  m_pBindings->BindEnd(&m_scanDS1, &m_scanDSLayout1);

  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, m_buffOffsets);
  m_pBindings->BindBuffer(1, m_buffOffsets);
  m_pBindings->BindEnd(&m_scanDS2, &m_scanDSLayout2);

  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, m_buffScans);
  m_pBindings->BindBuffer(1, m_buffOffsets);
  m_pBindings->BindBuffer(2, m_buffScans);
  m_pBindings->BindEnd(&m_scanDS3, &m_scanDSLayout3);

  std::vector<float> values(m_length);
  // Заполнение входного буфера единицами
  /*
  for (uint32_t i = 0; i < values.size(); ++i) {
    values[i] = 1;
  }
  */

  // Заполнение входного буфера членами ряда e**x
  float x = 7;
  float c_i = 1;
  for (uint32_t i = 0; i < values.size(); ++i) {
      values[i] = c_i;
      c_i *= x / (i+1);
  }
  m_pCopyHelper->UpdateBuffer(m_buffScans, 0, values.data(), sizeof(float) * values.size());
}

void SimpleCompute::BuildCommandBufferScan(VkCommandBuffer a_cmdBuff, VkPipeline a_pipeline)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  // Создаём барьер-буффер
  VkBufferMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.pNext = NULL;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.buffer = m_buffOffsets;
  barrier.offset = 0;
  barrier.size = VK_WHOLE_SIZE;

  // Заполняем буфер команд
  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  // Первый шейдер
  vkCmdBindPipeline      (a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline1);
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout1, 0, 1, &m_scanDS1, 0, NULL);
  
  // Заполняем пуш-константы для первого шейдера
  vkCmdPushConstants(a_cmdBuff, m_layout1, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(m_length), &m_length);

  // Диспатчим первый шейдер
  vkCmdDispatch(a_cmdBuff, m_nWorkGroups, 1, 1);

  // Синхронизируем ворк-группы
  vkCmdPipelineBarrier(a_cmdBuff,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_DEPENDENCY_BY_REGION_BIT,
      0, nullptr,
      1, &barrier,
      0, nullptr);


  // Второй шейдер
  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline2);
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout2, 0, 1, &m_scanDS2, 0, NULL);

  // Заполняем пуш-константы для второго шейдера
  vkCmdPushConstants(a_cmdBuff, m_layout2, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(m_nWorkGroups), &m_nWorkGroups);

  // Диспатчим второй шейдер
  vkCmdDispatch(a_cmdBuff, 1, 1, 1);

  // Синхронизируем ворк-группы
  vkCmdPipelineBarrier(a_cmdBuff,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_DEPENDENCY_BY_REGION_BIT,
      0, nullptr,
      1, &barrier,
      0, nullptr);


  // Третий шейдер
  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline3);
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout3, 0, 1, &m_scanDS3, 0, NULL);

  // Заполняем пуш-константы для третьего шейдера
  vkCmdPushConstants(a_cmdBuff, m_layout3, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(m_length), &m_length);

  // Диспатчим третий шейдер
  vkCmdDispatch(a_cmdBuff, m_nWorkGroups, 1, 1);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}


void SimpleCompute::CleanupPipeline()
{
  if (m_cmdBufferCompute)
  {
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_cmdBufferCompute);
  }

  vkDestroyBuffer(m_device, m_buffInput, nullptr);
  vkDestroyBuffer(m_device, m_buffScans, nullptr);
  vkDestroyBuffer(m_device, m_buffOffsets, nullptr);

  vkDestroyPipelineLayout(m_device, m_layout1, nullptr);
  vkDestroyPipelineLayout(m_device, m_layout2, nullptr);
  vkDestroyPipelineLayout(m_device, m_layout3, nullptr);

  vkDestroyPipeline(m_device, m_pipeline1, nullptr);
  vkDestroyPipeline(m_device, m_pipeline2, nullptr);
  vkDestroyPipeline(m_device, m_pipeline3, nullptr);
}


void SimpleCompute::Cleanup()
{
  CleanupPipeline();

  if (m_commandPool != VK_NULL_HANDLE)
  {
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
  }
}


void SimpleCompute::CreatePipeline(std::string spvPath, VkDescriptorSetLayout& dsLayout,
                                   VkPipelineLayout& pipelineLayout, VkPipeline& pipeline)
{
  // Загружаем шейдер
  std::vector<uint32_t> code = vk_utils::readSPVFile(spvPath.c_str());
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.pCode    = code.data();
  createInfo.codeSize = code.size()*sizeof(uint32_t);
    
  VkShaderModule shaderModule;
  // Создаём шейдер в вулкане
  VK_CHECK_RESULT(vkCreateShaderModule(m_device, &createInfo, NULL, &shaderModule));

  VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
  shaderStageCreateInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStageCreateInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStageCreateInfo.module = shaderModule;
  shaderStageCreateInfo.pName  = "main";

  VkPushConstantRange pcRange = {};
  pcRange.offset = 0;
  pcRange.size = sizeof(m_length);
  pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // Создаём layout для pipeline
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = 1;
  pipelineLayoutCreateInfo.pSetLayouts    = &dsLayout;
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &pcRange;
  VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout));

  VkComputePipelineCreateInfo pipelineCreateInfo = {};
  pipelineCreateInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stage  = shaderStageCreateInfo;
  pipelineCreateInfo.layout = pipelineLayout;

  // Создаём pipeline - объект, который выставляет шейдер и его параметры
  VK_CHECK_RESULT(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &pipeline));

  vkDestroyShaderModule(m_device, shaderModule, nullptr);
}


void SimpleCompute::CreatePipelines()
{
  CreatePipeline("../resources/shaders/scan_p1.comp.spv", m_scanDSLayout1, m_layout1, m_pipeline1);
  CreatePipeline("../resources/shaders/scan_p2.comp.spv", m_scanDSLayout2, m_layout2, m_pipeline2);
  CreatePipeline("../resources/shaders/scan_p3.comp.spv", m_scanDSLayout3, m_layout3, m_pipeline3);
}


void SimpleCompute::Execute()
{
  SetupPipelines();
  std::cout << "Pipelines setup" << std::endl;

  CreatePipelines();
  std::cout << "Pipelines created" << std::endl;

  BuildCommandBufferScan(m_cmdBufferCompute, nullptr);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_cmdBufferCompute;

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = 0;
  VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, NULL, &m_fence));

  // Отправляем буфер команд на выполнение
  VK_CHECK_RESULT(vkQueueSubmit(m_computeQueue, 1, &submitInfo, m_fence));

  //Ждём конца выполнения команд
  VK_CHECK_RESULT(vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, 100000000000));

  std::vector<float> values1(m_length);
  m_pCopyHelper->ReadBuffer(m_buffScans, 0, values1.data(), sizeof(float) * values1.size());
  for (auto v : values1) {
      std::cout << v << ' ';
  }
}
