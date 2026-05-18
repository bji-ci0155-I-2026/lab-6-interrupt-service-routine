# lab-6-interrupt-service-routine

Universidad de Costa Rica \- Sistemas Empotrados de Tiempo Real \- CI-0155

**Profesor**: Prof. Ariel Mora Jiménez

**Integrantes:**

- Isabella Rodríguez Sánchez (C26701)  
- Esteban Isaac Baires Cerdas (C10844)  
- Jorge Ricardo Díaz Sagot (C12565)

## Arquitectura y Buenas Prácticas

Todas las prácticas implementadas en estos ejercicios se basan en los principios y lineamientos documentados en el libro del curso, *"Embedded Systems Architecture 2nd edition"*. De este texto se aplicaron las siguientes reglas de oro para el diseño de ISRs (Interrupt Service Routines):

1. **Uso de la palabra clave `volatile`**: Toda variable modificada en una ISR y leída en el hilo principal debe ser declarada como `volatile` para garantizar que el compilador lea el valor en memoria real en todo momento.
2. **Mantener las ISR lo más cortas posibles**: Las rutinas de interrupción no deben realizar procesamiento pesado, impresiones seriales ni contener ciclos de espera (`delay`); su único propósito es registrar el evento en una variable de estado (`flag`).
3. **Desacoplamiento**: La carga computacional fuerte se delega al hilo principal (`loop`), separando el manejo de hardware subyacente de la lógica de la aplicación.

**Pines de Interrupción en Arduino UNO R3:**
A nivel físico, el microcontrolador ATmega328P del Arduino UNO R3 posee una limitación arquitectónica: solo cuenta con dos pines dedicados para interrupciones externas de hardware (señales INT0 e INT1). Debido a esto, la función `attachInterrupt()` del framework de Arduino exige que los botones o sensores que disparen una ISR se conecten estrictamente a los **Pines Digitales 2 o 3**. Esto segun el full pin layout del ATmega328P en el sitio de Arduino.

## simple-isr-interrupt.ino

En este código, se implementó el concepto básico de una **Rutina de Servicio de Interrupción (ISR)** utilizando buenas prácticas para sistemas empotrados. Se conectó un botón con una resistencia *pulldown* (470k Ω) que envía una señal en alto (5V) al presionar, configurando la interrupción en modo `RISING`.

La ISR es extremadamente corta y su única función es modificar el valor de una variable de estado declarada como `volatile` (`system_state`). Luego, es en la rutina principal (`loop`) donde se verifica esta variable y se imprime el cambio de estado en el monitor serial. Esto asegura que el procesador no realice tareas pesadas y no se bloquee dentro de la interrupción.

## semaphore-isr-interrupt.ino

A partir de la base anterior, se integró la ISR en un semáforo de tiempo fijo. Se incorporó un botón (Pin 3) como interrupción de hardware con el objetivo de permitir que un peatón presione el botón para solicitar el paso (cambiar el semáforo a amarillo y luego rojo).

**Particularidad de la interrupción (Solo activa en verde):**
Para no interrumpir el flujo normal del semáforo de forma incorrecta, la rutina principal evalúa y procesa la señal de la ISR **únicamente durante la fase de luz verde**. El tiempo de espera de la luz verde (`PASS_TEMPO`) se dividió en pequeñas iteraciones de 50 ms. 

Si el peatón presiona el botón durante la luz verde, la ISR cambia el estado de `system_state`. El ciclo detecta este cambio, se interrumpe de inmediato y cede el control para encender la luz amarilla y, posteriormente, la roja (permitiendo el paso). Si el botón se presiona mientras el semáforo ya está en rojo o en amarillo, la bandera de la interrupción se restablece a su estado normal de forma segura para no desencadenar ciclos innecesarios, asegurando que el cruce peatonal solo interrumpa la vía cuando está en verde.

**Video Demostrativo:**
[Ver Video Demostrativo](media/semaphore-isr-interrupt.mp4)
