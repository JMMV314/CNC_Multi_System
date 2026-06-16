Sistema SCADA Distribuido — Control de Celdas CNC
Sistema de monitoreo y control centralizado para múltiples máquinas CNC, diseñado para eliminar la dependencia de un PC dedicado por máquina y permitir la gestión remota del proceso de fabricación desde una interfaz web única.
Desarrollado como proyecto académico en el marco de la asignatura Embedded Linux System Programming — Universidad Nacional de Colombia, Sede Manizales (2025-2S).

Arquitectura
[Administrador remoto]
        |
        | HTTPS / REST
        v
[AWS EC2 — API REST + MySQL]
   Órdenes de producción · Archivos G-code (BLOB) · Historial
        |
        | HTTPS / REST (polling)
        v
[Raspberry Pi 3 — Gateway HMI]
   Cola de trabajos · Parser G-code · Dashboard web · Despacho
        |           |           |
        | WebSocket | WebSocket | WebSocket
        v           v           v
   [ESP32 #1]  [ESP32 #2]  [ESP32 #N]
   FluidNC     FluidNC     FluidNC
   CNC · WiFi  CNC · WiFi  CNC · WiFi

Componentes del sistema
CapaHardware / PlataformaRolNodo edgeESP32 + FluidNC (PCB propio)Ejecución G-code en tiempo realGatewayRaspberry Pi 3 — Debian LinuxCoordinación, cola de trabajos, HMIBackendAWS EC2 — Node.js + MySQLPersistencia, acceso remotoFrontendSPA web (servida desde EC2)Panel de control y monitoreo

Protocolos de comunicación
EnlaceProtocoloJustificaciónGateway ↔ ESP32WebSocket (TCP persistente)Nativo en FluidNC, bidireccional, baja latencia LANGateway ↔ AWSHTTPS / RESTFlujo asíncrono, cifrado TLS, sin infraestructura adicionalArchivos G-codeREST + BLOB en MySQLAtomicidad con la orden, sin límite de tamaño, trazabilidad integrada

Estructura del repositorio
/
├── gateway/
│   ├── src/
│   │   ├── network.c       # Cliente WebSocket hacia nodos ESP32
│   │   ├── parser.c        # Validación y filtrado de G-code
│   │   ├── scheduler.c     # Cola de trabajos y despacho
│   │   └── api_client.c    # Cliente HTTP hacia AWS
│   └── Makefile
├── backend/
│   ├── src/
│   │   ├── routes/         # Endpoints REST
│   │   └── db/             # Modelos MySQL
│   └── package.json
├── frontend/
│   └── src/                # Dashboard web (SPA)
├── hardware/
│   ├── schematic/
│   ├── pcb/
│   ├── gerbers/
│   └── bom/
├── firmware/
│   └── config.yaml         # Configuración FluidNC
├── docs/
│   ├── sistema_scada.docx  # Documento del sistema completo
│   └── diseno_pcb_cnc.docx # Documento de diseño electrónico
└── README.md

API REST — endpoints principales
MétodoEndpointDescripciónGET/api/ordenesLista órdenes por máquina destinoPOST/api/ordenesCrea orden con archivo G-code adjuntoGET/api/ordenes/:id/archivoDescarga archivo G-codePOST/api/eventosRegistra evento de máquinaGET/api/maquinasEstado actual de todos los nodos

Estados de máquina
En espera  ──►  En operación  ──►  Finalizado
                     |
                     ├──►  Pausada  ──►  En operación
                     └──►  Error
                     
Fuera de línea  (sin conexión WebSocket)

Requisitos del entorno
Gateway (Raspberry Pi)

Debian Linux
GCC (build de módulos en C)
Acceso WiFi a la red local de las máquinas
Acceso a internet para sincronización con AWS

Backend (AWS EC2)

Ubuntu 20.04
Node.js 18+
MySQL 8+

Nodo CNC

ESP32 con FluidNC instalado
Red WiFi compartida con el gateway
Configuración en firmware/config.yaml


Estado del proyecto
Sistema SCADA

 Arquitectura y protocolos definidos
 Gateway — cliente WebSocket y despacho de G-code
 Backend — API REST y persistencia MySQL
 Frontend — dashboard web con monitoreo en tiempo real
 Pruebas de integración end-to-end
 Autenticación robusta de usuarios
 Soporte multi-usuario con roles (operario / administrador)

Hardware (nodo CNC)

 Selección de plataforma y firmware
 Requerimientos electrónicos y presupuesto de potencia
 Asignación de pines y floorplan
 Esquemático finalizado
 Layout y fabricación del PCB
 Validación en máquina real (PENDIENTE)


Resultados de validación
MétricaCriterioResultadoLatencia de estado< 1 s420–870 ms (promedio 580 ms) ✓Latencia de comando< 300 msCumplido ✓Transferencia G-codeSin pérdidaVerificado (31 KB, hash MD5) ✓Recuperación tras reinicioSin pérdida de colaVerificado ✓Incorporación de nuevo nodoSin modificar servidorVerificado ✓