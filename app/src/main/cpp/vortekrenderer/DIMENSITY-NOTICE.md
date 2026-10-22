# Vortek Android server port

Imported from brunodev85/winlator app submodule commit
`a030f552f452158a2db64fdb32b490fa19c0b48d`, paired with Vortek client
`b1730c5def9b575672e671aee11d79ae7adc63d1` (LGPL-2.1; see LICENSE).
The serializer must match the pinned client. Vulkan headers are the paired
Khronos 1.3.302 headers and retain their original notices.

Local changes: JNI namespace for this app; system Mali Vulkan only; checked
initialization; cache directory supplied by Android Context; bounded cache
paths; error cleanup during AHB image creation. This backend is experimental
and does not establish game compatibility. No system driver is installed.

The paired client now waits for acknowledgement of every QueueSubmit/2;
request serialization is unchanged, but the reply policy is a local protocol
extension. Both halves must be used together. CPU BC1-5 uploads are recorded
inside the original command buffer, retaining its barriers and ordering;
staging lives until command reset/free, with a 256 MiB allocation limit.
Padded rows, array layers, mip extents and cache sizes are checked. BC4 worker
offsets and all-required memory property selection are corrected. Core device
features remain native; this does not implement BC6/7, GPU-generated compressed
uploads, BC encoding/readback, block-texel reinterpretation, or full BC support.

Reply protocol v3 adds real present/reset-fence results and an explicit acquire
index. Native fence status polling replaces destructive SYNC_FD export waits.
Outputs and ownership commands are separate per swapchain image (2–3 images);
import uses queried driver AHB usage and exact RGBA8_UNORM. Real Warzone first
level rendering was observed; full gameplay and AAA compatibility remain open.

Reply protocol v4 serializes padded VkMemoryHeap arrays element by element
and transfers complete VkDeviceSize memory-budget arrays. Preparation uses
a shared deterministic patch; both client and server must be updated together.

Offline memory-path corrections preserve allocation flags and real resource
dedication, validate imported AHB/DMA-BUF memory types, account the queried
AHB size, and preserve native errors. AHB BLOB imports use GPU_DATA_BUFFER.
The guest stores 64-bit allocation sizes, aligns mmap ranges to OS pages,
subtracts offsets for VK_WHOLE_SIZE, returns placed pointers, and releases
fds/mappings on failure/free. These changes do not prove game rendering.

Shader sources remain owned and immutable for their lifetime. Each graphics
pipeline gets private converted stage copies, including stage-order independent
vertex/fragment inspection; temporary native modules live through pipeline
creation and are then destroyed. Inspection failures stop driver pipeline
creation and propagate as errors. SPIR-V parsing checks operand/word bounds
and destination string capacity. These are bounds checks, not full validation.
Pipeline replies use exact stream reads/writes, bounded chunks, no SIGPIPE
on a closed result socket, and native null pipeline handles remain null.

The first guest map now establishes a persistent native vkMapMemory mapping
for that allocation; forwarded flush/invalidate calls no longer target an
unmapped VkDeviceMemory. Free unmaps it first. Native map errors are returned
without an invalid SCM_RIGHTS fd. Cross-process noncoherent cache behavior
still requires real-device verification; no coherence feature is fabricated.
