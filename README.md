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

**Anti-rebote (debouncing):** Dado que la ISR se dispara por flanco (`RISING`) y no por muestreo periódico, se aplica un anti-rebote por **ventana de tiempo con `millis()` dentro de la propia ISR** (~200 ms): un flanco solo se acepta si transcurrió la ventana desde el último válido, descartando los rebotes mecánicos del botón. Se eligió esta técnica en lugar de una máscara o registro de desplazamiento (suma de bits), porque esa última requiere un reloj que tome muestras periódicas del pin —como sí ocurre en la Parte 2 con el temporizador—, lo cual no aplica a una interrupción por flanco. `millis()` es legible dentro de una ISR y la rutina se mantiene corta (sin `Serial` ni `delay`).

## semaphore-isr-interrupt.ino

A partir de la base anterior, se integró la ISR en un semáforo de tiempo fijo. Se incorporó un botón (Pin 3) como interrupción de hardware con el objetivo de permitir que un peatón presione el botón para solicitar el paso (cambiar el semáforo a amarillo y luego rojo).

**Particularidad de la interrupción (Solo activa en verde):**
Para no interrumpir el flujo normal del semáforo de forma incorrecta, la rutina principal evalúa y procesa la señal de la ISR **únicamente durante la fase de luz verde**. El tiempo de espera de la luz verde (`PASS_TEMPO`) se dividió en pequeñas iteraciones de 50 ms. 

Si el peatón presiona el botón durante la luz verde, la ISR cambia el estado de `system_state`. El ciclo detecta este cambio, se interrumpe de inmediato y cede el control para encender la luz amarilla y, posteriormente, la roja (permitiendo el paso). Si el botón se presiona mientras el semáforo ya está en rojo o en amarillo, la bandera de la interrupción se restablece a su estado normal de forma segura para no desencadenar ciclos innecesarios, asegurando que el cruce peatonal solo interrumpa la vía cuando está en verde.

**Anti-rebote (debouncing):** Igual que en `simple-isr-interrupt.ino`, la ISR de este sketch aplica anti-rebote por ventana de tiempo con `millis()` (~200 ms) dentro de la propia interrupción, descartando flancos espurios del botón sin bloquear ni alargar la ISR.

**Video Demostrativo:**
[Ver Video Demostrativo](media/semaphore-isr-interrupt.mp4)

## cpc-multi-interrupt.ino

Esta es la Parte 2 del laboratorio, ahora sobre la **Adafruit Circuit Playground Classic** (microcontrolador ATmega32u4). La aplicación integra dos fuentes de interrupción que trabajan al mismo tiempo sobre los 10 NeoPixels integrados de la placa.

**Una particularidad del hardware:** los botones integrados (A y B) están en los pines D4 y D19, y en el ATmega32u4 esos pines no permiten interrupciones de hardware. Es decir, no se puede usar `attachInterrupt()` sobre un botón como sí hicimos en la Parte 1 con el Arduino UNO. Por eso el botón se atiende desde una interrupción de temporizador.

**Las dos interrupciones y para qué sirve cada una:**

1. **Timer1 (la "interrupción de botón"):** se dispara cada 5 ms y revisa el botón A. Aplica antirrebote contando muestras estables y, cuando confirma una pulsación, actualiza la variable de estado `system_state`. El botón nunca se lee en el `loop`, solo dentro de esta interrupción.
2. **Timer3 (la tarea periódica):** se dispara cada 120 ms y marca una bandera para que el `loop` avance un cuadro de la animación que se muestra en el anillo de NeoPixels.

Cada pulsación del botón A cambia el efecto de luz. Se reutilizan los cuatro efectos del proyecto navideño anterior, ahora reescritos para que avancen cuadro por cuadro con la interrupción del temporizador. Cada efecto tiene además su propia melodía corta: en cada cuadro suena una nota, de modo que la música y las luces avanzan al mismo ritmo. El sonido usa el Timer4 de la placa (a través de `CircuitPlayground.playTone`), que es independiente del Timer1 del botón y del Timer3 de la animación, así que las tres interrupciones conviven sin conflicto.

1. **SPIN:** los colores navideños giran alrededor del anillo, al ritmo de "Jingle Bells".
2. **TWINKLE:** se encienden píxeles al azar, como nieve cayendo, con "Twinkle Twinkle Little Star".
3. **ALT:** alternancia clásica de rojo y verde, acompañada de "Mary Had a Little Lamb".
4. **CHASE:** una luz dorada persigue alrededor del anillo, con la melodía de "Himno a la Alegría" (Oda a la Alegría).

Cada nota tiene su propia duración y se deja un pequeño silencio entre notas, de modo que las notas repetidas (como las tres "E" de "Jingle Bells") se escuchan separadas y la melodía suena natural en lugar de un tono continuo.

**Manejo de conflictos y condiciones de carrera:**
Las dos interrupciones y el `loop` comparten variables, así que se aplican las mismas buenas prácticas de la Parte 1 (variables `volatile`, interrupciones muy cortas que solo levantan banderas). Al inicio de cada vuelta del `loop` se toma una "foto" de las banderas dentro de un bloque atómico (`ATOMIC_BLOCK`), copiándolas y limpiándolas de una sola vez para que ninguna interrupción que ocurra a mitad de la lectura provoque datos inconsistentes o pierda un evento. Cuando coinciden una pulsación y un tic periódico en la misma vuelta, el botón tiene prioridad: el cambio de modo se aplica antes de dibujar el siguiente cuadro.

**Video Demostrativo:**
[Ver Video Demostrativo](media/cpc-multi-interrupt.mp4)

## Análisis comparativo: Arduino UNO R3 vs Circuit Playground Classic

Las dos partes del laboratorio usan plataformas distintas, y la diferencia más importante para este tema está en cómo cada una maneja las interrupciones.

| Aspecto | Arduino UNO R3 (ATmega328P) | Circuit Playground Classic (ATmega32u4) |
|---|---|---|
| Interrupciones externas | INT0 e INT1, en pines 2 y 3 | INT0 a INT3 e INT6, en pines 0, 1, 2, 3 y 7 |
| ¿El botón cae en un pin de interrupción? | Sí, el botón externo se conecta al pin 2 o 3 | No, los botones integrados están en D4 y D19 |
| Técnica usada para el botón | `attachInterrupt()` directo sobre el pin | Interrupción de temporizador con antirrebote |
| Interrupción periódica | Timer1 disponible | Timer1 y Timer3 disponibles (se evitó `tone()` para no ocupar Timer3) |
| Sensores internos | Ninguno integrado | Acelerómetro (su interrupción llega al pin D7), botones y switch |
| LEDs | LEDs sueltos con resistencias de 220 Ω | 10 NeoPixels integrados |

En la práctica, en el UNO R3 un botón físico se conecta directo a una línea de interrupción del microcontrolador, así que la pulsación dispara la ISR de inmediato. En la Circuit Playground Classic los botones no están en pines con esa capacidad, por lo que hubo que cambiar la estrategia y atender el botón muestreándolo desde una interrupción de temporizador. Esto deja claro que tener un "pin con capacidad de interrupción" es una restricción de arquitectura de la placa, no algo que siempre esté disponible.
