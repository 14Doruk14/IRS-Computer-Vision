# IRS Computer Vision Tasks

Computer vision tasks for a RoboMaster-style robot: Python (OpenCV, Colab) solutions and a C image-processing library written from scratch.

## Python notebooks (`python/`)

| Notebook | Task |
|---|---|
| task1_hsv_color_mask.ipynb | Task 1: HSV color thresholding (blue mask) |
| task2_light_bar_detection.ipynb | Task 2: Armor light-bar detection with morphology (erode/dilate) |
| task3_pnp_distance.ipynb | Task 3: Distance, yaw and pitch with solvePnP (work in progress) |

## C library (Code::Blocks, MinGW)

| File | What it does |
|---|---|
| irs_vision.c / .h | Reusable image-processing library: PPM/PGM I/O, color masks, erosion/dilation, connected regions, drawing |
| img_pro.c / .h | Basic image helpers |
| main.c | Task 2 pipeline in C (`IRS_Gorev2_C.cbp`) |
| zirh/ | Armor plate detection (`IRS_Zirh.cbp`), result: `zirh/zirh_sonuc.png` |
| deneyler/ | Experiments: kernel size and double-mask comparisons |

Input image: `arena.ppm`. Results: `sonuc.png`, `zirh/zirh_sonuc.png`, `deneyler/*.png`.

Speed (mask + opening + regions on arena.ppm): C `-O0` 137 ms, C `-O2` 46 ms, OpenCV 14 ms.
