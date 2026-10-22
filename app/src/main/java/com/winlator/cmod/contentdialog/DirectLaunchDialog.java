package com.winlator.cmod.contentdialog;

import android.app.AlertDialog;
import android.content.Context;
import android.text.InputType;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import android.widget.TextView;
import com.winlator.cmod.container.Container;
import com.winlator.cmod.container.Shortcut;
import com.winlator.cmod.dimensity.GameLaunches;
import com.winlator.cmod.core.WindowsLaunchCommand;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.UUID;
import java.util.ArrayList;
import com.winlator.cmod.inputcontrols.ControlsProfile;
import com.winlator.cmod.inputcontrols.InputControlsManager;

/** The same local EXE launch profile is used by the library, home shortcut and auto-start. */
public final class DirectLaunchDialog {
    public static void show(Context context, Container container, Shortcut shortcut, Runnable saved) {
        LinearLayout fields = new LinearLayout(context);
        fields.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (20 * context.getResources().getDisplayMetrics().density);
        fields.setPadding(pad, pad / 2, pad, pad / 2);
        ScrollView scroll = new ScrollView(context);
        scroll.addView(fields);
        WindowsLaunchCommand original = null;
        try { if (shortcut != null) original = GameLaunches.command(shortcut); }
        catch (IllegalArgumentException ignored) { /* Keep an invalid saved profile editable. */ }
        EditText executable = field(context, fields, "游戏 EXE 路径", original == null
                ? (shortcut == null ? "C:/Games/" : shortcut.path) : original.executable);
        EditText directory = field(context, fields, "工作目录（留空使用 EXE 所在目录）", shortcut == null ? "" : shortcut.getExtra("workingDirectory"));
        EditText arguments = field(context, fields, "启动参数（可留空）", original == null ? "" : original.arguments);
        TextView controlsLabel = new TextView(context);
        controlsLabel.setText("手柄与触屏配置");
        fields.addView(controlsLabel);
        ArrayList<String> profileIds = new ArrayList<>(), profileNames = new ArrayList<>();
        profileIds.add("0");
        profileNames.add("原生手柄（不转换为键鼠）");
        String currentProfile = shortcut == null ? "" : shortcut.getExtra("controlsProfile");
        if (currentProfile.isEmpty()) currentProfile = "0";
        for (ControlsProfile profile : new InputControlsManager(context).getProfiles(true)) {
            profileIds.add(Integer.toString(profile.id));
            profileNames.add(profile.getName());
        }
        if (!profileIds.contains(currentProfile)) {
            profileIds.add(currentProfile);
            profileNames.add("保留现有配置（" + currentProfile + "）");
        }
        Spinner controls = new Spinner(context);
        ArrayAdapter<String> profiles = new ArrayAdapter<>(context, android.R.layout.simple_spinner_item, profileNames);
        profiles.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        controls.setAdapter(profiles);
        controls.setSelection(profileIds.indexOf(currentProfile));
        fields.addView(controls);
        CheckBox adapterIdentity = new CheckBox(context);
        adapterIdentity.setText("兼容旧启动器的显卡识别（DXVK）");
        adapterIdentity.setChecked(shortcut != null && com.winlator.cmod.dimensity.AdapterIdentity.enabled(
                shortcut.getExtra(com.winlator.cmod.dimensity.AdapterIdentity.EXTRA)));
        fields.addView(adapterIdentity);
        CheckBox identityScope = new CheckBox(context);
        identityScope.setText("仅对当前 EXE 生效（子程序保留真实显卡识别）");
        identityScope.setChecked(shortcut != null && com.winlator.cmod.dimensity.AdapterIdentity.NVIDIA_MAIN
                .equals(shortcut.getExtra(com.winlator.cmod.dimensity.AdapterIdentity.EXTRA)));
        identityScope.setEnabled(adapterIdentity.isChecked());
        adapterIdentity.setOnCheckedChangeListener((button, checked) -> identityScope.setEnabled(checked));
        fields.addView(identityScope);
        CheckBox vortek = new CheckBox(context);
        vortek.setText("实验性 Mali 显示后端（Vortek）");
        vortek.setChecked(com.winlator.cmod.core.VortekBackend.selected(shortcut));
        fields.addView(vortek);
        CheckBox automatic = new CheckBox(context);
        automatic.setText("打开 Dimensity Gaming 后自动启动此游戏");
        automatic.setChecked(shortcut != null && GameLaunches.isAutomatic(context, shortcut));
        fields.addView(automatic);
        TextView hint = new TextView(context);
        hint.setText("直接运行选定的 EXE。自动启动前可取消；退出游戏返回游戏库。请保留完整 DLL 和资源文件。显卡识别兼容仅改变标识，不增加实际图形能力。");
        fields.addView(hint);
        AlertDialog dialog = new AlertDialog.Builder(context).setTitle("直接启动设置")
                .setView(scroll).setPositiveButton("保存", null).setNegativeButton("取消", null).create();
        dialog.setOnShowListener(d -> dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
            try {
                WindowsLaunchCommand command = new WindowsLaunchCommand(executable.getText().toString(),
                        arguments.getText().toString(), directory.getText().toString());
                if (!command.executable.toLowerCase(java.util.Locale.ROOT).matches(".*\\.(exe|lnk|bat|cmd)$"))
                    throw new IllegalArgumentException("请选择 EXE、LNK、BAT 或 CMD 文件");
                if (adapterIdentity.isChecked() && identityScope.isChecked() &&
                        com.winlator.cmod.dimensity.AdapterIdentity.executableName(command.executable) == null)
                    throw new IllegalArgumentException("此识别范围需要 EXE，且文件名不能包含 [、] 或分号");
                File target = resolve(container, command.executable);
                if (target == null || !target.isFile()) throw new IllegalArgumentException("游戏文件不存在，请先导入完整目录并检查路径");
                if (!command.directory.isEmpty()) {
                    File working = resolve(container, command.directory);
                    if (working == null || !working.isDirectory()) throw new IllegalArgumentException("工作目录不存在");
                }
                Shortcut result = shortcut;
                if (result == null) {
                    container.getDesktopDir().mkdirs();
                    String name = target.getName().replaceAll("[\\\\/:*?\"<>|\\p{Cntrl}]", "_");
                    File file = new File(container.getDesktopDir(), "DG-" + name + ".desktop");
                    if (file.exists()) file = new File(container.getDesktopDir(), "DG-" + UUID.randomUUID() + ".desktop");
                    String data = "[Desktop Entry]\nType=Application\nName=" + name + "\nExec=wine \"" + command.executable + "\"\n\n[Extra Data]\ncontainer_id=" + container.id + "\n";
                    Files.write(file.toPath(), data.getBytes(StandardCharsets.UTF_8));
                    result = new Shortcut(container, file);
                }
                result.putExtra("executablePath", command.executable);
                result.putExtra("execArgs", command.arguments.isEmpty() ? null : command.arguments);
                result.putExtra("workingDirectory", directory.getText().toString().trim().isEmpty() ? null : command.directory);
                String profileId = profileIds.get(controls.getSelectedItemPosition());
                result.putExtra("controlsProfile", profileId);
                result.putExtra(com.winlator.cmod.dimensity.AdapterIdentity.EXTRA, adapterIdentity.isChecked()
                        ? (identityScope.isChecked() ? com.winlator.cmod.dimensity.AdapterIdentity.NVIDIA_MAIN
                        : com.winlator.cmod.dimensity.AdapterIdentity.NVIDIA) : null);
                result.putExtra(com.winlator.cmod.core.VortekBackend.EXTRA, vortek.isChecked()
                        ? com.winlator.cmod.core.VortekBackend.NAME : null);
                result.saveData();
                GameLaunches.setAutomatic(context, result, automatic.isChecked());
                dialog.dismiss();
                if (saved != null) saved.run();
            } catch (IOException | IllegalArgumentException e) {
                executable.setError(e.getMessage());
            }
        }));
        dialog.show();
    }

    private static EditText field(Context context, LinearLayout parent, String label, String value) {
        TextView title = new TextView(context);
        title.setText(label);
        parent.addView(title);
        EditText input = new EditText(context);
        input.setSingleLine(true);
        input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        input.setText(value);
        parent.addView(input);
        return input;
    }

    private static File resolve(Container container, String path) {
        if (path.matches("(?i)^c:/.*")) return new File(container.getRootDir(), ".wine/drive_c/" + path.substring(3));
        if (path.matches("(?i)^[a-z]:/.*")) {
            for (String[] drive : container.drivesIterator())
                if (drive[0].replace(":", "").equalsIgnoreCase(path.substring(0, 1)))
                    return new File(drive[1], path.substring(3));
        }
        return null;
    }
}
