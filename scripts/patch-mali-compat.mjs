// Reviewed local fixes over the pinned upstream tree. Verify both fresh and patched inputs.
import fs from 'node:fs';
import crypto from 'node:crypto';
const root = '.tooling/mali-compat-source';
const lock = JSON.parse(fs.readFileSync(`${root}/dimensity-source-lock.json`, 'utf8'));
const edits = {
  'src/pipelines.cpp': [
    ['#include <cstdint>', '#include <cstdint>\n#include <cstdio>\n#include <cstdlib>\n#include <unistd.h>'],
    ['void DumpDisassembledSpirv(', `// Opt-in last input per native thread, saved before driver code can fault.
// No allocation or exception is introduced into the Vulkan C entry point.
static void CaptureShaderInput(const VkShaderModuleCreateInfo *info) {
    static const char *directory = std::getenv("COMPAT_CAPTURE_SHADER_INPUTS");
    if (!directory || !*directory || !info || !info->pCode ||
        info->codeSize == 0 || info->codeSize > 16 * 1024 * 1024) return;
    char path[4096];
    int length = std::snprintf(path, sizeof(path), "%s/last-input-%d-%d.spv",
                               directory, static_cast<int>(getpid()),
                               static_cast<int>(gettid()));
    if (length < 0 || static_cast<size_t>(length) >= sizeof(path)) return;
    FILE *output = std::fopen(path, "wb");
    if (!output) return;
    size_t written = std::fwrite(info->pCode, 1, info->codeSize, output);
    std::fclose(output);
    if (written != info->codeSize)
        Logger::log("error", "Incomplete shader input capture: %zu/%zu", written, info->codeSize);
}

void DumpDisassembledSpirv(`],
    ['    VkResult result = dev->table.CreateShaderModule(device, pCreateInfo,',
     '    CaptureShaderInput(pCreateInfo);\n    VkResult result = dev->table.CreateShaderModule(device, pCreateInfo,'],
  ],
  'src/sparse_binding.hpp': [
    ['    VkDeviceSize pageGranularity = 0;', '    VkDeviceSize committedSize = 0; // actual allocation charged to sparse budget\n    VkDeviceSize pageGranularity = 0;'],
  ],
  'src/sparse_binding.cpp': [
    ['    DxvkMaliCompatLayer_BindBufferMemory(dev->handle, buf->handle, shadowMemory,\n                                         0);',
     `    // The buffer is not registered in buffersMap until construction succeeds.
    result = dev->table.BindBufferMemory(dev->handle, buf->handle, shadowMemory, 0);
    if (result != VK_SUCCESS) {
        dev->table.DestroyBuffer(dev->handle, buf->handle, pAllocator);
        dev->table.FreeMemory(dev->handle, shadowMemory, pAllocator);
        ReleaseBudget(dev, reqs.size);
        Logger::log("error", "sparse_binding: shadow buffer bind failed: %d", result);
        return result;
    }
    buf->memory = shadowMemory;
    buf->offset = 0;`],
    ['    res->virtualSize = pCreateInfo->size;', '    res->virtualSize = pCreateInfo->size;\n    res->committedSize = reqs.size;'],
    ['    dev->table.BindImageMemory(dev->handle, img->handle, shadowMemory, 0);',
     `    result = dev->table.BindImageMemory(dev->handle, img->handle, shadowMemory, 0);
    if (result != VK_SUCCESS) {
        dev->table.DestroyImage(dev->handle, img->handle, pAllocator);
        dev->table.FreeMemory(dev->handle, shadowMemory, pAllocator);
        ReleaseBudget(dev, reqs.size);
        Logger::log("error", "sparse_binding: shadow image bind failed: %d", result);
        return result;
    }`],
    ['    res->virtualSize = reqs.size;', '    res->virtualSize = reqs.size;\n    res->committedSize = reqs.size;'],
    ['    ReleaseBudget(dev, res->virtualSize);', '    ReleaseBudget(dev, res->committedSize);', 2],
  ],
};
const blob = text => crypto.createHash('sha1').update(`blob ${Buffer.byteLength(text)}\0`).update(text).digest('hex');
const sha256 = text => crypto.createHash('sha256').update(text).digest('hex');
const modified = [];
for (const [file, changes] of Object.entries(edits)) {
  const current = fs.readFileSync(`${root}/${file}`, 'utf8');
  let original = current;
  // Reverse only this exact patch before checking the immutable upstream blob.
  for (const [before, after] of [...changes].reverse()) if (original.includes(after)) original = original.replaceAll(after, before);
  const entry = lock.files.find(entry => entry.path === file);
  if (!entry || blob(original) !== entry.sha) throw new Error(`Unexpected upstream source: ${file}`);
  let patched = original;
  for (const [before, after, count = 1] of changes) {
    if (patched.split(before).length - 1 !== count) throw new Error(`Patch context mismatch: ${file}`);
    patched = patched.replaceAll(before, after);
  }
  if (patched !== current) fs.writeFileSync(`${root}/${file}`, patched);
  modified.push({path:file, upstreamGitBlob:entry.sha, patchedSha256:sha256(patched)});
}
const manifest = {...lock, localPatches:[{name:'sparse-allocation-and-opt-in-shader-capture',
  scriptSha256:sha256(fs.readFileSync('scripts/patch-mali-compat.mjs')), files:modified}]};
fs.writeFileSync(`${root}/dimensity-patched-source-lock.json`, JSON.stringify(manifest, null, 2)+'\n');
console.log(`Verified local sparse fixes and opt-in shader capture: ${modified.length} source files.`);
