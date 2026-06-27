# NBVulkan

## 1. Project Overview

NBVulkan is a Vulkan-based real-time N-body simulation project that visualizes the movement of many particles under gravitational interaction. The simulation uses a Barnes-Hut style approach to handle large numbers of bodies more efficiently than a direct all-pairs calculation, making it suitable for testing and observing large particle systems.

The project combines GPU rendering with interactive controls so that the simulation is not only computed and displayed, but also easier to explore visually. Users can zoom, rotate, tilt the view, pause the simulation, and adjust runtime behavior while the program is running.

A Dear ImGui control panel is integrated into the application to provide a simple graphical interface for common controls and runtime information. This makes the project easier to test, demonstrate, and extend in the future. The final system is designed as an interactive visualization tool where simulation, rendering, camera movement, and user interface work together as one complete application.

---

## 2. Interactive Features and Runtime Controls

The project includes several interactive features that make the simulation easier to observe, control, and demonstrate while it is running.

* **Dear ImGui control panel**
  Provides an on-screen interface for controlling the simulation and view settings without changing the source code during runtime.

* **Runtime information display**
  Shows useful information such as FPS and the current number of bodies, helping users understand performance while the simulation is running.

* **Zoom control**
  Allows the user to zoom in and out of the particle system using the mouse wheel or the ImGui panel.

* **Mouse-based view interaction**
  Supports left mouse drag movement to rotate/orbit the view and inspect the particle system from different angles.

* **Camera tilt control**
  Allows the view angle to be adjusted so the particle distribution can be observed with more depth and perspective.

* **Auto-rotation control**
  Provides automatic rotation of the particle system, with runtime control over rotation speed and direction.

* **Pause and resume**
  Allows the simulation to be paused and resumed while keeping the current visual state on screen.

* **Reset controls**
  Provides options to reset zoom, camera/view settings, and other runtime controls back to default values.

* **Help menu**
  Displays available keyboard, mouse, and UI controls inside the application so users can quickly learn how to interact with the simulation.

---

## 3. User Controls

The simulation can be controlled using the keyboard, mouse, and the Dear ImGui panel.

| Control             | Action                                                        |
| ------------------- | ------------------------------------------------------------- |
| Mouse wheel         | Zoom in or out                                                |
| Left mouse drag     | Rotate/orbit the view                                         |
| Space               | Pause or resume the simulation                                |
| R                   | Toggle auto-rotation                                          |
| Up / Down arrows    | Increase or decrease simulation timestep                      |
| Left / Right arrows | Change rotation speed and direction                           |
| H                   | Show or hide the help menu                                    |
| 0                   | Reset view/settings                                           |
| ImGui panel         | Adjust zoom, camera tilt, rotation, pause, and reset controls |

These controls allow the simulation to be explored interactively without recompiling the program.

---

## 4. Dear ImGui Control Panel

The Dear ImGui panel provides a runtime interface inside the Vulkan application. It is used to display useful information and expose common controls directly on the screen.

The panel includes runtime information such as FPS and body count, along with controls for zoom, camera tilt, auto-rotation, pause/resume, and reset options. This makes the application easier to test and demonstrate because the user can adjust the simulation view while it is running.

The ImGui integration also gives the project a clear place for future controls. Additional simulation parameters, debugging tools, or profiling information can be added to the same panel later.

---

## 5. Camera and View Interaction

The project supports camera-style interaction through zooming, rotation, tilt, and mouse dragging. These controls allow the particle system to be viewed from different angles and distances.

The current implementation uses CPU-side transformation of particle positions before rendering. This approach was used because the renderer does not currently use a full Vulkan camera/view-projection matrix system. Instead of introducing a complete camera pipeline, the view controls are applied in a simpler way that fits into the existing rendering structure.

This design keeps the interaction easy to integrate while still giving the user useful visual control over the simulation. A future version could replace this with a full camera system using view and projection matrices.

---

## 6. Build and Run

The project expects the `imgui` folder to be placed beside the `NBVulkan` folder:

```text
CG_Project/
├── NBVulkan/
└── imgui/
```

This is important because the Makefile uses ImGui from:

```text
../imgui
```

To build and run the project:

```bash
cd NBVulkan

cd shaders
bash compile.sh
cd ..

make clean
make
./exec
```

If the program cannot find the shader files, run the shader compile step again before rebuilding.

---

## 7. Important Code Areas

The main implementation areas are:

* `main.cpp`
  Contains the simulation application logic, interaction variables, mouse callbacks, ImGui frame creation, UI controls, camera/view transformation logic, and help menu updates.

* `Makefile`
  Builds the project and includes the Dear ImGui source files and backend files.

* `../imgui`
  External Dear ImGui dependency used by the project. This folder should remain beside the `NBVulkan` folder unless the Makefile paths are changed.

These areas are the best starting points for future developers who want to extend the user interface, add more controls, or improve the camera system.

---

## 8. Performance Notes

The simulation can be tested with different values of `NBODIES` in `main.cpp`. Performance depends on the system, Vulkan device, graphics driver, and whether the program is running on a native GPU Vulkan setup or software rendering.

For benchmarking, body counts were doubled each time to observe how performance changes as the problem size increases. The typical FPS value represents the stable observed FPS during runtime.

Frame Time (ms) was calculated as:

Frame Time (ms) = 1000 / Typical FPS

The `n log2(n)` column is included to compare the observed performance trend with the expected Barnes-Hut style scaling behavior.

| Bodies (n) | Typical FPS | Frame Time (ms) | log2(n) | n * log2(n) |
|---:|---:|---:|---:|---:|
| 10,000 | 27.0 | 37.04 | 13.29 | 132,877 |
| 20,000 | 21.0 | 47.62 | 14.29 | 285,754 |
| 40,000 | 14.0 | 71.43 | 15.29 | 611,508 |
| 80,000 | 7.0 | 142.86 | 16.29 | 1,303,017 |
| 160,000 | 3.7 | 270.27 | 17.29 | 2,766,034 |
| 320,000 | 2.3 | 434.78 | 18.29 | 5,852,068 |

As the number of bodies increases, the FPS decreases and the frame time increases. Using doubled body counts helps show how the simulation scales as the problem size grows.

For smoother testing, use a fixed window size and avoid resizing the window while the program is running.

---

## 9. Known Limitations

The current project has some limitations:

* Window resizing is disabled because swapchain recreation is not implemented.
* On WSL or software Vulkan rendering, flickering or unstable display behavior may occur.
* The camera/view system is currently based on CPU-side particle transformation, not a full Vulkan camera matrix system.
* Some Vulkan synchronization and frame handling behavior could be improved in future versions.

These limitations do not prevent the project from running, but they are important to understand before extending the renderer.

---

## 10. Future Improvements

Possible future improvements include:

* Add a full camera system using view and projection matrices.
* Implement swapchain recreation so the window can be resized safely.
* Add more simulation parameters to the ImGui panel.
* Add profiling or benchmarking information inside the UI.
* Improve Vulkan synchronization and frame handling.
* Add save/load presets for UI and camera settings.
* Add more debugging controls for simulation and rendering behavior.

These improvements would make the project easier to use, more stable across systems, and more flexible for future development.
