// Single device-creation check before transferring large DX12 game fixtures.
// This does not render or claim game compatibility, and never forces a feature level.
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <stdio.h>

typedef HRESULT (WINAPI *CreateDeviceFn)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);

static void query_features(FILE *out, ID3D12Device *device) {
    const D3D_FEATURE_LEVEL requested[] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0
    };
    D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {sizeof(requested) / sizeof(requested[0]), requested, 0};
    HRESULT hr = ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels));
    fprintf(out, "feature_levels_hr=0x%08lx\n", (unsigned long)hr);
    if (SUCCEEDED(hr)) fprintf(out, "max_supported_feature_level=0x%x\n", levels.MaxSupportedFeatureLevel);

    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {0};
    hr = ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
    fprintf(out, "options_hr=0x%08lx\n", (unsigned long)hr);
    if (SUCCEEDED(hr)) {
        fprintf(out, "resource_binding_tier=%u tiled_resources_tier=%u typed_uav_additional_formats=%d output_merger_logic_op=%d\n",
            options.ResourceBindingTier, options.TiledResourcesTier,
            options.TypedUAVLoadAdditionalFormats, options.OutputMergerLogicOp);
    }
    D3D12_FEATURE_DATA_SHADER_MODEL model = {D3D_SHADER_MODEL_6_0};
    hr = ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_SHADER_MODEL, &model, sizeof(model));
    fprintf(out, "shader_model_hr=0x%08lx\n", (unsigned long)hr);
    if (SUCCEEDED(hr)) fprintf(out, "shader_model_for_request_6_0=0x%x\n", model.HighestShaderModel);
    fflush(out);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)instance; (void)previous; (void)command; (void)show;
    CreateDirectoryA("C:\\DGSmoke", NULL);
    FILE *out = fopen("C:\\DGSmoke\\d3d12-capability.txt", "w");
    if (!out) return 2;
    HMODULE module = LoadLibraryA("d3d12.dll");
    fprintf(out, "d3d12_load=%d winerror=%lu\n", module != NULL, module ? 0 : GetLastError());
    char feature_override[64] = {0};
    DWORD override_length = GetEnvironmentVariableA("VKD3D_FEATURE_LEVEL", feature_override, sizeof(feature_override));
    fprintf(out, "feature_level_override=%s\n", override_length == 0 ? "unset" :
        override_length < sizeof(feature_override) ? feature_override : "present (too long)");
    fflush(out);
    CreateDeviceFn create = module ? (CreateDeviceFn)(void *)GetProcAddress(module, "D3D12CreateDevice") : NULL;
    int result = 1;
    if (create) {
        ID3D12Device *device = NULL;
        HRESULT hr = create(NULL, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device, (void **)&device);
        fprintf(out, "d3d12_create_fl12_0=0x%08lx\n", (unsigned long)hr);
        fflush(out);
        if (SUCCEEDED(hr) && device) result = 0;
        fprintf(out, "required_fl12_0_supported=%d\n", result == 0);
        if (result != 0) {
            if (device) ID3D12Device_Release(device);
            device = NULL;
            // Inspect a lower-level device to explain missing capabilities; it is not a pass.
            hr = create(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&device);
            fprintf(out, "d3d12_create_fl11_0_diagnostic=0x%08lx\n", (unsigned long)hr);
        }
        if (SUCCEEDED(hr) && device) query_features(out, device);
        if (device) ID3D12Device_Release(device);
    }
    fprintf(out, "complete=1 device_creation_only=1\n");
    fclose(out);
    if (module) FreeLibrary(module);
    return result;
}
