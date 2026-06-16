Controlador CNC — PCB de Control FluidNC / ESP32
PCB de control para ruteadora CNC de 3 ejes basada en el firmware FluidNC sobre ESP32. El diseño integra drivers de motor, control de spindle, interfaz de sensores y subsistema de alimentación en una placa de dos capas de 10×10 cm.
Este nodo forma parte de un sistema SCADA distribuido donde actúa como elemento edge, recibiendo comandos G-code y reportando telemetría a un gateway central (Raspberry Pi) mediante WebSocket.

Hardware
ComponenteDetalleMicrocontroladorESP32 (antena integrada, WiFi)FirmwareFluidNCDrivers de motorA4988 × 3 (NEMA17, hasta 2 A/fase)Control de spindleRelé on/off (Terminal Block, hasta 100 V / 10 A)Finales de carreraConector genérico 3 pines (3.3 V / GND / Signal)AlmacenamientoTarjeta SD (SPI)USB / UARTCP2102 — conector USB-CAlimentaciónDC Barrel Jack 12 V
Alimentación
12V (Jack) ──┬──────────────────────► Motores NEMA17 x3 (12V @ 6A)
             └──► DC-DC TPS56528 (12V→5V)
                      ├──► A4988 x3 + Relé (5V)
                      └──► LDO AMS1117 (5V→3.3V)
                               ├──► ESP32 (380mA)
                               ├──► Sensores x6 (54mA)
                               └──► SD (100mA)

Terminal Block independiente ────────► Spindle (via relé)
Pines ESP32
FunciónGPIOSTEP X / Y / Z22 / 16 / 27DIR X / Y / Z21 / 17 / 26ENABLE X / Y / Z4 / 13 / 25Límite X− / X+39 / 36Límite Y− / Y+35 / 34Límite Z +/− (compartido)32Trigger (relé spindle)33SD CS / MISO / MOSI / SCK5 / 19 / 23 / 18
Estructura del repositorio
/
├── hardware/
│   ├── schematic/          # Esquemático eléctrico
│   ├── pcb/                # Layout (archivos de diseño)
│   ├── gerbers/            # Archivos de fabricación
│   └── bom/                # Lista de materiales
├── firmware/
│   └── config.yaml         # Configuración FluidNC
├── docs/
│   └── diseno_pcb_cnc.docx # Documento de diseño completo
└── README.md
Estado del diseño

 Selección de plataforma y firmware
 Requerimientos electrónicos
 Presupuesto de potencia
 Asignación de pines
 Floorplan y stack-up
 Esquemático finalizado
 Layout PCB
 Fabricación y ensamble
 Validación y pruebas

Limitaciones conocidas

Sin parada de emergencia física. La detención de la máquina depende exclusivamente de comandos enviados por red. Ante pérdida de conexión WiFi no existe mecanismo local de corte. Pendiente de corrección en revisión futura.
Límites Z+ y Z− compartidos (GPIO 32). Por disponibilidad de pines ambos extremos del eje Z usan la misma entrada, impidiendo a FluidNC distinguir cuál límite se activó.

Documentación
El documento de diseño completo (docs/diseno_pcb_cnc.docx) cubre el planteamiento del problema, selección y justificación de plataforma, requerimientos por subsistema, presupuesto de potencia, asignación de pines, distribución física y sección de revisiones.