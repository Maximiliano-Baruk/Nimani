import numpy as np
import matplotlib.pyplot as plt

# --- Parámetros del Ejercicio y Fisiológicos (Basados en la conversación anterior) ---
total_time = 60       # Duración total en segundos
num_breaths = 10      # Número total de respiraciones
cycle_duration = total_time / num_breaths  # Duración de un ciclo respiratorio (6 segundos)
inhale_duration = 2   # Duración de la inhalación en segundos
exhale_duration = 4   # Duración de la exhalación en segundos
peak_inhale_flow = 35 # Flujo pico de inhalación en L/min (Positivo)
peak_exhale_flow = -18 # Flujo pico de exhalación en L/min (Negativo)

# --- Parámetros para la Generación de Datos ---
sampling_rate = 20  # Puntos de datos por segundo (aumentado para mayor suavidad)
num_points = int(total_time * sampling_rate) # Número total de puntos de datos

# Generar el vector de tiempo (de 0 a 60 segundos)
# Usamos num_points+1 para incluir exactamente 0.0 y 60.0
time_vector = np.linspace(0, total_time, num_points + 1)

# Generar el vector de flujo objetivo inicializado a ceros
target_flow_vector = np.zeros_like(time_vector)

# --- Calcular el flujo para cada punto en el tiempo ---
for i, t in enumerate(time_vector):
    # Calcular en qué parte del ciclo de 6 segundos estamos
    time_in_cycle = t % cycle_duration

    if time_in_cycle < inhale_duration:
        # Estamos en la fase de inhalación
        # Usamos sin(pi * t / T) para una media onda sinusoidal
        current_flow = peak_inhale_flow * np.sin(np.pi * time_in_cycle / inhale_duration)
    elif time_in_cycle < cycle_duration: # Originalmente usaba < cycle_duration, pero para evitar problemas en el punto exacto de cycle_duration, ajustamos ligeramente
        # Estamos en la fase de exhalación
        # Calculamos el tiempo transcurrido dentro de la fase de exhalación
        time_in_exhale_phase = time_in_cycle - inhale_duration
        current_flow = peak_exhale_flow * np.sin(np.pi * time_in_exhale_phase / exhale_duration)
    else:
         # Esto no debería ocurrir con el linspace usado, pero por si acaso
         current_flow = 0

    # Corrección mínima para evitar que en t=0, t=2, t=6 etc. se ponga un valor infinitesimal negativo por errores de flotante
    # Y asegurar que el flujo es exactamente 0 en las transiciones si t es exactamente el final de una fase
    if np.isclose(time_in_cycle, 0.0) or np.isclose(time_in_cycle, inhale_duration) or np.isclose(time_in_cycle % cycle_duration, 0.0, atol=1e-9):
         # Ajuste para el último punto del ciclo completo
         if np.isclose(t, total_time) and np.isclose(time_in_cycle, 0.0):
              current_flow = 0.0
         # Para otros puntos de transición exactos
         elif not np.isclose(t, 0.0): # Evita forzar 0 en t=0 si no es ciclo completo
              current_flow = 0.0


    target_flow_vector[i] = current_flow


# --- Crear la Gráfica ---
fig, ax = plt.subplots(figsize=(15, 6)) # Tamaño de la figura (ancho, alto) en pulgadas

# Graficar los datos objetivo
ax.plot(time_vector, target_flow_vector, label='Patrón Objetivo', color='dodgerblue', linewidth=2)

# Añadir línea cero como referencia
ax.axhline(0, color='grey', linestyle='--', linewidth=0.8, label='Flujo Cero')

# Configurar etiquetas y título
ax.set_xlabel('Tiempo (segundos)', fontsize=12)
ax.set_ylabel('Flujo de Aire (L/min)', fontsize=12)
ax.set_title('Gráfica Objetivo: Patrón de Respiración Controlada\n(10 resp/min, Inhalación 2s, Exhalación 4s)', fontsize=14)

# Configurar límites del eje Y para mejor visualización
ax.set_ylim(peak_exhale_flow - 10, peak_inhale_flow + 10) # Un poco de margen

# Añadir rejilla
ax.grid(True, linestyle=':', alpha=0.6)

# Añadir leyenda
ax.legend(fontsize=10)

# Ajustar layout para evitar solapamientos
plt.tight_layout()

# Mostrar la gráfica
plt.show()

# --- Nota para la segunda gráfica ---
# Cuando tengas los datos reales de tu sensor (por ejemplo, en 'real_time_vector' y 'real_flow_vector'),
# podrías añadirlos a esta misma gráfica o crear una segunda subtrama así:
# fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(15, 10), sharex=True) # 2 filas, 1 columna
# ax1.plot(time_vector, target_flow_vector, ...)
# ax1.set_title('Objetivo')
# ax2.plot(real_time_vector, real_flow_vector, ...) # Graficar datos reales
# ax2.set_title('Real')
# plt.show()
# O superponerlos en la misma gráfica:
# ax.plot(real_time_vector, real_flow_vector, label='Flujo Real', color='red', alpha=0.7)
# ax.legend()
# plt.show()