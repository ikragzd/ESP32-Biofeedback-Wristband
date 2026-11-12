# Closed-Loop Biofeedback Wristband with Standalone TinyML

A standalone, closed-loop wearable biomechatronic system designed to process physiological signals in real time and regulate the autonomic nervous system through haptic and thermal actuators. By running TinyML inference directly on an ESP32-S3 Mini, the device operates completely offline without requiring a mobile application or cloud connectivity.

## System Architecture
Unlike passive biometric trackers that only log data or open-loop stimulators that require manual activation, this hardware performs autonomous on-device classification and physical intervention:

1. **Startup Auto-Calibration:** During the first 2 minutes after power-on, TinyML inference is paused while baseline sensor readings are averaged and stored in non-volatile memory to adapt to daily user baselines.
2. **32 Hz Multi-Sensor Fusion & Filtering:** Physiological and motion signals are sampled synchronously at 32 Hz and processed through a software Moving Average Filter to suppress hardware noise and motion artifacts.
3. **On-Device TinyML Classification:** A trained neural network evaluates multi-dimensional time-series patterns locally on the ESP32-S3 Mini to distinguish physical exercise or daily movement from acute autonomic stress episodes.
4. **Closed-Loop Actuation:** When physiological thresholds are exceeded, the microcontroller autonomously drives tactile and thermal actuators to help regulate the autonomic nervous system.
5. ## TinyML Model Training & Evaluation

To distinguish physical exercise and daily movement from acute autonomic stress episodes without cloud dependency, a supervised neural network was trained on synchronized multi-sensor time-series data and deployed offline on the ESP32-S3 Mini.

### Data Pipeline & Preprocessing
* **Sampling Rate:** 32 Hz synchronized acquisition across optical heart rate, skin temperature, GSR, and 6-axis accelerometer channels.
* **Noise Suppression:** Raw sensor streams are filtered using a software Moving Average Filter prior to feature extraction and inference to eliminate motion artifacts.
* **Target Classes:** Baseline Calm, Physical Activity, and Autonomic Stress Crisis.

### Model Specifications
| Parameter | Specification |
| :--- | :--- |
| **Input Features** | Heart Rate, Skin Temperature, GSR Conductance, 6-Axis Acceleration |
| **Sampling Frequency** | 32 Hz |
| **DSP Filter** | Moving Average Filter |
| **Deployment Format** | Quantized TensorFlow Lite C Array on ESP32-S3 Mini |
| **Inference Mode** | 100% Offline Edge AI |

### Validation & Confusion Matrix
Model training scripts, sample sensor datasets, and embedded deployment files are located in the main repository directory. Classification performance across resting, physical exertion, and high-stress states is visualized below:

## Hardware Components
* **Microcontroller:** ESP32-S3 Mini dual-core module running offline Edge AI inference
* **Sensors:**
  * **MAX30101:** Optical heart rate and blood oxygen saturation monitoring
  * **100k NTC Thermistor:** Direct skin-contact temperature measurement via a voltage divider circuit
  * **GSR Sensor with Op-Amp:** Galvanic skin response measurement with hardware analog noise filtering
  * **6-Axis Accelerometer:** Motion sensing to separate physical activity from cognitive or autonomic stress
* **Actuators:**
  * **LRA Motor with AC Driver IC:** Sharp, frequency-controlled tactile feedback
  * **PWM-Driven Peltier Module:** Fluctuating cooling stimulation paired with an aluminum heat-dissipation plate and vented housing to prevent thermal adaptation
* **Power Management:** Rechargeable 3.7V 400mAh 25C LiPo battery with dedicated charge and overcurrent protection circuitry
* **Mechanical Design:** Curved ergonomic upper housing for the microcontroller and battery, paired with a lower skin-contact module housing the sensors and Peltier element via an asymmetric TPU strap.

## Modular Industrial Variant
* **Occupational Health & Safety Extension:** Supports an optional LoRa transceiver, battery telemetry, and physical emergency SOS button for off-grid worker safety monitoring in high-risk sites such as mines.
