// Prepare the exact client paired with upstream Winlator app a030f552.
// The host server and Android UI integration remain separate; this is not a display pass.
import fs from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
const root='.tooling/vortek-source',out='artifacts/vortek';
const expected='eef892505c77fb6f3cc7bc78a151662ffb77bab934e5eeff8ead7270a6734c82';
const hash=b=>createHash('sha256').update(b).digest('hex');
if(!fs.existsSync('.tooling/vortek-source.tar.gz')){
  const response=await fetch('https://codeload.github.com/brunodev85/vortek/tar.gz/b1730c5def9b575672e671aee11d79ae7adc63d1');
  if(!response.ok)throw Error('Cannot fetch pinned Vortek source');
  const bytes=Buffer.from(await response.arrayBuffer());
  if(hash(bytes)!==expected)throw Error('Vortek archive integrity mismatch');
  fs.mkdirSync('.tooling',{recursive:true});fs.writeFileSync('.tooling/vortek-source.tar.gz',bytes);
}
const archive=fs.readFileSync('.tooling/vortek-source.tar.gz');
if(hash(archive)!==expected)throw Error('Unrecognized Vortek source archive');
fs.mkdirSync(root,{recursive:true});
execFileSync('tar',['-xf','.tooling/vortek-source.tar.gz','-C',root,'--strip-components=1']);
const server='app/src/main/cpp/vortekrenderer/include';
const normalized=file=>fs.readFileSync(file,'utf8').replaceAll('\r\n','\n');
const hostSerializer=normalized(`${server}/vortek_serializer.h`).replaceAll(
  'vt_hostImageLayout(*(VkImageLayout*)(inputBuffer + bufferOffset))','*(VkImageLayout*)(inputBuffer + bufferOffset)');
if(normalized(`${root}/include/vortek_serializer.h`)!==hostSerializer)
  throw Error('Client/server serializers differ; do not combine protocols');
if(hash(normalized(`${server}/vulkan/vulkan_core.h`))!=='c274cdac1e77755ad2c9e12a1d4d24a91e1f72dad138bfcd8b4a54279b9c2e71')
  throw Error('Unexpected paired Vulkan headers');
for(const directory of ['vulkan','vk_video']) fs.cpSync(`${server}/${directory}`,`${root}/include/${directory}`,{recursive:true});
const xlib=`${root}/include/vulkan/vulkan_xlib.h`,xlibHash='e8878c1efe85ce965d3e5019c1ca48043671e1a782804c4ec68eb460d45f7042';
if(!fs.existsSync(xlib)||hash(fs.readFileSync(xlib))!==xlibHash){
  const response=await fetch('https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/v1.3.302/include/vulkan/vulkan_xlib.h');
  if(!response.ok)throw Error('Cannot fetch pinned Khronos Xlib surface header');
  const bytes=Buffer.from(await response.arrayBuffer());if(hash(bytes)!==xlibHash)throw Error('Khronos header integrity mismatch');
  fs.writeFileSync(xlib,bytes);
}
const xcb=`${root}/include/vulkan/vulkan_xcb.h`,xcbHash='3d9c38165d2ffa24f585ae1b287ee89e32b54cc49bc28ac142f3ab92d00c8818';
if(!fs.existsSync(xcb)||hash(fs.readFileSync(xcb))!==xcbHash){
  const cached='.tooling/vulkan_xcb.h';
  let bytes=fs.existsSync(cached)?fs.readFileSync(cached):null;
  if(!bytes||hash(bytes)!==xcbHash){
    const response=await fetch('https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/v1.3.302/include/vulkan/vulkan_xcb.h');
    if(!response.ok)throw Error('Cannot fetch pinned Khronos XCB surface header');
    bytes=Buffer.from(await response.arrayBuffer());
  }
  if(hash(bytes)!==xcbHash)throw Error('Khronos XCB header integrity mismatch');
  fs.writeFileSync(xcb,bytes);
}
let header=fs.readFileSync(`${root}/include/vortek.h`,'utf8');
header=header.replace('#ifdef __ANDROID__\n#define VT_SERVER 1','#if defined(__ANDROID__) && !defined(DIMENSITY_VORTEK_CLIENT)\n#define VT_SERVER 1')
  .replace('#ifdef __ANDROID__\r\n#define VT_SERVER 1','#if defined(__ANDROID__) && !defined(DIMENSITY_VORTEK_CLIENT)\r\n#define VT_SERVER 1');
fs.writeFileSync(`${root}/include/vortek.h`,header);
let main=fs.readFileSync(`${root}/src/main.c`,'utf8');
const pathLine='    strncpy(server_addr.sun_path, VORTEK_SERVER_PATH, sizeof(server_addr.sun_path) - 1);';
const replacement=`    const char *path = getenv("VORTEK_SERVER_PATH");
    if (!path || path[0] != '/' || strlen(path) >= sizeof(server_addr.sun_path)) {
        close(fd);
        return -1;
    }
    memcpy(server_addr.sun_path, path, strlen(path) + 1);`;
if(main.includes(pathLine))main=main.replace(pathLine,replacement);
else if(!main.includes(replacement))throw Error('Unexpected Vortek connection source');
fs.writeFileSync(`${root}/src/main.c`,main);
// Surface handles carry the same X11 window id for Xlib and XCB; no new wire
// opcode or foreign XCB implementation is needed on the Android host.
let calls=fs.readFileSync(`${root}/src/vulkan_calls.c`,'utf8').replaceAll('\r\n','\n');
const submitWait='bool shouldWait = RingBuffer_hasStatus(clientRing, RING_STATUS_WAIT);';
if(calls.split(submitWait).length-1!==2)throw Error('Unexpected Vortek submit acknowledgement policy');
calls=calls.replaceAll(submitWait,'bool shouldWait = true; // Paired Dimensity server acknowledges every submission.');
// The original wire uses zero capacity for a count-only query. Handle an
// explicit non-NULL zero-capacity output locally so it cannot become a query.
for (const [name,count,items] of [
  ['vkGetPhysicalDeviceSurfaceFormatsKHR','pSurfaceFormatCount','pSurfaceFormats'],
  ['vkGetPhysicalDeviceSurfacePresentModesKHR','pPresentModeCount','pPresentModes'],
  ['vkGetSwapchainImagesKHR','pSwapchainImageCount','pSwapchainImages']
]) {
  const signature = new RegExp('(VkResult vt_call_'+name+'\\([^\\n]*\\) \{\\n)');
  if (!signature.test(calls)) throw Error('Unexpected enumeration entry: '+name);
  calls=calls.replace(signature,'$1    if ('+items+' && !*'+count+') return VK_INCOMPLETE;\n');
}

// Paired WSI replies: real per-swapchain results and explicit acquire index.
const acquireStatus = '    if (result >= 0 && result <= 10) {\n        *pImageIndex = result;\n        result = VK_SUCCESS;\n    }';
if (!calls.includes(acquireStatus)) throw Error('Unexpected acquire reply decoding');
calls=calls.replace(acquireStatus,'    if (result == VK_SUCCESS) memcpy(pImageIndex, inputBuffer, sizeof(*pImageIndex));');
const presentStart=calls.indexOf('VkResult vt_call_vkQueuePresentKHR(');
const presentEnd=calls.indexOf('\nVkResult vt_call_vkCreateXlibSurfaceKHR(',presentStart);
if (presentStart<0 || presentEnd<presentStart || !calls.slice(presentStart,presentEnd).includes('pPresentInfo->pResults[i] = VK_SUCCESS'))
  throw Error('Unexpected present acknowledgement implementation');
calls=calls.slice(0,presentStart)+
`VkResult vt_call_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    if (pPresentInfo->swapchainCount > CLIENT_RING_BUFFER_SIZE / sizeof(VkResult)) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VT_CALL_LOCK();
    VkPresentInfoKHR info = *pPresentInfo;
    info.pResults = NULL; // Output memory must not be serialized as input.
    VT_SERIALIZE_CMD(VkPresentInfoKHR, &info);
    VT_SEND_CHECKED(REQUEST_CODE_VK_QUEUE_PRESENT_KHR, VT_RETURN);
    char* inputBuffer = NULL;
    int replySize = 0;
    int result = vt_recv(clientRing, &inputBuffer, &replySize, &globalMemoryPool);
    if (result != VK_ERROR_DEVICE_LOST && replySize != pPresentInfo->swapchainCount * sizeof(VkResult)) {
        VT_CALL_UNLOCK();
        return VK_ERROR_DEVICE_LOST;
    }
    if (pPresentInfo->pResults && result != VK_ERROR_DEVICE_LOST)
        memcpy(pPresentInfo->pResults, inputBuffer, replySize);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}
`+calls.slice(presentEnd);

// SYNC_FD export transfers the fence payload; it is not an implementation of
// an ordinary Vulkan fence wait. Poll native state without holding the global
// IPC lock across the wait, so another thread can submit the signalling work.
calls='#include <time.h>\n'+calls;
const fenceStart=calls.indexOf('VkResult vt_call_vkWaitForFences(');
const fenceEnd=calls.indexOf('\nVkResult vt_call_vkCreateSemaphore(',fenceStart);
if (fenceStart<0 || fenceEnd<fenceStart || !calls.slice(fenceStart,fenceEnd).includes('recv_fds'))
  throw Error('Unexpected external fence wait path');
calls=calls.slice(0,fenceStart)+
`static uint64_t dgMonotonicNs(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000ull + now.tv_nsec;
}

VkResult vt_call_vkWaitForFences(VkDevice device, uint32_t count, const VkFence* fences, VkBool32 waitAll, uint64_t timeout) {
    uint64_t started = dgMonotonicNs();
    for (;;) {
        uint32_t signalled = 0;
        for (uint32_t i = 0; i < count; i++) {
            VkResult result = vt_call_vkGetFenceStatus(device, fences[i]);
            if (result == VK_SUCCESS) signalled++;
            else if (result != VK_NOT_READY) return result;
            if (!waitAll && signalled) return VK_SUCCESS;
        }
        if (waitAll && signalled == count) return VK_SUCCESS;
        uint64_t elapsed = dgMonotonicNs() - started;
        if (timeout == 0 || (timeout != UINT64_MAX && elapsed >= timeout)) return VK_TIMEOUT;
        uint64_t pause = timeout == UINT64_MAX ? 1000000 : MIN(1000000, timeout - elapsed);
        struct timespec delay = {0, (long)pause};
        nanosleep(&delay, NULL);
    }
}
`+calls.slice(fenceEnd);
const resetStart=calls.indexOf('VkResult vt_call_vkResetFences(');
const resetEnd=calls.indexOf('\nVkResult vt_call_vkGetFenceStatus(',resetStart);
if (resetStart<0 || resetEnd<resetStart) throw Error('Missing reset fences entry');
let reset=calls.slice(resetStart,resetEnd);
if (!reset.includes('return VK_SUCCESS;')) throw Error('Unexpected reset result policy');
reset=reset.replace('    VT_CALL_UNLOCK();','    VT_RECV_CHECKED(VT_RETURN);\n    VT_CALL_UNLOCK();')
  .replace('return VK_SUCCESS;','return (VkResult)result;');
calls=calls.slice(0,resetStart)+reset+calls.slice(resetEnd);

const supportMarker='VkResult vt_call_vkGetPhysicalDeviceSurfaceSupportKHR(';
if(!calls.includes(supportMarker))throw Error('Unexpected Vortek queue support entry');
calls=calls.replace(supportMarker,`static VkBool32 vt_supportsPresentation(VkPhysicalDevice device, uint32_t family) {
    uint32_t count = 0;
    vt_call_vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
    if (!count) return VK_FALSE;
    VkQueueFamilyProperties* properties = calloc(count, sizeof(VkQueueFamilyProperties));
    if (!properties) return VK_FALSE;
    vt_call_vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties);
    VkBool32 supported = VK_FALSE;
    for (uint32_t i = 0; i < count; i++) {
        if (properties[i].queueCount && (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            supported = family == i;
            break;
        }
    }
    free(properties);
    return supported;
}

`+supportMarker);
const supportAssignment='    *pSupported = VK_TRUE;\n    return VK_SUCCESS;\n}\n\nVkResult vt_call_vkGetPhysicalDeviceSurfaceCapabilitiesKHR';
if(!calls.includes(supportAssignment))throw Error('Unexpected Vortek surface support body');
calls=calls.replace(supportAssignment,supportAssignment.replace('VK_TRUE','vt_supportsPresentation(physicalDevice, queueFamilyIndex)'));
const xlibSupport='VkBool32 vt_call_vkGetPhysicalDeviceXlibPresentationSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, Display* dpy, VisualID visualID) {\n    return VK_TRUE;\n}';
if(!calls.includes(xlibSupport))throw Error('Unexpected Xlib presentation support');
calls=calls.replace(xlibSupport,xlibSupport.replace('return VK_TRUE','return vt_supportsPresentation(physicalDevice, queueFamilyIndex)'));
const marker='VkResult vt_call_vkCreateXlibSurfaceKHR(';
if(!calls.includes(marker))throw Error('Unexpected Vortek surface entry points');
calls=calls.replace(marker,`VkResult vt_call_vkCreateXcbSurfaceKHR(VkInstance instance, const VkXcbSurfaceCreateInfoKHR* info, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) {
    if (!info || !surface || !info->window) return VK_ERROR_SURFACE_LOST_KHR;
    VkObject* object = VkObject_create(VK_OBJECT_TYPE_SURFACE_KHR, (uint64_t)info->window);
    if (!object) return VK_ERROR_OUT_OF_HOST_MEMORY;
    *surface = VkObject_toHandle(object);
    return VK_SUCCESS;
}

VkBool32 vt_call_vkGetPhysicalDeviceXcbPresentationSupportKHR(VkPhysicalDevice device, uint32_t family, xcb_connection_t* connection, xcb_visualid_t visual) {
    return vt_supportsPresentation(device, family);
}

`+marker);
const entry='    {"vkCreateXlibSurfaceKHR", vt_call_vkCreateXlibSurfaceKHR},';
if(!calls.includes(entry))throw Error('Unexpected Vortek function table');
calls=calls.replace(entry,`    {"vkCreateXcbSurfaceKHR", vt_call_vkCreateXcbSurfaceKHR},
    {"vkGetPhysicalDeviceXcbPresentationSupportKHR", vt_call_vkGetPhysicalDeviceXcbPresentationSupportKHR},
`+entry);
fs.writeFileSync(`${root}/src/vulkan_calls.c`,calls);
// Only opaque Xlib ABI names used by Vulkan surface declarations; no Xlib calls.
fs.mkdirSync(`${root}/dimensity-abi/X11`,{recursive:true});
fs.writeFileSync(`${root}/dimensity-abi/X11/Xlib.h`,
  '#pragma once\ntypedef struct _XDisplay Display;\ntypedef unsigned long XID;\ntypedef XID Window;\ntypedef XID VisualID;\n');
fs.mkdirSync(`${root}/dimensity-abi/xcb`,{recursive:true});
fs.writeFileSync(`${root}/dimensity-abi/xcb/xcb.h`,
  '#pragma once\n#include <stdint.h>\ntypedef struct xcb_connection_t xcb_connection_t;\ntypedef uint32_t xcb_window_t;\ntypedef uint32_t xcb_visualid_t;\n');
fs.writeFileSync(`${root}/CMakeLists.txt`,`cmake_minimum_required(VERSION 3.22.1)
project(DimensityVortekClient C)
add_library(vulkan_vortek SHARED src/main.c src/vulkan_calls.c src/vk_object.c
  src/vk_object_pool.c src/arrays.c src/descriptor_update_template.c src/ring_buffer.c)
target_include_directories(vulkan_vortek PRIVATE dimensity-abi include)
target_compile_definitions(vulkan_vortek PRIVATE DIMENSITY_VORTEK_CLIENT VK_USE_PLATFORM_XLIB_KHR VK_USE_PLATFORM_XCB_KHR)
target_compile_options(vulkan_vortek PRIVATE -O2 -Wno-incompatible-pointer-types-discards-qualifiers)
target_link_libraries(vulkan_vortek log m)
target_link_options(vulkan_vortek PRIVATE -Wl,--no-undefined)
`);
fs.mkdirSync(out,{recursive:true});
fs.copyFileSync(`${root}/LICENSE`,`${out}/LICENSE`);
fs.writeFileSync(`${out}/source-lock.json`,JSON.stringify({
  repository:'https://github.com/brunodev85/vortek',commit:'b1730c5def9b575672e671aee11d79ae7adc63d1',
  archiveSha256:expected,pairedWinlatorAppCommit:'a030f552f452158a2db64fdb32b490fa19c0b48d',
  changes:['Explicit Android guest-client build role','Session-owned socket from environment; no other app path',
    'Xlib/XCB surface window-id bridge; opaque ABI declarations without Xlib/XCB functions',
    'Paired submit/present acknowledgements and explicit acquire image-index reply; request byte layouts unchanged'],
  preparedSha256:{header:hash(header),main:hash(main),calls:hash(calls)},
  protocol:'Dimensity reply v3 (WSI v2, reset-fence ack, native fence-status polling); use paired server',
  status:'optional per-game backend; offline build only, not validated on device'
},null,2));
console.log('Prepared pinned Android Vortek client; existing renderer unchanged.');
