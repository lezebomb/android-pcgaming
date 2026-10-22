package com.winlator.cmod.xenvironment;

import android.content.Context;

import com.winlator.cmod.core.FileUtils;
import com.winlator.cmod.xenvironment.components.GuestProgramLauncherComponent;

import java.io.File;
import java.util.ArrayList;
import java.util.Iterator;

public class XEnvironment implements Iterable<EnvironmentComponent> {
    private final Context context;
    private final ImageFs imageFs;
    private final ArrayList<EnvironmentComponent> components = new ArrayList<>();
    private final ArrayList<EnvironmentComponent> startedComponents = new ArrayList<>();

    public XEnvironment(Context context, ImageFs imageFs) {
        this.context = context;
        this.imageFs = imageFs;
    }

    public Context getContext() {
        return context;
    }

    public ImageFs getImageFs() {
        return imageFs;
    }

    public void addComponent(EnvironmentComponent environmentComponent) {
        environmentComponent.environment = this;
        components.add(environmentComponent);
    }

    public <T extends EnvironmentComponent> T getComponent(Class<T> componentClass) {
        for (EnvironmentComponent component : components) {
            if (component.getClass() == componentClass) return (T)component;
        }
        return null;
    }

    @Override
    public Iterator<EnvironmentComponent> iterator() {
        return components.iterator();
    }

    public File getTmpDir() {
        File tmpDir = new File(context.getFilesDir(), "tmp");
        if (!tmpDir.isDirectory()) {
            tmpDir.mkdirs();
            FileUtils.chmod(tmpDir, 0771);
        }
        return tmpDir;
    }

    public synchronized void startEnvironmentComponents() {
        if (!startedComponents.isEmpty()) return;
        FileUtils.clear(getTmpDir());
        try {
            for (EnvironmentComponent component : this) {
                startedComponents.add(component);
                component.start();
            }
        } catch (RuntimeException failure) {
            stopEnvironmentComponents();
            throw failure;
        }
    }

    public synchronized void stopEnvironmentComponents() {
        // Stop Wine before the rendering/socket services it may still be using.
        while (!startedComponents.isEmpty()) {
            EnvironmentComponent component = startedComponents.remove(startedComponents.size() - 1);
            try { component.stop(); }
            catch (RuntimeException failure) {
                android.util.Log.e("XEnvironment", "Component shutdown failed: " + component.getClass().getSimpleName(), failure);
            }
        }
    }

    public void onPause() {
        GuestProgramLauncherComponent guestProgramLauncherComponent = getComponent(GuestProgramLauncherComponent.class);
        if (guestProgramLauncherComponent != null) guestProgramLauncherComponent.suspendProcess();
    }

    public void onResume() {
        GuestProgramLauncherComponent guestProgramLauncherComponent = getComponent(GuestProgramLauncherComponent.class);
        if (guestProgramLauncherComponent != null) guestProgramLauncherComponent.resumeProcess();
    }
}
