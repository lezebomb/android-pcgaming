package com.winlator.cmod.core;

import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Paths and arguments are separate; a different working directory must not change the target. */
public final class WindowsLaunchCommand {
    private static final Pattern LEGACY_TARGET = Pattern.compile("(?i)\\.(exe|lnk|bat|cmd)(?=\\s|$)");
    public final String executable;
    public final String arguments;
    public final String directory;

    public WindowsLaunchCommand(String target, String arguments, String directory) {
        target = singleLine(target).trim();
        String embeddedArgs = "";
        if (target.startsWith("\"")) {
            int end = target.indexOf('"', 1);
            if (end < 0) throw new IllegalArgumentException("EXE 路径的引号未闭合");
            embeddedArgs = target.substring(end + 1).trim();
            target = target.substring(1, end);
        } else {
            Matcher matcher = LEGACY_TARGET.matcher(target);
            if (matcher.find()) {
                embeddedArgs = target.substring(matcher.end()).trim();
                target = target.substring(0, matcher.end());
            }
        }
        executable = path(target);
        this.arguments = (embeddedArgs + " " + singleLine(arguments)).trim();
        String requestedDir = singleLine(directory).trim();
        this.directory = requestedDir.isEmpty()
                ? (executable.toLowerCase(Locale.ROOT).endsWith(".lnk") ? "" : parent(executable))
                : path(requestedDir);
    }

    public static String path(String value) {
        value = singleLine(value).trim();
        if (value.startsWith("\"") && value.endsWith("\"") && value.length() > 1)
            value = value.substring(1, value.length() - 1);
        if (value.isEmpty() || value.indexOf('"') >= 0)
            throw new IllegalArgumentException("请输入有效的游戏路径");
        return value.replace('\\', '/');
    }

    private static String singleLine(String value) {
        if (value == null) return "";
        for (int i = 0; i < value.length(); i++)
            if (Character.isISOControl(value.charAt(i)))
                throw new IllegalArgumentException("启动设置不能包含换行或控制字符");
        return value;
    }

    public static String parent(String path) {
        int slash = path.lastIndexOf('/');
        if (slash < 0) return "";
        return slash == 2 && path.charAt(1) == ':' ? path.substring(0, 3) : path.substring(0, slash);
    }

    public String toWinHandlerCommand() {
        String dir = directory;
        // A link's own Start In setting remains authoritative unless the user overrides it.
        String prefix = dir.isEmpty() ? "" : "/dir " + dir.replace(" ", "\\ ") + " ";
        return "winhandler.exe " + prefix + "\"" + executable + "\"" +
                (arguments.isEmpty() ? "" : " " + arguments);
    }
}
