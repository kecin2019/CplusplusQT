markdown
# Med YOLO11 Qt (skeleton)

A minimal Qt6 C++ UI skeleton for a medical imaging app that will host your **Ultralytics YOLO11** detection/segmentation models. It already supports:

- Opening **standard image files** (PNG/JPG/TIFF/BMP) and showing them with pan/zoom
- (Optional) Opening **DICOM** single-frame images via **GDCM** (window/level, slope/intercept, MONOCHROME1 inversion)
- Side-by-side **Input** and **Output** viewers
- A simple **metadata** table (DICOM tags or file info)
- A **log** pane
- A placeholder **inference** button that draws a mock overlay

> No camera/video. The inference logic is a stub — you will wire your ONNX/TensorRT code into `InferenceEngine` later.

## Build

### 1) Install dependencies

- **Qt 6** (Widgets). On Ubuntu:
  ```bash
  sudo apt-get install qt6-base-dev
  ```

- **(Optional) GDCM** for DICOM support:
  ```bash
  sudo apt-get install libgdcm-dev libgdcm-tools
  ```
  If GDCM is not installed, CMake will show a warning and DICOM open will be disabled.

> Note: This project uses **GDCM only** (no VTK). If you previously had issues with `vtkgdcm`, this avoids that path.

### 2) Configure & build

```bash
cmake -S . -B build -DUSE_GDCM=ON   # or OFF if you don't need DICOM
cmake --build build -j
./build/medapp
```

## Where to add your model code

- `src/InferenceEngine.h/.cpp`: expose methods to load YOLO11 weights (ONNX/TensorRT) and run inference.
- The `run()` method currently draws a dummy rectangle + text. Replace with real results (classification for FAI X-rays; segmentation overlay for hip MRI).

## Next steps (suggested)

- Add a **Task selector** (FAI vs Hip MRI) in the toolbar or a dock widget.
- Implement a **Layer overlay** (e.g., show segmentation mask with adjustable opacity).
- Show more **DICOM tags** and support **multi-frame** (MRI series) via a frame slider.
- Wire **ONNXRuntime** or **TensorRT** backend loading per task.
- Export results (PNG mask / CSV measurements).

## Folder structure

```
src/
  MainWindow.*       # UI shell, menus, tabs, log, meta
  ImageView.*        # QGraphicsView with pan/zoom
  InferenceEngine.*  # Placeholder: where YOLO11 inference goes
  DicomUtils.*       # GDCM-backed single-frame loader + WL
  MetaTable.*        # Simple key-value table widget
CMakeLists.txt
```

## Troubleshooting

- **DICOM disabled** dialog appears: install `libgdcm-dev` and reconfigure with `-DUSE_GDCM=ON`.
- If GDCM is installed but not found: ensure `GDCMConfig.cmake` is present (e.g., `/usr/lib/cmake/gdcm/`) and available in `CMAKE_PREFIX_PATH`.
