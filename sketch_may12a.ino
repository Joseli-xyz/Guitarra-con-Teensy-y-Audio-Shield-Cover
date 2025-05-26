#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// Objetos para el procesamiento de audio de la guitarra
AudioInputI2S            audioInput;     // Entrada audio desde LINE IN
AudioAnalyzePeak         peak;           // Para medir nivel de señal
AudioEffectWaveshaper    waveshaper;     // Para distorsión
AudioMixer4              mixer;          // Para mezcla y control de ganancia
AudioOutputI2S           audioOutput;    // Salida audio (auriculares/LINE OUT)
AudioControlSGTL5000     audioShield;    // Control del codec

// Conexiones entre componentes de audio
AudioConnection          patchCord1(audioInput, 0, peak, 0);
AudioConnection          patchCord2(audioInput, 0, waveshaper, 0);
AudioConnection          patchCord3(waveshaper, 0, mixer, 0);
AudioConnection          patchCord4(mixer, 0, audioOutput, 0);
AudioConnection          patchCord5(mixer, 0, audioOutput, 1);

// Variables para controlar efectos y configuración
const int LED_PIN = 13;
float inputGain = 3.0;      // Ganancia de entrada inicial
float distortionLevel = 3;  // Nivel de distorsión (1-10)
float outputLevel = 0.7;    // Nivel de salida
float waveshapeValues[17];  // Array para la curva de distorsión

// Potenciómetros (opcional si se añaden)
#define POT_GAIN A1         // Potenciómetro para ganancia
#define POT_DISTORTION A2   // Potenciómetro para distorsión

void setup() {
  // Inicializar comunicación serial
  Serial.begin(9600);
  delay(500);
  Serial.println("Procesador de Guitarra con Teensy");
  
  // Configurar LED
  pinMode(LED_PIN, OUTPUT);
  
  // Reservar memoria de audio
  AudioMemory(12);
  
  // Activar el Audio Shield
  audioShield.enable();
  
  // Configuración del Audio Shield
  audioShield.inputSelect(AUDIO_INPUT_LINEIN);  // Usar entrada de línea
  audioShield.micGain(0);                       // Sin ganancia de micrófono
  audioShield.lineInLevel(5);                   // Sensibilidad media-alta para línea
  audioShield.volume(outputLevel);              // Volumen de salida
  
  // Configurar mixer
  mixer.gain(0, 1.0);  // Canal 0 a máxima ganancia
  
  // Inicializar la curva de distorsión
  updateDistortionCurve();
  
  // Opcional: configurar pines para potenciómetros
  pinMode(POT_GAIN, INPUT);
  pinMode(POT_DISTORTION, INPUT);
  
  Serial.println("Sistema inicializado. Conecta tu guitarra al LINE IN");
  Serial.println("Controles disponibles vía serial:");
  Serial.println("g+/g- : Subir/bajar ganancia de entrada");
  Serial.println("d+/d- : Subir/bajar distorsión");
  Serial.println("v+/v- : Subir/bajar volumen de salida");
  Serial.println("c : Mostrar configuración actual");
}

void loop() {
  // Leer potenciómetros si están conectados
  // readPotentiometers();
  
  // Leer nivel de señal
  if (peak.available()) {
    float peakLevel = peak.read();
    
    // Mostrar nivel en el monitor serial como VU-meter textual
    displayVUMeter(peakLevel);
    
    // LED se ilumina con la intensidad de la señal
    int ledBrightness = int(peakLevel * 255);
    analogWrite(LED_PIN, ledBrightness);
  }
  
  // Procesar comandos seriales
  processSerialCommands();
  
  delay(50);  // Pequeña pausa para estabilidad
}

// Actualiza la curva de distorsión basada en el nivel seleccionado
void updateDistortionCurve() {
  // Crear una curva de distorsión de 17 puntos
  // La curva define cómo se transforman las amplitudes de entrada
  
  // Calcular la cantidad de "saturación" basada en el nivel de distorsión
  float curve = 1.0 + (distortionLevel * 0.4);
  
  // Generar la curva de waveshaping
  for (int i=0; i<17; i++) {
    float x = (float)((i - 8) / 8.0);
    // Ecuación de distorsión suave que aumenta con el nivel
    waveshapeValues[i] = tanh(x * curve) / tanh(curve);
  }
  
  // Aplicar la curva al efecto waveshaper
  waveshaper.shape(waveshapeValues, 17);
  
  Serial.print("Distorsión actualizada: ");
  Serial.println(distortionLevel);
}

// Procesa comandos recibidos a través del puerto serial
void processSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    
    if (command == "g+") {
      inputGain += 0.5;
      audioShield.lineInLevel(constrain(inputGain, 0, 15));
      Serial.print("Ganancia de entrada: ");
      Serial.println(inputGain);
    }
    else if (command == "g-") {
      inputGain -= 0.5;
      audioShield.lineInLevel(constrain(inputGain, 0, 15));
      Serial.print("Ganancia de entrada: ");
      Serial.println(inputGain);
    }
    else if (command == "d+") {
      distortionLevel += 0.5;
      distortionLevel = constrain(distortionLevel, 1, 10);
      updateDistortionCurve();
    }
    else if (command == "d-") {
      distortionLevel -= 0.5;
      distortionLevel = constrain(distortionLevel, 1, 10);
      updateDistortionCurve();
    }
    else if (command == "v+") {
      outputLevel += 0.05;
      outputLevel = constrain(outputLevel, 0, 1.0);
      audioShield.volume(outputLevel);
      Serial.print("Volumen: ");
      Serial.println(outputLevel);
    }
    else if (command == "v-") {
      outputLevel -= 0.05;
      outputLevel = constrain(outputLevel, 0, 1.0);
      audioShield.volume(outputLevel);
      Serial.print("Volumen: ");
      Serial.println(outputLevel);
    }
    else if (command == "c") {
      Serial.println("\n--- Configuración Actual ---");
      Serial.print("Ganancia entrada: ");
      Serial.println(inputGain);
      Serial.print("Nivel distorsión: ");
      Serial.println(distortionLevel);
      Serial.print("Volumen salida: ");
      Serial.println(outputLevel);
      Serial.print("Uso CPU: ");
      Serial.print(AudioProcessorUsage());
      Serial.println("%");
      Serial.print("Memoria: ");
      Serial.print(AudioMemoryUsage());
      Serial.print(" de ");
      Serial.println(AudioMemoryUsageMax());
      Serial.println("-------------------------\n");
    }
  }
}

// Opcional: leer valores de potenciómetros físicos
void readPotentiometers() {
  // Solo activar si tienes potenciómetros conectados
  int gainPot = analogRead(POT_GAIN);
  int distPot = analogRead(POT_DISTORTION);
  
  // Mapear valores a los rangos adecuados
  float newGain = map(gainPot, 0, 1023, 0, 15) / 2.0;
  float newDist = map(distPot, 0, 1023, 10, 100) / 10.0;
  
  // Aplicar solo si hay cambios significativos
  if (abs(newGain - inputGain) > 0.2) {
    inputGain = newGain;
    audioShield.lineInLevel(inputGain);
  }
  
  if (abs(newDist - distortionLevel) > 0.2) {
    distortionLevel = newDist;
    updateDistortionCurve();
  }
}

// Muestra un medidor VU textual en el monitor serial
void displayVUMeter(float level) {
  static unsigned long lastDisplayTime = 0;
  unsigned long currentTime = millis();
  
  // Actualizar solo cada 200ms para no saturar el puerto serial
  if (currentTime - lastDisplayTime > 200) {
    lastDisplayTime = currentTime;
    
    // Convertir nivel a dB (aprox)
    float dB = 20.0 * log10(level + 0.0001);
    dB = constrain(dB, -60, 0);
    
    // Crear VU meter textual
    String meter = "Signal: [";
    int meterLength = 30;
    int activeBars = map(dB, -60, 0, 0, meterLength);
    
    for (int i = 0; i < meterLength; i++) {
      if (i < activeBars) {
        // Añadir caracteres diferentes según el nivel
        if (i > meterLength * 0.8) {
          meter += "!"; // Zona roja/clipeo
        }
        else if (i > meterLength * 0.6) {
          meter += "*"; // Zona amarilla
        }
        else {
          meter += "="; // Zona verde
        }
      }
      else {
        meter += " "; // Sin señal
      }
    }
    
    meter += "] ";
    meter += String(int(dB));
    meter += " dB";
    
    Serial.println(meter);
  }
}
