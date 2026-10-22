package com.winlator.cmod.dimensity;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.*;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.*;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.view.*;
import android.widget.*;
import androidx.fragment.app.Fragment;
import com.winlator.cmod.*;
import com.winlator.cmod.container.*;
import com.winlator.cmod.core.*;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.zip.*;

/** Tablet landing page. Imports copy only user-selected documents into private C:. */
public final class DimensityFragment extends Fragment {
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final Handler main = new Handler(Looper.getMainLooper());
    private TextView status;
    private LinearLayout column;
    private int importContainerId;
    private Button cancelImport;
    private volatile ImportProgress activeImport;
    private static final long IMPORT_RESERVE = 256L * 1024 * 1024;
    private static final int EXE = 71, TREE = 72, EXPORT = 73;

    private static final class ImportProgress {
        volatile boolean cancelled;
        long bytes, lastUpdate;
        int entries, files;
        void checkCancelled() throws InterruptedIOException {
            if (cancelled) throw new InterruptedIOException("导入已取消");
        }
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        if (state != null) importContainerId = state.getInt("importContainerId");
    }
    @Override public void onSaveInstanceState(Bundle state) {
        state.putInt("importContainerId", importContainerId);
        super.onSaveInstanceState(state);
    }
    @Override public View onCreateView(LayoutInflater inflater, ViewGroup parent, Bundle state) {
        requireActivity().setTitle("Dimensity Gaming");
        ScrollView scroll = new ScrollView(requireContext());
        column = new LinearLayout(requireContext());
        column.setOrientation(LinearLayout.VERTICAL);
        int p = (int)(24 * getResources().getDisplayMetrics().density);
        column.setPadding(p, p, p, p);
        scroll.setBackgroundColor(Color.rgb(16, 23, 33));
        scroll.addView(column);
        label("DIMENSITY GAMING", 28);
        label("面向 8GB / Mali 的本地 Windows 游戏运行器 · " + BuildConfig.VERSION_NAME, 17);
        label("首次解包完成后：创建容器 → 导入完整游戏目录 → 游戏库启动。每款游戏建议独立容器。", 15);
        button("容器与 Wine 桌面", () -> navigate(new ContainersFragment()));
        button("Windows 游戏库", () -> navigate(new ShortcutsFragment()));
        button("导入 EXE（单文件程序）", () -> chooseContainer(c -> pick(c, EXE)));
        button("导入游戏目录（保留 DLL 与数据文件）", () -> chooseContainer(c -> pick(c, TREE)));
        button("添加容器内已有游戏（无需重新复制）", () -> chooseContainer(c ->
                com.winlator.cmod.contentdialog.DirectLaunchDialog.show(requireContext(), c, null,
                        () -> message("已加入游戏库，可在「直接启动设置」中修改 EXE、工作目录与自动启动。"))));
        cancelImport = new Button(requireContext());
        cancelImport.setText("取消当前导入");
        cancelImport.setVisibility(activeImport == null ? View.GONE : View.VISIBLE);
        cancelImport.setOnClickListener(v -> {
            ImportProgress progress = activeImport;
            if (progress != null) {
                progress.cancelled = true;
                cancelImport.setEnabled(false);
                message("正在取消，已复制文件会保留在此容器 C:/Games 中。");
            }
        });
        column.addView(cancelImport);
        button("应用设备预设 / 恢复均衡默认", () -> chooseContainer(this::profile));
        button("运行 Wine 基础程序 winecfg.exe", () -> chooseContainer(c -> {
            try {
                File shortcut = shortcut(c, "Wine 配置", "C:/windows/system32/winecfg.exe");
                startActivity(new Intent(requireContext(), XServerDisplayActivity.class).putExtra("container_id", c.id)
                        .putExtra("shortcut_path", shortcut.getPath()).putExtra("shortcut_name", "Wine 配置"));
            } catch (Exception e) { message(e.toString()); }
        }));
        button("检测真实 GPU / Vulkan / 内存", this::diagnose);
        button("导出本次诊断 ZIP", () -> startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT)
                .setType("application/zip").addCategory(Intent.CATEGORY_OPENABLE).putExtra(Intent.EXTRA_TITLE, "dimensity-diagnostics.zip"), EXPORT));
        status = label("尚未检测设备。实机游戏兼容性与性能：未验证。", 14);
        status.setTextIsSelectable(true);
        label("预设不会自动改写游戏内画质；请在游戏中设低纹理、低阴影。DirectX 12 暂不作为默认路径。", 14);
        return scroll;
    }
    private TextView label(String text, int size) {
        TextView v = new TextView(requireContext());
        v.setText(text); v.setTextSize(size); v.setTextColor(Color.rgb(222, 233, 244));
        v.setPadding(0, 8, 0, 12); column.addView(v); return v;
    }
    private void button(String text, Runnable action) {
        Button b = new Button(requireContext()); b.setText(text); b.setAllCaps(false);
        b.setOnClickListener(v -> action.run()); column.addView(b);
    }
    private void navigate(Fragment fragment) {
        getParentFragmentManager().beginTransaction().replace(R.id.FLFragmentContainer, fragment).addToBackStack(null).commit();
    }
    private void message(String text) { if (status != null) status.setText(text); }
    private void chooseContainer(java.util.function.Consumer<Container> action) {
        ArrayList<Container> containers = new ContainerManager(requireContext()).getContainers();
        if (containers.isEmpty()) { message("请先到「容器与 Wine 桌面」创建一个容器，等待运行环境解包完成。"); return; }
        new AlertDialog.Builder(requireContext()).setTitle("选择游戏容器")
                .setItems(containers.stream().map(Container::getName).toArray(String[]::new), (d, i) -> action.accept(containers.get(i))).show();
    }
    private void profile(Container c) {
        int[] selected = {1};
        new AlertDialog.Builder(requireContext()).setTitle("为 " + c.getName() + " 保存预设")
                .setSingleChoiceItems(DeviceProfiles.NAMES, 1, (d, i) -> selected[0] = i)
                .setPositiveButton("应用", (d, i) -> {
                    DeviceProfiles.apply(c, selected[0]);
                    message(DeviceProfiles.DETAILS[selected[0]] + "\n分辨率、帧率、DXVK/Wrapper 与转译器设置已保存。已有快捷方式的单独覆盖设置需在游戏设置中恢复默认。");
                }).setNegativeButton("取消", null).show();
    }
    private void pick(Container c, int request) {
        if (activeImport != null) { message("已有导入任务，请等待完成或取消。"); return; }
        importContainerId = c.id;
        Intent intent = new Intent(request == TREE ? Intent.ACTION_OPEN_DOCUMENT_TREE : Intent.ACTION_OPEN_DOCUMENT);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        if (request == EXE) intent.addCategory(Intent.CATEGORY_OPENABLE).setType("*/*");
        startActivityForResult(intent, request);
    }
    @Override public void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (result != Activity.RESULT_OK || data == null || data.getData() == null) return;
        final Context context = requireContext().getApplicationContext();
        final Uri uri = data.getData();
        final int containerId = importContainerId;
        if (request == EXPORT) {
            worker.execute(() -> {
                try (OutputStream stream = context.getContentResolver().openOutputStream(uri);
                     ZipOutputStream zip = new ZipOutputStream(stream)) {
                    File[] files = new File(context.getFilesDir(), "diagnostics").listFiles(File::isFile);
                    if (files != null) for (File file : files) {
                        zip.putNextEntry(new ZipEntry(file.getName()));
                        try (InputStream in = new FileInputStream(file)) { pipe(in, zip); }
                        zip.closeEntry();
                    }
                    post("诊断 ZIP 已导出。");
                } catch (Exception e) { post("导出失败：" + e); }
            }); return;
        }
        if (request != EXE && request != TREE) return;
        if (activeImport != null) { message("已有导入任务，请等待完成或取消。"); return; }
        final ImportProgress progress = new ImportProgress();
        activeImport = progress;
        cancelImport.setEnabled(true);
        cancelImport.setVisibility(View.VISIBLE);
        message("正在复制到应用私有游戏目录。将显示实际复制量，每个文件写入前检查剩余空间。");
        worker.execute(() -> {
            File destination = null;
            try {
                Container c = new ContainerManager(context).getContainerById(containerId);
                if (c == null) throw new IOException("容器不存在");
                destination = new File(c.getRootDir(), ".wine/drive_c/Games/import-" + UUID.randomUUID().toString().substring(0, 8));
                if (!destination.mkdirs()) throw new IOException("无法创建私有游戏目录");
                ArrayList<File> executables = new ArrayList<>();
                Uri document = request == TREE ? DocumentsContract.buildDocumentUriUsingTree(uri, DocumentsContract.getTreeDocumentId(uri)) : uri;
                copy(context, document, destination, executables, progress, 0);
                final File importedRoot = destination;
                main.post(() -> {
                    if (!isAdded()) return;
                    if (executables.isEmpty()) { message("目录已复制，未找到 EXE。可在 Wine 桌面的 C:/Games 中查看。"); return; }
                    message(String.format(Locale.ROOT, "已复制 %d 个文件，共 %.1f MiB。请选择主 EXE。", progress.files, progress.bytes / 1048576.0));
                    String[] names = executables.stream().map(f -> importedRoot.toURI().relativize(f.toURI()).getPath()).toArray(String[]::new);
                    new AlertDialog.Builder(requireContext()).setTitle("选择游戏主 EXE 创建快捷方式")
                            .setItems(names, (d, i) -> {
                                try {
                                    String relative = new File(c.getRootDir(), ".wine/drive_c").toURI().relativize(executables.get(i).toURI()).getPath();
                                    shortcut(c, executables.get(i).getName(), "C:/" + relative);
                                    message("已加入 Windows 游戏库。请从游戏库启动并保存该游戏的设置。");
                                } catch (Exception e) { message("快捷方式创建失败：" + e); }
                            }).setNegativeButton("稍后", null).show();
                });
            } catch (Exception e) {
                String location = destination == null ? "C:/Games" : "C:/Games/" + destination.getName();
                post((progress.cancelled ? "导入已取消。" : "导入失败：" + e) + "\n部分文件保留在 " + location + "，未完成的文件带 .part 后缀。");
            } finally {
                activeImport = null;
                main.post(() -> { if (cancelImport != null && activeImport == null) cancelImport.setVisibility(View.GONE); });
            }
        });
    }
    private void copy(Context context, Uri uri, File target, List<File> exes, ImportProgress progress, int depth) throws Exception {
        progress.checkCancelled();
        if (depth > 48 || ++progress.entries > 50000) throw new IOException("目录层数或文件数量超出限制");
        String name = null, mime = null;
        long expectedSize = -1;
        try (Cursor cursor = context.getContentResolver().query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int n = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME), m = cursor.getColumnIndex(DocumentsContract.Document.COLUMN_MIME_TYPE);
                if (n >= 0) name = cursor.getString(n);
                if (m >= 0) mime = cursor.getString(m);
                int s = cursor.getColumnIndex(OpenableColumns.SIZE);
                if (s >= 0 && !cursor.isNull(s)) expectedSize = cursor.getLong(s);
            }
        }
        if (name == null || name.equals(".") || name.equals("..") || name.matches(".*[\\\\/\\p{Cntrl}].*")) throw new IOException("不安全的文件名");
        File file = new File(target, name.replaceAll("[:*?\"<>|]", "_"));
        if (file.exists()) throw new IOException("文件名冲突：" + file.getName());
        if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
            if (!file.mkdir()) throw new IOException("目录重名或创建失败");
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(uri, DocumentsContract.getDocumentId(uri));
            try (Cursor cursor = context.getContentResolver().query(children, new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID}, null, null, null)) {
                if (cursor == null) throw new IOException("目录不可读");
                while (cursor.moveToNext()) copy(context, DocumentsContract.buildDocumentUriUsingTree(uri, cursor.getString(0)), file, exes, progress, depth + 1);
            }
        } else {
            File part = new File(file.getPath() + ".part");
            long available = Math.max(0, target.getUsableSpace() - IMPORT_RESERVE);
            if (expectedSize > available || target.getUsableSpace() < IMPORT_RESERVE)
                throw new IOException("空间不足以复制 " + name + "，需保留 256 MiB 余量");
            if (!part.createNewFile()) throw new IOException("文件重名");
            long copied = 0, untilSpaceCheck = 0;
            try (InputStream in = context.getContentResolver().openInputStream(uri); OutputStream out = new FileOutputStream(part)) {
                if (in == null) throw new IOException("文件不可读：" + name);
                byte[] buffer = new byte[256 * 1024]; int read;
                while ((read = in.read(buffer)) != -1) {
                    progress.checkCancelled();
                    if (untilSpaceCheck <= 0) {
                        if (target.getUsableSpace() < IMPORT_RESERVE + buffer.length) throw new IOException("存储空间不足，保留 256 MiB 余量");
                        untilSpaceCheck = 8L * 1024 * 1024;
                    }
                    out.write(buffer, 0, read);
                    copied += read;
                    progress.bytes += read;
                    untilSpaceCheck -= read;
                    long now = SystemClock.elapsedRealtime();
                    if (now - progress.lastUpdate >= 500) {
                        progress.lastUpdate = now;
                        String text = String.format(Locale.ROOT, "已复制 %.1f MiB / %d 个完整文件\n%s：%.1f MiB%s", progress.bytes / 1048576.0, progress.files, name, copied / 1048576.0,
                                expectedSize >= 0 ? String.format(Locale.ROOT, " / %.1f MiB", expectedSize / 1048576.0) : "");
                        main.post(() -> { if (isAdded() && getView() != null && activeImport == progress && !progress.cancelled) message(text); });
                    }
                }
            }
            progress.checkCancelled();
            if (expectedSize >= 0 && copied != expectedSize) throw new IOException("源文件大小发生变化或读取不完整：" + name);
            if (file.exists() || !part.renameTo(file)) throw new IOException("文件提交失败");
            progress.files++;
            if (file.getName().toLowerCase(Locale.ROOT).endsWith(".exe") && exes.size() < 256) exes.add(file);
        }
    }
    private static File shortcut(Container c, String name, String windowsPath) throws IOException {
        c.getDesktopDir().mkdirs();
        File file = new File(c.getDesktopDir(), "DG-" + name.replaceAll("[\\\\/:*?\"<>|\\p{Cntrl}]", "_") + ".desktop");
        if (file.exists()) file = new File(c.getDesktopDir(), "DG-" + UUID.randomUUID() + ".desktop");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            out.println("[Desktop Entry]\nType=Application\nName=" + name);
            out.println("Exec=wine " + StringUtils.escapeFileDOSPath(windowsPath));
            out.println("\n[Extra Data]\ncontainer_id=" + c.id);
            out.println("executablePath=" + windowsPath);
        }
        return file;
    }
    private void diagnose() {
        Context context = requireContext().getApplicationContext();
        message("正在读取真实驱动能力……");
        worker.execute(() -> {
            try {
                JSONObject report = new JSONObject(VulkanProbe.inspect());
                android.app.ActivityManager.MemoryInfo memory = MemoryMonitor.systemMemory(context);
                report.put("device", Build.MANUFACTURER + " " + Build.MODEL).put("android", Build.VERSION.RELEASE)
                        .put("total_memory_bytes", memory.totalMem).put("available_memory_bytes", memory.availMem).put("low_memory", memory.lowMemory);
                if (Build.VERSION.SDK_INT >= 30) {
                    JSONArray exits = new JSONArray();
                    android.app.ActivityManager am = (android.app.ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
                    for (android.app.ApplicationExitInfo exit : am.getHistoricalProcessExitReasons(context.getPackageName(), 0, 5))
                        exits.put(new JSONObject().put("reason", exit.getReason()).put("timestamp", exit.getTimestamp()).put("pss_kib", exit.getPss()));
                    report.put("recent_android_exit_reasons", exits);
                }
                File dir = new File(context.getFilesDir(), "diagnostics"); dir.mkdirs();
                try (Writer out = new OutputStreamWriter(new FileOutputStream(new File(dir, "vulkan.json")), StandardCharsets.UTF_8)) { out.write(report.toString(2)); }
                post(report.toString(2));
            } catch (Throwable e) { post("检测 unavailable：" + e); }
        });
    }
    private void post(String text) { main.post(() -> { if (isAdded() && getView() != null) message(text); }); }
    private static void pipe(InputStream in, OutputStream out) throws IOException {
        byte[] bytes = new byte[65536]; int n; while ((n = in.read(bytes)) != -1) out.write(bytes, 0, n);
    }
    @Override public void onDestroy() {
        ImportProgress progress = activeImport;
        if (progress != null) progress.cancelled = true;
        worker.shutdown(); super.onDestroy();
    }
}
