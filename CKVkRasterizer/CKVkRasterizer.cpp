#include "CKVkRasterizer.h"

#include <cstdio>
#include <algorithm>
#include <vector>
#include <set>

CKVkRasterizer::CKVkRasterizer(void): m_Init(FALSE), vkinst(nullptr)
{
}

CKVkRasterizer::~CKVkRasterizer(void)
{
}

// Callback for EnumDisplayMonitors in CKVkRasterizer::Start
//
static BOOL CALLBACK monitorCallback(HMONITOR handle,
                                     HDC dc,
                                     RECT* rect,
                                     LPARAM data)
{
    return GetMonitorInfo(handle, (LPMONITORINFO)data);
}

void CKVkRasterizer::toggle_console(int t)
{
    static bool enabled = false;
    if (t == 0) enabled ^= true;
    else enabled = t > 0;
    if (enabled)
    {
        AllocConsole();
        freopen("CON", "r", stdin);
        freopen("CON", "w", stdout);
        freopen("CON", "w", stderr);
    }
    else
    {
        FreeConsole();
        freopen("NUL", "r", stdin);
        freopen("NUL", "w", stdout);
        freopen("NUL", "w", stderr);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT s, VkDebugUtilsMessageTypeFlagsEXT ty,
    const VkDebugUtilsMessengerCallbackDataEXT* d, void *u)
{
    fprintf(stderr, "%x %s\n",s, d->pMessage);
    return VK_FALSE;
}

XBOOL CKVkRasterizer::Start(WIN_HANDLE AppWnd)
{
    toggle_console(1);
    m_MainWindow = AppWnd;
    m_Init = TRUE;

    auto appinfo = make_vulkan_structure<VkApplicationInfo>();
    appinfo.pApplicationName = "Virtools Application";
    appinfo.applicationVersion = VK_MAKE_API_VERSION(0, 2, 1, 0);
    appinfo.pEngineName = "CK2";
    appinfo.engineVersion = VK_MAKE_API_VERSION(0, 2, 1, 0);
    appinfo.apiVersion = VK_API_VERSION_1_3;

    auto dmcinfo = make_vulkan_structure<VkDebugUtilsMessengerCreateInfoEXT>();
    dmcinfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dmcinfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    dmcinfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    dmcinfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    dmcinfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    dmcinfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    dmcinfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dmcinfo.pfnUserCallback = debug_callback;
    dmcinfo.pUserData = nullptr;

    uint32_t lcnt = 0;
    vkEnumerateInstanceLayerProperties(&lcnt, nullptr);
    std::vector<VkLayerProperties> availlayers(lcnt);
    vkEnumerateInstanceLayerProperties(&lcnt, availlayers.data());
    std::vector<char*> needed_layers = {"VK_LAYER_KHRONOS_validation"};
    for (auto layer : needed_layers)
    {
        if (std::none_of(availlayers.begin(), availlayers.end(),
            [layer](const VkLayerProperties& p) -> bool { return !strcmp(layer, p.layerName); }))
            return FALSE;
    }

    std::vector<char*> instext = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        //VK_KHR_DISPLAY_EXTENSION_NAME   //can't use this sh*t right now because Intel is so sh*t.
    };
    auto instcinfo = make_vulkan_structure<VkInstanceCreateInfo>();
    instcinfo.pApplicationInfo = &appinfo;
    instcinfo.enabledExtensionCount = instext.size();
    instcinfo.ppEnabledExtensionNames = instext.data();
    instcinfo.enabledLayerCount = needed_layers.size();
    instcinfo.ppEnabledLayerNames = needed_layers.data();
    instcinfo.pNext = &dmcinfo;
    if (VK_SUCCESS != vkCreateInstance(&instcinfo, nullptr, &vkinst))
        return FALSE;

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(vkinst, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT)
        vkCreateDebugUtilsMessengerEXT(vkinst, &dmcinfo, nullptr, &dbgmessenger);

    //temporary surface for device selection
    HWND sacw = CreateWindowA("", "vulkan surface test",
                              WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                              0, 0, 1, 1, nullptr, nullptr,
                              GetModuleHandle(NULL), NULL);
    auto surfacecinfo = make_vulkan_structure<VkWin32SurfaceCreateInfoKHR>();
    surfacecinfo.hwnd = sacw;
    surfacecinfo.hinstance = GetModuleHandle(NULL);
    VkSurfaceKHR sacs;
    if (VK_SUCCESS != vkCreateWin32SurfaceKHR(vkinst, &surfacecinfo, nullptr, &sacs))
        return FALSE;

    std::vector<char*> devext = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    uint32_t nphydev = 0;
    uint32_t selected_phydev = ~0U;
    vkEnumeratePhysicalDevices(vkinst, &nphydev, nullptr);
    std::vector<VkPhysicalDevice> phydevs(nphydev);
    vkEnumeratePhysicalDevices(vkinst, &nphydev, phydevs.data());
    auto select_qf = [sacs](const VkPhysicalDevice &phydev, uint32_t &gqidx, uint32_t &pqidx) -> bool {
        uint32_t nqf = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phydev, &nqf, nullptr);
        std::vector<VkQueueFamilyProperties> qf(nqf);
        vkGetPhysicalDeviceQueueFamilyProperties(phydev, &nqf, qf.data());
        gqidx = ~0U;
        pqidx = ~0U;
        for (size_t i = 0; i < qf.size(); ++i)
        {
            if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                gqidx = i;
                break;
            }
            VkBool32 pres = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(phydev, i, sacs, &pres);
            if (pres)
                pqidx = i;
        }
        return ~gqidx;
    };
    for (size_t i = 0; i < phydevs.size(); ++i)
    {
        //!!TODO: give priority to discrete graphics.
        std::set<std::string> dext(devext.begin(), devext.end());
        uint32_t dextpc = 0;
        vkEnumerateDeviceExtensionProperties(phydevs[i], nullptr, &dextpc, nullptr);
        std::vector<VkExtensionProperties> dextp(dextpc);
        vkEnumerateDeviceExtensionProperties(phydevs[i], nullptr, &dextpc, dextp.data());
        for (auto &e : dextp)
            dext.erase(std::string(e.extensionName));
        uint32_t gqidx = ~0U;
        uint32_t pqidx = ~0U;
        if (select_qf(phydevs[i], gqidx, pqidx) && dext.empty())
        {
            selected_phydev = i;
            break;
        }
    }
    if (!~selected_phydev)
    {
        vkDestroySurfaceKHR(vkinst, sacs, nullptr);
        DestroyWindow(sacw);
        return FALSE;
    }

    uint32_t gqidx = ~0U;
    uint32_t pqidx = ~0U;
    select_qf(phydevs[selected_phydev], gqidx, pqidx);
    auto qcinfo = make_vulkan_structure<VkDeviceQueueCreateInfo>();
    qcinfo.queueFamilyIndex = gqidx;
    qcinfo.queueCount = 1;
    float qprio = 1;
    qcinfo.pQueuePriorities = &qprio;

    VkPhysicalDeviceFeatures devfs{};
    auto devcinfo = make_vulkan_structure<VkDeviceCreateInfo>();
    devcinfo.queueCreateInfoCount = 1;
    devcinfo.pQueueCreateInfos = &qcinfo;
    devcinfo.pEnabledFeatures = &devfs;
    devcinfo.enabledExtensionCount = devext.size();
    devcinfo.ppEnabledExtensionNames = devext.data();
    devcinfo.enabledLayerCount = needed_layers.size();
    devcinfo.ppEnabledLayerNames = needed_layers.data();

    vkDestroySurfaceKHR(vkinst, sacs, nullptr);
    DestroyWindow(sacw);

    if (VK_SUCCESS != vkCreateDevice(phydevs[selected_phydev], &devcinfo, nullptr, &vkdev))
        return FALSE;

    DISPLAY_DEVICEA adapter;
    DISPLAY_DEVICEA display;
    MONITORINFOEXA mi;
    for (DWORD adapterIndex = 0; ; ++adapterIndex) {
        ZeroMemory(&adapter, sizeof(adapter));
        adapter.cb = sizeof(adapter);
        if (!EnumDisplayDevicesA(NULL, adapterIndex, &adapter, 0))
            return adapterIndex != 0;
        if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
            continue;
        if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
            continue;
        for (DWORD displayIndex = 0; ; ++displayIndex) {
            ZeroMemory(&display, sizeof(display));
            display.cb = sizeof(display);
            if (!EnumDisplayDevicesA(adapter.DeviceName, displayIndex, &display, 0))
                break;
            if (!(display.StateFlags & DISPLAY_DEVICE_ACTIVE))
                continue;
            ZeroMemory(&mi, sizeof(mi));
            mi.cbSize = sizeof(mi);
            EnumDisplayMonitors(NULL, NULL, monitorCallback, (LPARAM)&mi);
            char* name = adapter.DeviceString;
            if (!name)
                return FALSE;
            CKVkRasterizerDriver* driver = new CKVkRasterizerDriver(this);
            driver->m_Desc = adapter.DeviceName;
            driver->m_Desc << " @ ";
            driver->m_Adapter = adapter;
            driver->vkinst = vkinst;
            driver->vkphydev = phydevs[selected_phydev];
            driver->vkdev = vkdev;
            driver->gqidx = gqidx;
            driver->pqidx = pqidx;
            if (!driver->InitializeCaps())
            {
                delete driver;
                driver = nullptr;
            }
            if (driver)
                m_Drivers.PushBack(driver);
        }
    }
    return TRUE;
}


void CKVkRasterizer::Close(void)
{
    vkDestroyDevice(vkdev, nullptr);
    auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(vkinst, "vkDestroyDebugUtilsMessengerEXT");
    if (vkDestroyDebugUtilsMessengerEXT)
        vkDestroyDebugUtilsMessengerEXT(vkinst, dbgmessenger, nullptr);
    vkDestroyInstance(vkinst, nullptr);
}

CKRasterizer* CKVkRasterizerStart(WIN_HANDLE AppWnd) {
    CKRasterizer* rst = new CKVkRasterizer;
    if (!rst)
        return nullptr;
    if (!rst->Start(AppWnd))
    {
        delete rst;
        rst = nullptr;
    }
    return rst;
}

void CKVkRasterizerClose(CKRasterizer* rst)
{
    if (rst)
    {
        rst->Close();
        delete rst;
    }
}

PLUGIN_EXPORT void CKRasterizerGetInfo(CKRasterizerInfo* info)
{
    info->StartFct = CKVkRasterizerStart;
    info->CloseFct = CKVkRasterizerClose;
    info->Desc = "Vulkan Rasterizer";
}
